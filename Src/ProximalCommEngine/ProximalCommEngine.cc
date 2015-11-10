/******************************************************************************
 * Copyright (c) 2014-2015, Beijing HengShengDongYang Technology Ltd. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#if defined(QCC_OS_GROUP_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#endif
#include <qcc/platform.h>

#include <alljoyn/about/AboutPropertyStoreImpl.h>
#include <alljoyn/about/AnnouncementRegistrar.h>

#include "Common/GuidUtil.h"

#include <signal.h>

#include "Common/GatewayConstants.h"
#include "Common/GatewayStd.h"
#include "ProximalCommEngine/ProximalCommEngineBusObject.h"
#include "Common/CommonBusListener.h"

#define QCC_MODULE "SIPE2E"

using namespace std;
using namespace qcc;
using namespace ajn;

using namespace sipe2e;
using namespace gateway;

/** Top level message bus object. */
static BusAttachment* s_bus = NULL;
static CommonBusListener* s_busListener = NULL;
static services::AboutPropertyStoreImpl* s_propertyStoreImpl = NULL;
static ProximalCommEngineBusObject* s_pceBusObject = NULL;
class ProximalCommEngineAnnounceHandler;
static ProximalCommEngineAnnounceHandler* s_announceHandler = NULL;

static volatile sig_atomic_t s_interrupt = false;

static volatile sig_atomic_t s_restart = false;

static void SigIntHandler(int sig)
{
    s_interrupt = true;
}

static void daemonDisconnectCB()
{
    s_restart = true;
}

typedef void (*AnnounceHandlerCallback)(qcc::String const& busName, unsigned short port);

class ProximalCommEngineAnnounceHandler : public ajn::services::AnnounceHandler {

public:

    ProximalCommEngineAnnounceHandler(AnnounceHandlerCallback callback = 0)
        : AnnounceHandler(), m_Callback(callback)
    {
    }

    ~ProximalCommEngineAnnounceHandler()
    {
    }


    virtual void Announce(unsigned short version, unsigned short port, const char* busName, const ObjectDescriptions& objectDescs,
        const AboutData& aboutData)
    {
        /* Only receive the announcement from CloudCommEngine module */
        if (gwConsts::SIPE2E_CLOUDCOMMENGINE_SESSION_PORT == port) {
            for (services::AnnounceHandler::ObjectDescriptions::const_iterator itObjDesc = objectDescs.begin();
                itObjDesc != objectDescs.end(); ++itObjDesc) {
                    String objPath = itObjDesc->first;
                    size_t pos = objPath.find_last_of('/');
                    if (String::npos != pos) {
                        if (objPath.substr(pos + 1, objPath.length() - pos - 1) == gwConsts::SIPE2E_CLOUDCOMMENGINE_NAME) {
                            s_bus->EnableConcurrentCallbacks();
                            SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
                            SessionId sessionId;
                            QStatus status = s_bus->JoinSession(busName, (SessionPort)port, s_busListener, sessionId, opts);
                            if (ER_OK != status) {
                                QCC_LogError(status, ("Error while trying to join the session with remote CloudCommEngine"));
                                continue;
                            }
                            const char* objPathStr = objPath.c_str();
                            bool isSec = false;
                            ManagedObj<ProxyBusObject> tmp(*s_bus, busName, objPathStr, sessionId, isSec);
                            status = tmp->IntrospectRemoteObject();
                            if (ER_OK != status) {
                                QCC_LogError(status, ("Error while IntrospectRemoteObject the CloudCommEngineBusObject"));
                                continue;
                            }
                            s_pceBusObject->ResetCloudEngineProxyBusObject(tmp);
//                             s_pceBusObject->GetCloudEngineProxyBusObject().wrap(new ProxyBusObject(*s_bus, busName, objPath.c_str(), sessionId, false));
                        }
                    }
            }
        }
    }
private:

    AnnounceHandlerCallback m_Callback;
};



void cleanup()
{
    if (s_bus && s_busListener) {
        s_bus->UnregisterBusListener(*s_busListener);
        s_bus->UnbindSessionPort(s_busListener->getSessionPort());
    }
    if (s_pceBusObject) {
        if (s_bus) {
            s_bus->UnregisterBusObject(*s_pceBusObject);
        }
        s_pceBusObject->Cleanup();
        delete s_pceBusObject;
        s_pceBusObject = NULL;
    }
    /* Destroying the AboutService must be after deletion of s_pceBusObject where AboutService will unregister the s_pceBusObject */
    services::AboutServiceApi::DestroyInstance();
    if (s_busListener) {
        delete s_busListener;
        s_busListener = NULL;
    }
    if (s_propertyStoreImpl) {
        delete s_propertyStoreImpl;
        s_propertyStoreImpl = NULL;
    }
    if (s_announceHandler) {
        if (s_bus) {
            services::AnnouncementRegistrar::UnRegisterAllAnnounceHandlers(*s_bus);
        }
        delete s_announceHandler;
        s_announceHandler = NULL;
    }
    if (s_bus) {
        delete s_bus;
        s_bus = NULL;
    }
}


QStatus fillPropertyStore(services::AboutPropertyStoreImpl* propertyStore, String const& appIdHex,
                                            String const& appName, String const& deviceId, map<String, String> const& deviceNames,
                                            String const& defaultLanguage)
{
    if (!propertyStore) {
        return ER_BAD_ARG_1;
    }

    QStatus status = ER_OK;

    CHECK_RETURN(propertyStore->setDeviceId(deviceId))
    CHECK_RETURN(propertyStore->setAppId(appIdHex))
    CHECK_RETURN(propertyStore->setAppName(appName))

    std::vector<qcc::String> languages(2);
    languages[0] = "en";
    languages[1] = "zh";
    CHECK_RETURN(propertyStore->setSupportedLangs(languages))
    CHECK_RETURN(propertyStore->setDefaultLang(defaultLanguage))

    CHECK_RETURN(propertyStore->setModelNumber("sipe2e001"))
    CHECK_RETURN(propertyStore->setDateOfManufacture("1/1/2015"))
    CHECK_RETURN(propertyStore->setSoftwareVersion("1.0.0 build 1"))
    CHECK_RETURN(propertyStore->setAjSoftwareVersion(ajn::GetVersion()))
    CHECK_RETURN(propertyStore->setHardwareVersion("1.0.0"))

    map<String, String>::const_iterator iter = deviceNames.find(languages[0]);
    if (iter != deviceNames.end()) {
        CHECK_RETURN(propertyStore->setDeviceName(iter->second.c_str(), languages[0]));
    } else {
        CHECK_RETURN(propertyStore->setDeviceName(gwConsts::SIPE2E_PROXIMALCOMMENGINE_NAME.c_str(), "en"));
    }

    iter = deviceNames.find(languages[1]);
    if (iter != deviceNames.end()) {
        CHECK_RETURN(propertyStore->setDeviceName(iter->second.c_str(), languages[1]));
    } else {
        CHECK_RETURN(propertyStore->setDeviceName("近场通讯引擎", "zh"));
    }

    CHECK_RETURN(propertyStore->setDescription("Proximal Communication Engine", "en"))
    CHECK_RETURN(propertyStore->setDescription("近场通讯引擎", "zh"))

    CHECK_RETURN(propertyStore->setManufacturer("Beijing HengShengDongYang Technology Ltd.", "en"))
    CHECK_RETURN(propertyStore->setManufacturer("北京恒胜东阳科技有限公司", "zh"))

    CHECK_RETURN(propertyStore->setSupportUrl("http://www.nane.cn"))

    return status;
}
QStatus prepareAboutService(BusAttachment* bus, services::AboutPropertyStoreImpl* propertyStore,
                                              CommonBusListener* busListener, uint16_t port)
{
    if (!bus) {
        return ER_BAD_ARG_1;
    }

    if (!propertyStore) {
        return ER_BAD_ARG_2;
    }

    if (!busListener) {
        return ER_BAD_ARG_3;
    }

    services::AboutServiceApi::Init(*bus, *propertyStore);
    services::AboutServiceApi* aboutService = services::AboutServiceApi::getInstance();
    if (!aboutService) {
        return ER_BUS_NOT_ALLOWED;
    }

    busListener->setSessionPort(port);
    bus->RegisterBusListener(*busListener);

    TransportMask transportMask = TRANSPORT_ANY;
    SessionPort sp = port;
    SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, false, SessionOpts::PROXIMITY_ANY, transportMask);

    QStatus status = bus->BindSessionPort(sp, opts, *busListener);
    if (status != ER_OK) {
        return status;
    }

    status = aboutService->Register(port);
    if (status != ER_OK) {
        return status;
    }

    return (bus->RegisterBusObject(*aboutService));
}



int main(int argc, char** argv, char** envArg)
{
    /* Install SIGINT handler */
    signal(SIGINT, SigIntHandler);

    QStatus status = ER_OK;

#if 0
#ifdef _DEBUG
    MsgArg* arg = new MsgArg();
    status = arg->Set("av", 0, NULL);
    MsgArg* args;
    size_t numArgs;
    arg->Get("av", &numArgs, &args);

    args = new MsgArg[2];
    numArgs = 2;
    MsgArg* arg1 = new MsgArg("s", "string1");
    status = args[0].Set("v", arg1);
    MsgArg* arg2 = new MsgArg("s", "string2");
    status = args[1].Set("v", arg2);
    status = arg->Set("av", numArgs, args);
    arg->SetOwnershipFlags(MsgArg::OwnsArgs, true);


    arg->Get("av", &numArgs, &args);
    delete arg;
    delete[] args;
#endif
#endif

    start:
    /* Prepare bus attachment */
    s_bus = new BusAttachment(gwConsts::SIPE2E_PROXIMALCOMMENGINE_NAME.c_str(), true, 8);
    if (!s_bus) {
        status = ER_OUT_OF_MEMORY;
        return status;
    }
    status = s_bus->Start();
    if (ER_OK != status) {
        delete s_bus;
        return status;
    }

    /* Prepare the BusListener */
    s_busListener = new CommonBusListener(s_bus, daemonDisconnectCB);
    s_bus->RegisterBusListener(*s_busListener);

    status = s_bus->Connect();
    if (ER_OK != status) {
        delete s_bus;
        return status;
    }

    /* Prepare About */
    qcc::String device_id, app_id;
    qcc::String app_name = gwConsts::SIPE2E_PROXIMALCOMMENGINE_NAME;
    map<String, String> deviceNames;
    deviceNames.insert(pair<String, String>("en", "ProximalCommEngine"));
    deviceNames.insert(pair<String, String>("zh", "近场通讯引擎"));
    services::GuidUtil::GetInstance()->GetDeviceIdString(&device_id);
    services::GuidUtil::GetInstance()->GenerateGUID(&app_id);

    s_propertyStoreImpl = new services::AboutPropertyStoreImpl();
    status = fillPropertyStore(s_propertyStoreImpl, app_id, app_name, device_id, deviceNames, String("en"));
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while filling the property store"));
        cleanup();
        return status;
    }
    status = prepareAboutService(s_bus, s_propertyStoreImpl, s_busListener, gwConsts::SIPE2E_PROXIMALCOMMENGINE_SESSION_PORT);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while preparing the about service"));
        cleanup();
        return status;
    }
    s_pceBusObject = new ProximalCommEngineBusObject(gwConsts::SIPE2E_PROXIMALCOMMENGINE_OBJECTPATH);
    status = s_pceBusObject->Init(*s_bus);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while preparing proxy context for ProximalCommEngine"));
        cleanup();
        return status;
    }

    /* Register AnnounceHandler */
    s_announceHandler = new ProximalCommEngineAnnounceHandler();
    const char* cceIntf = gwConsts::SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_INTERFACE.c_str();
    status = services::AnnouncementRegistrar::RegisterAnnounceHandler(*s_bus, *s_announceHandler, &cceIntf, 1);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while registering AnnounceHandler"));
        cleanup();
        return status;
    }


    status = services::AboutServiceApi::getInstance()->Announce();
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while announcing"));
        cleanup();
        return status;
    }

    while (s_interrupt == false) {
/*
#ifdef _WIN32
        ::Sleep(100);
#else
        usleep(100 * 1000);
#endif
*/
        qcc::Sleep(100);
    }
    cleanup();
    return 0;
}