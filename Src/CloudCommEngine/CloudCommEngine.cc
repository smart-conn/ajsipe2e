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

// Delete dependency on Axis2, 20151019, LYH
/*
#include <platforms/axutil_platform_auto_sense.h>
#include <axis2_ims_server.h>
#include <axis2_ims_transport.h>
#include <axis2_ims_transport_sender.h>
#include <axutil_error_default.h>
#include <axutil_log_default.h>
#include <axutil_thread_pool.h>
#include <axutil_types.h>
#include <axiom_xml_reader.h>
#include <axutil_version.h>
#include <axutil_file_handler.h>
#include <axiom.h>
#include <axis2_util.h>
#include <axiom_soap.h>
#include <axis2_client.h>
*/


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

/** Top level message bus object. */
static BusAttachment* s_bus = NULL;
static CommonBusListener* s_busListener = NULL;
static services::AboutPropertyStoreImpl* s_propertyStoreImpl = NULL;
static services::AboutService* s_cceAboutService = NULL;
static CloudCommEngineBusObject* s_cceBusObject = NULL;
class CloudCommEngineAnnounceHandler;
static CloudCommEngineAnnounceHandler* s_announceHandler = NULL;

static ManagedObj<ProxyBusObject> s_pceProxyBusObject;

// Delete dependency on Axis2, 20151019, LYH
/*
static axutil_env_t* s_axis2Env = NULL;
static axis2_transport_receiver_t* s_axis2Server = NULL;
*/

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
                            s_bus->EnableConcurrentCallbacks();
                            SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
                            SessionId sessionId;
                            QStatus status = s_bus->JoinSession(busName, (SessionPort)port, s_busListener, sessionId, opts);
                            if (ER_OK != status) {
                                QCC_LogError(status, ("Error while trying to join the session with remote ProximalCommEngine"));
                                continue;
                            }
                            const char* objPathStr = objPath.c_str();
                            bool isSec = false;
                            ManagedObj<ProxyBusObject> tmp(*s_bus, busName, objPathStr, sessionId, isSec);
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

ThreadReturn STDCALL MessageReceiverThreadFunc(void* arg)
{
    while (1) {
        char* msgBuf = NULL;
        if (IMSTransport::GetInstance()->ReadCloudMessage(&msgBuf) && msgBuf) {
            QStatus status = ER_OK;

            char* peer = msgBuf;
            char* tmp = strchr(msgBuf, '^');
            if (!tmp) {
                // the format is not correct
                QCC_LogError(ER_FAIL, ("The notification message format is not correct"));
                ITReleaseBuf(msgBuf);
                continue;
            }
            *tmp = '\0';
            char* callId = tmp + 1;
            tmp = strchr(callId, '^');
            if (!tmp) {
                // the format is not correct
                QCC_LogError(ER_FAIL, ("The notification message format is not correct"));
                ITSendCloudMessage(0, peer, callId, NULL, "", NULL);
                ITReleaseBuf(msgBuf);
                continue;
            }
            *tmp = '\0';
            char* addr = tmp + 1;
            tmp = strchr(addr, '^');
            if (!tmp) {
                // the format is not correct
                QCC_LogError(ER_FAIL, ("The message format is not correct"));
                ITSendCloudMessage(0, peer, callId, addr, "", NULL);
                ITReleaseBuf(msgBuf);
                continue;
            }
            char* msgContent = tmp + 1;

            MsgArg args[3];
            args[0].Set("s", addr);
            args[2].Set("s", callId);

            StringSource source(msgContent);
            XmlParseContext pc(source);
            status = XmlElement::Parse(pc);
            if (ER_OK != status) {
                QCC_LogError(status, ("Error parsing the message xml content: %s", msgContent));
                ITSendCloudMessage(0, peer, callId, addr, "", NULL);
                ITReleaseBuf(msgBuf);
                continue;
            }
            const XmlElement* rootEle = pc.GetRoot();
            if (!rootEle) {
                // the format is not correct
                QCC_LogError(ER_FAIL, ("The message format is not correct"));
                ITSendCloudMessage(0, peer, callId, addr, "", NULL);
                ITReleaseBuf(msgBuf);
                continue;
            }

            std::vector<MsgArg> argsVec;
            argsVec.clear();
            const std::vector<XmlElement*>& argsEles = rootEle->GetChildren();
            for (size_t argIndx = 0; argIndx < argsEles.size(); argIndx++) {
                MsgArg arg;
                arg.Clear();
                XmlElement* argEle = argsEles[argIndx];
                if (argEle) {
                    if (ER_OK == XmlToArg(argEle, arg)) {
                        argsVec.push_back(arg);
                    }
                }
            }
            MsgArg* argsArray = new MsgArg[argsVec.size()];
            for (size_t argIndx = 0; argIndx < argsVec.size(); argIndx++) {
                argsArray[argIndx] = argsVec[argIndx];
            }
            args[1].Set("av", argsVec.size(), argsArray);

            // Calling the local method
            ajn::Message replyMsg(*s_bus);
            // TBD: Change it to MethodCallAsync or start a new task thread to execute the MethodCall
            status = s_pceProxyBusObject->MethodCall(sipe2e::gateway::gwConsts::SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_INTERFACE.c_str(),
                sipe2e::gateway::gwConsts::SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_LOCALMETHODCALL.c_str(),
                args, 3, replyMsg);

            if (ER_OK != status) {
                QCC_LogError(status, ("Error subscribing cloud service to local"));
                ITSendCloudMessage(0, peer, callId, addr, "", NULL);
                ITReleaseBuf(msgBuf);
                continue;
            }
            
            const MsgArg* replyArgsArray = replyMsg->GetArg(0);
            if (replyArgsArray) {
                MsgArg* outArgsVariant = NULL;
                size_t numOutArgs = 0;
                replyArgsArray->Get("av", &numOutArgs, &outArgsVariant);
                String argsStr = String("<args>\n");
                for (size_t outArgIndx = 0; outArgIndx < numOutArgs; outArgIndx++) {
                    argsStr += ArgToXml(outArgsVariant[outArgIndx].v_variant.val, 0);
                }
                argsStr += String("</args>");

                int itStatus = ITSendCloudMessage(0, peer, callId, addr, argsStr.c_str(), NULL);
                if (0 != itStatus) {
                    QCC_LogError(ER_FAIL, ("Failed to send message to cloud"));
                }
            } else {
                // The reply message is not correct
                // Reply with empty message
                ITSendCloudMessage(0, peer, callId, addr, "", NULL);
            }
            ITReleaseBuf(msgBuf);
        }
    }
    return NULL;
}


ThreadReturn STDCALL NotificationReceiverThreadFunc(void* arg)
{
    while (1) {
        boost::shared_array<char> notification = IMSTransport::GetInstance()->ReadServiceNotification();
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

// Delete the dependency on Axis2, 20151019, LYH
/*
ThreadReturn STDCALL Axis2ServerThreadFunc(void* arg)
{
    axis2_status_t status = axis2_transport_receiver_start(s_axis2Server, s_axis2Env);
    if (status == AXIS2_FAILURE) {
        QCC_LogError(ER_FAIL, ("Error (%u: %s) while trying to start IMS transport server", s_axis2Env->error->error_number, AXIS2_ERROR_GET_MESSAGE(s_axis2Env->error)));
        return NULL;
    }
    return NULL;
}

QStatus initAxi2System(const axutil_log_levels_t logLevel)
{
    // Before initializing any Axis2 moudule, first initialize the IMSTransport
    if (0 != ITInitialize())
    {
        return ER_FAIL;
    }

    axutil_allocator_t *allocator = axutil_allocator_init(NULL);
    axutil_error_t *error = axutil_error_create(allocator);
    axutil_log_t *log = axutil_log_create(allocator, NULL, "axis2ims.log");
    axutil_thread_pool_t *thread_pool = axutil_thread_pool_init(allocator);
    // We need to init the parser in main thread before spawning child
    // threads
    axis2_status_t status = axiom_xml_reader_init();
    if (AXIS2_SUCCESS != status) {
        QCC_LogError(ER_FAIL, ("Error (%u) while initializing axiom xml reader of Axis2", status));
        return ER_FAIL;
    }
    s_axis2Env = axutil_env_create_with_error_log_thread_pool(allocator, error, log, thread_pool);

    if (!s_axis2Env) {
        QCC_LogError(ER_FAIL, ("Error while creating Axis2 system environment"));
        return ER_FAIL;
    }
    status = axutil_error_init();
    if (AXIS2_SUCCESS != status) {
        QCC_LogError(ER_FAIL, ("Error (%u: %s) while initializing error system of Axis2", s_axis2Env->error->error_number, AXIS2_ERROR_GET_MESSAGE(s_axis2Env->error)));
        return ER_FAIL;
    }

    String axisPath(gwConsts::AXIS2_DEPLOY_HOME);
    QStatus qStatus = NormalizePath(axisPath);
    if (ER_OK != qStatus) {
        QCC_LogError(qStatus, ("Error while trying to converting relative path"));
        return qStatus;
    }
    status = axutil_file_handler_access((const axis2_char_t*)axisPath.c_str(), AXIS2_R_OK);
    if (AXIS2_SUCCESS != status) {
        QCC_LogError(ER_FAIL, ("Error (%u: %s) while trying to access the Axis2 server home directory (%s)", s_axis2Env->error->error_number, AXIS2_ERROR_GET_MESSAGE(s_axis2Env->error), gwConsts::AXIS2_DEPLOY_HOME));
        return ER_FAIL;
    }
    // Try to prepare the IMS transport server side
    s_axis2Server = axis2_ims_server_create(s_axis2Env, (const axis2_char_t*)axisPath.c_str());
    if (!s_axis2Server) {
        QCC_LogError(ER_FAIL, ("Error (%u: %s) while creating IMS server", s_axis2Env->error->error_number, AXIS2_ERROR_GET_MESSAGE(s_axis2Env->error)));
        return ER_FAIL;
    }

    // Try to prepare the IMS transport client side

    return ER_OK;
}

QStatus cleanupAxisSystem()
{
    if (s_axis2Server) {
        axis2_transport_receiver_free(s_axis2Server, s_axis2Env);
        s_axis2Server = NULL;
    }
    if (s_axis2Env) {
        axutil_env_free(s_axis2Env);
        s_axis2Env = NULL;
    }
    axiom_xml_reader_cleanup();

    ITShutdown();
    return ER_OK;
}
*/


void cleanup()
{
    if (s_bus && s_busListener) {
        s_bus->UnregisterBusListener(*s_busListener);
        s_bus->UnbindSessionPort(s_busListener->getSessionPort());
    }
/*
    if (s_pceProxyBusObject->IsValid()) {
        ManagedObj<ProxyBusObject> tmp;
        s_pceProxyBusObject = tmp;
    }
*/
    if (s_cceBusObject) {
        if (s_bus) {
            s_bus->UnregisterBusObject(*s_cceBusObject);
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

    // Delete the dependency on Axis2, 20151019, LYH
/*
    cleanupAxisSystem();
*/
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

    status = s_cceAboutService->Register(port);
    if (status != ER_OK) {
        return status;
    }

    return (bus->RegisterBusObject(*s_cceAboutService));
}



int main(int argc, char** argv, char** envArg)
{
    /* Install SIGINT handler */
    signal(SIGINT, SigIntHandler);

    QStatus status = ER_OK;

    start:
    /* Prepare bus attachment */
    s_bus = new BusAttachment(gwConsts::SIPE2E_CLOUDCOMMENGINE_NAME.c_str(), true, 8);
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
    qcc::String app_name = gwConsts::SIPE2E_CLOUDCOMMENGINE_NAME;
    map<String, String> deviceNames;
    deviceNames.insert(pair<String, String>("en", gwConsts::SIPE2E_CLOUDCOMMENGINE_NAME.c_str()));
    deviceNames.insert(pair<String, String>("zh", "云端通讯引擎"));
    services::GuidUtil::GetInstance()->GetDeviceIdString(&device_id);
    services::GuidUtil::GetInstance()->GenerateGUID(&app_id);

    s_propertyStoreImpl = new services::AboutPropertyStoreImpl();
    status = fillPropertyStore(s_propertyStoreImpl, app_id, app_name, device_id, deviceNames, String("en"));
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while filling the property store"));
        cleanup();
        return status;
    }
    status = prepareAboutService(s_bus, s_propertyStoreImpl, s_busListener, gwConsts::SIPE2E_CLOUDCOMMENGINE_SESSION_PORT);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while preparing the about service"));
        cleanup();
        return status;
    }
    s_cceBusObject = new CloudCommEngineBusObject(gwConsts::SIPE2E_CLOUDCOMMENGINE_OBJECTPATH, gwConsts::CLOUD_METHOD_CALL_THREAD_POOL_SIZE);
    status = s_cceBusObject->Init(*s_bus, *s_cceAboutService);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while initializing ProximalCommEngine"));
        cleanup();
        return status;
    }

    /* Register AnnounceHandler */
    s_announceHandler = new CloudCommEngineAnnounceHandler();
    const char* pceIntf = gwConsts::SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_INTERFACE.c_str();
    status = services::AnnouncementRegistrar::RegisterAnnounceHandler(*s_bus, *s_announceHandler, &pceIntf, 1);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while registering AnnounceHandler"));
        cleanup();
        return status;
    }


    status = s_cceAboutService->Announce();
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while announcing"));
        cleanup();
        return status;
    }

    // Start a new thread to handle incoming service call
    Thread messageReceiverThread("MessageReceiverThread", MessageReceiverThreadFunc);

    // Start a new thread to handle incoming service notification (from previous offline subscription)
    Thread notificationReceiverThread("NotificationReceiverThread", NotificationReceiverThreadFunc);
    notificationReceiverThread.Start();

    // Initialize the Axis2 system environment
    // Delete dependency on Axis2, 20151019, LYH
/*
#ifdef _DEBUG
    status = initAxi2System(AXIS2_LOG_LEVEL_TRACE);
#else
    status = initAxi2System(AXIS2_LOG_LEVEL_ERROR);
#endif
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while initializing the Axis system environment"));
        cleanup();
        IMSTransport::GetInstance()->StopReadServiceNotification();
        return status;
    }

    s_cceBusObject->SetAxis2Env(s_axis2Env);

    // Start a new thread to execute the server side environment
    Thread axis2ServerThread("Axis2ServerThread", Axis2ServerThreadFunc);
    axis2ServerThread.Start();
*/
    // Initialize the IMSTransport
    if (0 != ITInitialize())
    {
        return ER_FAIL;
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

    ITShutdown();

    cleanup();
    // Delete dependency on Axis2, 20151019, LYH
/*
    axis2ServerThread.Join();
*/
    IMSTransport::GetInstance()->StopReadServiceNotification();
    notificationReceiverThread.Join();

    return 0;
}