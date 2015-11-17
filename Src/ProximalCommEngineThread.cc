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

namespace pcethread {

/** Top level message bus object. */
static BusAttachment* s_pceBus = NULL;
static CommonBusListener* s_pceBusListener = NULL;
static services::AboutPropertyStoreImpl* s_pcePropertyStoreImpl = NULL;
static services::AboutService* s_pceAboutService = NULL;
static ProximalCommEngineBusObject* s_pceBusObject = NULL;
class ProximalCommEngineAnnounceHandler;
static ProximalCommEngineAnnounceHandler* s_pceAnnounceHandler = NULL;

static volatile sig_atomic_t s_pceInterrupt = false;

static volatile sig_atomic_t s_pceRestart = false;

static void SigIntHandler(int sig)
{
    s_pceInterrupt = true;
}

static void daemonDisconnectCB()
{
    s_pceRestart = true;
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
                            s_pceBus->EnableConcurrentCallbacks();
                            SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
                            SessionId sessionId;
                            QStatus status = s_pceBus->JoinSession(busName, (SessionPort)port, s_pceBusListener, sessionId, opts);
                            if (ER_OK != status) {
                                QCC_LogError(status, ("Error while trying to join the session with remote CloudCommEngine"));
                                continue;
                            }
                            const char* objPathStr = objPath.c_str();
                            bool isSec = false;
                            ManagedObj<ProxyBusObject> tmp(*s_pceBus, busName, objPathStr, sessionId, isSec);
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
    if (s_pceBus && s_pceBusListener) {
        s_pceBus->UnregisterBusListener(*s_pceBusListener);
        s_pceBus->UnbindSessionPort(s_pceBusListener->getSessionPort());
    }
    if (s_pceBusObject) {
        if (s_pceBus) {
            s_pceBus->UnregisterBusObject(*s_pceBusObject);
        }
        s_pceBusObject->Cleanup();
        delete s_pceBusObject;
        s_pceBusObject = NULL;
    }
    /* Destroying the AboutService must be after deletion of s_pceBusObject where AboutService will unregister the s_pceBusObject */
    if (s_pceAboutService) {
        delete s_pceAboutService;
        s_pceAboutService = NULL;
    }
    if (s_pceBusListener) {
        delete s_pceBusListener;
        s_pceBusListener = NULL;
    }
    if (s_pcePropertyStoreImpl) {
        delete s_pcePropertyStoreImpl;
        s_pcePropertyStoreImpl = NULL;
    }
    if (s_pceAnnounceHandler) {
        if (s_pceBus) {
            services::AnnouncementRegistrar::UnRegisterAllAnnounceHandlers(*s_pceBus);
        }
        delete s_pceAnnounceHandler;
        s_pceAnnounceHandler = NULL;
    }
    if (s_pceBus) {
        delete s_pceBus;
        s_pceBus = NULL;
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

    s_pceAboutService = new services::AboutService(*bus, *propertyStore);
    if (!s_pceAboutService) {
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

    /*status = */s_pceAboutService->Register(port);
//     if (status != ER_OK) {
//         return status;
//     }

    bus->RegisterBusObject(*s_pceAboutService);
    return status;
}

} // namespace pcethread

using namespace pcethread;

ThreadReturn STDCALL ProximalCommEngineThreadFunc(void* arg)
{
    /* Install SIGINT handler */
    signal(SIGINT, SigIntHandler);

    QStatus status = ER_OK;

    start:
    /* Prepare bus attachment */
    s_pceBus = new BusAttachment(gwConsts::SIPE2E_PROXIMALCOMMENGINE_NAME.c_str(), true, 8);
    if (!s_pceBus) {
        status = ER_OUT_OF_MEMORY;
        return (ThreadReturn)status;
    }
    status = s_pceBus->Start();
    if (ER_OK != status) {
        delete s_pceBus;
        return (ThreadReturn)status;
    }

    /* Prepare the BusListener */
    s_pceBusListener = new CommonBusListener(s_pceBus, daemonDisconnectCB);
    s_pceBus->RegisterBusListener(*s_pceBusListener);

    status = s_pceBus->Connect();
    if (ER_OK != status) {
        delete s_pceBus;
        return (ThreadReturn)status;
    }

    /* Prepare About */
    qcc::String device_id, app_id;
    qcc::String app_name = gwConsts::SIPE2E_PROXIMALCOMMENGINE_NAME;
    map<String, String> deviceNames;
    deviceNames.insert(pair<String, String>("en", "ProximalCommEngine"));
    deviceNames.insert(pair<String, String>("zh", "近场通讯引擎"));
    services::GuidUtil::GetInstance()->GetDeviceIdString(&device_id);
    services::GuidUtil::GetInstance()->GenerateGUID(&app_id);

    s_pcePropertyStoreImpl = new services::AboutPropertyStoreImpl();
    status = fillPropertyStore(s_pcePropertyStoreImpl, app_id, app_name, device_id, deviceNames, String("en"));
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while filling the property store"));
        cleanup();
        return (ThreadReturn)status;
    }
    status = prepareAboutService(s_pceBus, s_pcePropertyStoreImpl, s_pceBusListener, gwConsts::SIPE2E_PROXIMALCOMMENGINE_SESSION_PORT);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while preparing the about service"));
        cleanup();
        return (ThreadReturn)status;
    }
    s_pceBusObject = new ProximalCommEngineBusObject(gwConsts::SIPE2E_PROXIMALCOMMENGINE_OBJECTPATH);
    status = s_pceBusObject->Init(*s_pceBus, *s_pceAboutService);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while preparing proxy context for ProximalCommEngine"));
        cleanup();
        return (ThreadReturn)status;
    }

    /* Register AnnounceHandler */
    s_pceAnnounceHandler = new ProximalCommEngineAnnounceHandler();
    const char* cceIntf = gwConsts::SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_INTERFACE.c_str();
    status = services::AnnouncementRegistrar::RegisterAnnounceHandler(*s_pceBus, *s_pceAnnounceHandler, NULL, 0/*&cceIntf, 1*/);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while registering AnnounceHandler"));
        cleanup();
        return (ThreadReturn)status;
    }


    status = s_pceAboutService->Announce();
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while announcing"));
        cleanup();
        return (ThreadReturn)status;
    }

    while (s_pceInterrupt == false) {
        qcc::Sleep(100);
    }
    cleanup();
    return 0;
}
