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
#include "CloudCommEngine/IMSTransport/IMSTransport.h"
#include <qcc/Thread.h>
#include <qcc/StringSource.h>
#include <qcc/XmlElement.h>

#include <alljoyn/about/AboutPropertyStoreImpl.h>
#include <alljoyn/about/AnnouncementRegistrar.h>

#include <Common/GuidUtil.h>

#include <signal.h>

#include "Common/GatewayConstants.h"
#include "Common/GatewayStd.h"
#include "Common/CommonBusListener.h"
#include "Common/CommonUtils.h"
#include "CloudCommEngine/IMSTransport/pidf.h"
#include "CloudCommEngine/IMSTransport/IMSTransportExport.h"
#include "CloudCommEngine/CloudCommEngineBusObject.h"

#include <boost/shared_array.hpp>


#define QCC_MODULE "SIPE2E"

using namespace std;
using namespace qcc;
using namespace ajn;

using namespace sipe2e;
using namespace gateway;

namespace ccethread {

/** Top level message bus object. */
static BusAttachment* s_cceBus = NULL;
static CommonBusListener* s_cceBusListener = NULL;
static services::AboutPropertyStoreImpl* s_ccePropertyStoreImpl = NULL;
static services::AboutService* s_cceAboutService = NULL;
static CloudCommEngineBusObject* s_cceBusObject = NULL;
class CloudCommEngineAnnounceHandler;
static CloudCommEngineAnnounceHandler* s_cceAnnounceHandler = NULL;

static ManagedObj<ProxyBusObject> s_pceProxyBusObject;

static volatile sig_atomic_t s_cceInterrupt = false;

static volatile sig_atomic_t s_cceRestart = false;

static void SigIntHandler(int sig)
{
    s_cceInterrupt = true;
}

static void daemonDisconnectCB()
{
    s_cceRestart = true;
}

typedef void (*AnnounceHandlerCallback)(qcc::String const& busName, unsigned short port);

class CloudCommEngineAnnounceHandler : public ajn::services::AnnounceHandler {

public:

    CloudCommEngineAnnounceHandler(AnnounceHandlerCallback callback = 0)
        : AnnounceHandler(), m_Callback(callback)
    {
    }

    ~CloudCommEngineAnnounceHandler()
    {
    }


    virtual void Announce(unsigned short version, unsigned short port, const char* busName, const ObjectDescriptions& objectDescs,
        const AboutData& aboutData)
    {
        /* Only receive the announcement from ProximalCommEngine module */
        if (gwConsts::SIPE2E_PROXIMALCOMMENGINE_SESSION_PORT == port) {
            for (services::AnnounceHandler::ObjectDescriptions::const_iterator itObjDesc = objectDescs.begin();
                itObjDesc != objectDescs.end(); ++itObjDesc) {
                    String objPath = itObjDesc->first;
                    size_t pos = objPath.find_last_of('/');
                    if (String::npos != pos) {
                        if (objPath.substr(pos + 1, objPath.length() - pos - 1) == gwConsts::SIPE2E_PROXIMALCOMMENGINE_NAME) {
                            s_cceBus->EnableConcurrentCallbacks();
                            SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
                            SessionId sessionId;
                            QStatus status = s_cceBus->JoinSession(busName, (SessionPort)port, s_cceBusListener, sessionId, opts);
                            if (ER_OK != status) {
                                QCC_LogError(status, ("Error while trying to join the session with remote ProximalCommEngine"));
                                continue;
                            }
                            const char* objPathStr = objPath.c_str();
                            bool isSec = false;
                            ManagedObj<ProxyBusObject> tmp(*s_cceBus, busName, objPathStr, sessionId, isSec);
                            status = tmp->IntrospectRemoteObject();
                            if (ER_OK != status) {
                                QCC_LogError(status, ("Error while IntrospectRemoteObject the ProximalCommEngineBusObject"));
                                continue;
                            }
                            s_cceBusObject->ResetProximalEngineProxyBusObject(tmp);
                            s_pceProxyBusObject = tmp;
                        }
                    }
            }
        }
    }
private:

    AnnounceHandlerCallback m_Callback;
};

ThreadReturn STDCALL NotificationReceiverThreadFunc(void* arg)
{
    while (boost::shared_array<char> notification = IMSTransport::GetInstance()->ReadServiceNotification()) {
        QStatus status = ER_OK;
        char* notificationBuffer = notification.get();
        char* tmp = strchr(notificationBuffer, '^');
        if (!tmp) {
            // the format is not correct
            QCC_LogError(ER_FAIL, ("The notification message format is not correct"));
            continue;
        }
        *tmp = '\0';
        char* peer = notificationBuffer;
        char* subState = tmp + 1;
        int nSubState = atoi(subState);
        tmp = strchr(subState, '^');
        if (!tmp) {
            // the format is not correct
            QCC_LogError(ER_FAIL, ("The notification message format is not correct"));
            continue;
        }
        *tmp = '\0';
        char* notificationContent = tmp + 1;

/*
        // 
        xmlDocPtr doc = xmlParseMemory(notificationContent, strlen(notificationContent));
        if (doc == NULL) {
            QCC_LogError(ER_XML_MALFORMED, ("Can not parse the notification xml content: %s", notificationContent));
            continue;
        }
        xmlNodePtr rootNode = xmlDocGetRootElement(doc);
        if (rootNode == NULL) {
            QCC_LogError(ER_XML_MALFORMED, ("Can not get the root node of the notification xml content: %s", notificationContent));
            continue;
        }
        if (!xmlStrcmp(rootNode->name, (const xmlChar*)"presence")) {
            // check if the entity property of the node is equal to peer
            xmlChar* entity = xmlGetProp(rootNode, (const xmlChar*)"entity");
            if (entity) {
                char* tmp = strchr((char*)entity, ':');
                char* entityAddr = (char*)entity;
                if (tmp) {
                    entityAddr = tmp + 1;
                } else {
                    // if the entity address does not contains ':', just ignore it
                }
                if (strcmp(entityAddr, peer)) { // Here only for 
                    // if the entity address is not the peer address, then the notification
                    // is not from the correct peer address, just skip this notification
                    QCC_LogError(ER_FAIL, ("The entity address (%s) is not equal to the peer address (%s)", entityAddr, peer));
                    continue;
                }
            } else {
                // if the entity property is not present, just ignore it
            }
*/

        StringSource source(notificationContent);
        XmlParseContext pc(source);
        status = XmlElement::Parse(pc);
        if (ER_OK != status) {
            QCC_LogError(status, ("Error parsing the notification xml content: %s", notificationContent));
            continue;
        }
        const XmlElement* rootNode = pc.GetRoot();
        if (rootNode == NULL) {
            QCC_LogError(ER_XML_MALFORMED, ("Can not get the root node of the notification xml content: %s", notificationContent));
            continue;
        }
        if (rootNode->GetName() == "presence") {
            const String& entity = rootNode->GetAttribute("entity");
            size_t tmp = entity.find_first_of(':');
            String entityAddr;
            if (tmp != String::npos) {
                entityAddr = entity.substr(tmp + 1, entity.size() - tmp - 1);
            } else {
                entityAddr = entity;
            }
            if (entityAddr != (const char*)peer) {
                // if the entity address is not the peer address, then the notification
                // is not from the correct peer address, just skip this notification
                QCC_LogError(ER_FAIL, ("The entity address (%s) is not equal to the peer address (%s)", entityAddr.c_str(), peer));
                continue;
            }

            ims::presence pidfPresence;
            pidfPresence.Deserialize(rootNode);

            // Note that the presence server will probably send all tuples to watchers,
            // so we should iterate all tuples, and check if the basic status is open or closed
            // to determine how to deal with it. If some service's (tuple's) status is closed,
            // then unsubscribe the corresponding service from local environment
            std::vector<ims::tuple>& tuples = pidfPresence.GetTuples();
            for (size_t i = 0; i < tuples.size(); i++) {
                ims::tuple& _tuple = tuples[i];
                String& serviceIntrospectionXml = _tuple.GetService().GetIntrospectionXml();
                ims::status& _status = _tuple.GetStatus();
                if (nSubState && _status.GetBasicStatus() == ims::basic::open) {
                    // if the status is open, then subscribe it
                    MsgArg proximalCallArg("ss", peer, serviceIntrospectionXml.c_str());
                    status = s_pceProxyBusObject->MethodCall(gwConsts::SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_INTERFACE.c_str(),
                        gwConsts::SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_SUBSCRIBE.c_str(),
                        &proximalCallArg, 2);
                    if (ER_OK != status) {
                        QCC_LogError(status, ("Error subscribing cloud service to local"));
                    }
                } else {
                    // if the status is closed, then unsubscribe it
                    MsgArg proximalCallArg("ss", peer, serviceIntrospectionXml.c_str());
                    status = s_pceProxyBusObject->MethodCall(gwConsts::SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_INTERFACE.c_str(),
                        gwConsts::SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_UNSUBSCRIBE.c_str(),
                        &proximalCallArg, 2);
                    if (ER_OK != status) {
                        QCC_LogError(status, ("Error unsubscribing cloud service from local"));
                    }
                }
            }
        } else {
            // the root node is not presence node, just ignore it
            QCC_DbgPrintf(("The root node is not presence."));
            continue;
        }

    }
    return NULL;
}


void cleanup()
{
    if (s_cceBus && s_cceBusListener) {
        s_cceBus->UnregisterBusListener(*s_cceBusListener);
        s_cceBus->UnbindSessionPort(s_cceBusListener->getSessionPort());
    }
/*
    if (s_pceProxyBusObject->IsValid()) {
        ManagedObj<ProxyBusObject> tmp;
        s_pceProxyBusObject = tmp;
    }
*/
    if (s_cceBusObject) {
        if (s_cceBus) {
            s_cceBus->UnregisterBusObject(*s_cceBusObject);
        }
        s_cceBusObject->Cleanup();
        delete s_cceBusObject;
        s_cceBusObject = NULL;
    }
    if (s_pceProxyBusObject->IsValid()) {
        ManagedObj<ProxyBusObject> tmp;
        s_pceProxyBusObject = tmp;
    }
    /* Destroying the AboutService must be after deletion of s_pceBusObject where AboutService will unregister the s_pceBusObject */
    if (s_cceAboutService) {
        delete s_cceAboutService;
        s_cceAboutService = NULL;
    }
    if (s_cceBusListener) {
        delete s_cceBusListener;
        s_cceBusListener = NULL;
    }
    if (s_ccePropertyStoreImpl) {
        delete s_ccePropertyStoreImpl;
        s_ccePropertyStoreImpl = NULL;
    }
    if (s_cceAnnounceHandler) {
        if (s_cceBus) {
            services::AnnouncementRegistrar::UnRegisterAllAnnounceHandlers(*s_cceBus);
        }
        delete s_cceAnnounceHandler;
        s_cceAnnounceHandler = NULL;
    }
    if (s_cceBus) {
        delete s_cceBus;
        s_cceBus = NULL;
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

    CHECK_RETURN(propertyStore->setModelNumber("sipe2e002"))
    CHECK_RETURN(propertyStore->setDateOfManufacture("1/1/2015"))
    CHECK_RETURN(propertyStore->setSoftwareVersion("1.0.0 build 1"))
    CHECK_RETURN(propertyStore->setAjSoftwareVersion(ajn::GetVersion()))
    CHECK_RETURN(propertyStore->setHardwareVersion("1.0.0"))

    map<String, String>::const_iterator iter = deviceNames.find(languages[0]);
    if (iter != deviceNames.end()) {
        CHECK_RETURN(propertyStore->setDeviceName(iter->second.c_str(), languages[0]));
    } else {
        CHECK_RETURN(propertyStore->setDeviceName(gwConsts::SIPE2E_CLOUDCOMMENGINE_NAME.c_str(), "en"));
    }

    iter = deviceNames.find(languages[1]);
    if (iter != deviceNames.end()) {
        CHECK_RETURN(propertyStore->setDeviceName(iter->second.c_str(), languages[1]));
    } else {
        CHECK_RETURN(propertyStore->setDeviceName("云端通讯引擎", "zh"));
    }

    CHECK_RETURN(propertyStore->setDescription("Cloud Communication Engine", "en"))
    CHECK_RETURN(propertyStore->setDescription("云端通讯引擎", "zh"))

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

    s_cceAboutService = new services::AboutService(*bus, *propertyStore);
    if (!s_cceAboutService) {
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

    /*status = */s_cceAboutService->Register(port);
//     if (status != ER_OK) {
//         return status;
//     }

    bus->RegisterBusObject(*s_cceAboutService);
    return status;
}


}

using namespace ccethread;

ThreadReturn STDCALL CloudCommEngineThreadFunc(void* arg)
{
    /* Install SIGINT handler */
    signal(SIGINT, SigIntHandler);

    QStatus status = ER_OK;

    start:
    /* Prepare bus attachment */
    s_cceBus = new BusAttachment(gwConsts::SIPE2E_CLOUDCOMMENGINE_NAME.c_str(), true, 8);
    if (!s_cceBus) {
        status = ER_OUT_OF_MEMORY;
        return (ThreadReturn)status;
    }
    status = s_cceBus->Start();
    if (ER_OK != status) {
        delete s_cceBus;
        return (ThreadReturn)status;
    }

    /* Prepare the BusListener */
    s_cceBusListener = new CommonBusListener(s_cceBus, daemonDisconnectCB);
    s_cceBus->RegisterBusListener(*s_cceBusListener);

    status = s_cceBus->Connect();
    if (ER_OK != status) {
        delete s_cceBus;
        return (ThreadReturn)status;
    }

    /* Prepare About */
    qcc::String device_id, app_id;
    qcc::String app_name = gwConsts::SIPE2E_CLOUDCOMMENGINE_NAME;
    map<String, String> deviceNames;
    deviceNames.insert(pair<String, String>("en", gwConsts::SIPE2E_CLOUDCOMMENGINE_NAME.c_str()));
    deviceNames.insert(pair<String, String>("zh", "云端通讯引擎"));
    services::GuidUtil::GetInstance()->GetDeviceIdString(&device_id);
    services::GuidUtil::GetInstance()->GenerateGUID(&app_id);

    s_ccePropertyStoreImpl = new services::AboutPropertyStoreImpl();
    status = fillPropertyStore(s_ccePropertyStoreImpl, app_id, app_name, device_id, deviceNames, String("en"));
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while filling the property store"));
        cleanup();
        return (ThreadReturn)status;
    }
    status = prepareAboutService(s_cceBus, s_ccePropertyStoreImpl, s_cceBusListener, gwConsts::SIPE2E_CLOUDCOMMENGINE_SESSION_PORT);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while preparing the about service"));
        cleanup();
        return (ThreadReturn)status;
    }
    s_cceBusObject = new CloudCommEngineBusObject(gwConsts::SIPE2E_CLOUDCOMMENGINE_OBJECTPATH, gwConsts::CLOUD_METHOD_CALL_THREAD_POOL_SIZE);
    status = s_cceBusObject->Init(*s_cceBus, *s_cceAboutService);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while initializing ProximalCommEngine"));
        cleanup();
        return (ThreadReturn)status;
    }

    /* Register AnnounceHandler */
    s_cceAnnounceHandler = new CloudCommEngineAnnounceHandler();
    const char* pceIntf = gwConsts::SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_INTERFACE.c_str();
    status = services::AnnouncementRegistrar::RegisterAnnounceHandler(*s_cceBus, *s_cceAnnounceHandler, NULL, 0/*&pceIntf, 1*/);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while registering AnnounceHandler"));
        cleanup();
        return (ThreadReturn)status;
    }


    status = s_cceAboutService->Announce();
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while announcing"));
        cleanup();
        return (ThreadReturn)status;
    }

    // Start a new thread to handle incoming service notification (from previous offline subscription)
    Thread notificationReceiverThread("NotificationReceiverThread", NotificationReceiverThreadFunc);
    notificationReceiverThread.Start();

    // Initialize the IMSTransport
    if (0 != ITInitialize())
    {
        return (ThreadReturn)ER_FAIL;
    }

    while (s_cceInterrupt == false) {
        qcc::Sleep(100);
    }

    ITShutdown();

    cleanup();
    IMSTransport::GetInstance()->StopReadServiceNotification();
    notificationReceiverThread.Join();

    return 0;
}
