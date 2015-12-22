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
#include <qcc/String.h>
#include <qcc/Debug.h>
#include <qcc/Thread.h>
#include <qcc/XmlElement.h>
#include <qcc/StringSource.h>
#include <qcc/StringUtil.h>

#include "Common/CommonUtils.h"

#include <alljoyn/AllJoynStd.h>
#include <alljoyn/about/AboutService.h>
#include <alljoyn/about/AnnouncementRegistrar.h>

#include "CloudCommEngine/CloudCommEngineBusObject.h"
#include "Common/GatewayStd.h"
#include "Common/GatewayConstants.h"
#include "CloudCommEngine/CloudServiceAgentBusObject.h"
#include "CloudCommEngine/ProximalProxyBusObjectWrapper.h"

#include "CloudCommEngine/IMSTransport/IMSTransportExport.h"
#include "CloudCommEngine/IMSTransport/IMSTransportConstants.h"
#include "CloudCommEngine/IMSTransport/pidf.h"

#define QCC_MODULE "SIPE2E"


using namespace ajn;
using namespace qcc;
using namespace std;

namespace sipe2e {
namespace gateway {

using namespace gwConsts;

typedef struct _CloudMethodCallThreadArg
{
    gwConsts::customheader::RPC_MSG_TYPE_ENUM callType;
    MsgArg* inArgs;
    unsigned int inArgsNum;
    String peer;
    String calledAddr;
    CloudServiceAgentBusObject* agent;
    Message msg;
    CloudCommEngineBusObject* owner;
    _CloudMethodCallThreadArg(Message _msg)
        : callType(gwConsts::customheader::RPC_MSG_TYPE_METHOD_CALL),
        inArgs(NULL), inArgsNum(0),
        agent(NULL), msg(_msg),
        owner(NULL)
    {

    }
    ~_CloudMethodCallThreadArg()
    {
        if (inArgs) {
            delete[] inArgs;
            inArgs = NULL;
        }
    }
} CloudMethodCallThreadArg;

typedef struct _LocalMethodCallThreadArg {
    gwConsts::customheader::RPC_MSG_TYPE_ENUM msgType;
    MsgArg* inArgs;
    size_t inArgsNum;
    String addr;
    String cloudSessionId;
    String peer;
    CloudCommEngineBusObject* owner;
    _LocalMethodCallThreadArg()
        : inArgs(NULL), inArgsNum(0), owner(NULL)
    {

    }
    ~_LocalMethodCallThreadArg() 
    {
        if (inArgs) {
            delete[] inArgs;
            inArgs = NULL;
        }
    }
} LocalMethodCallThreadArg;


CloudCommEngineBusObject::CloudCommEngineBusObject(qcc::String const& objectPath, uint32_t threadPoolSize)
    : BusObject(objectPath.c_str()),
    methodCallThreadPool("CloudMethodCallThreadPool", threadPoolSize),
    aboutService(NULL),
    messageReceiverThread("MessageReceiverThread", CloudCommEngineBusObject::MessageReceiverThreadFunc),
    notificationReceiverThread("NotificationReceiverThread", CloudCommEngineBusObject::NotificationReceiverThreadFunc)
{

}

CloudCommEngineBusObject::~CloudCommEngineBusObject()
{

}


CloudCommEngineBusObject::LocalServiceAnnounceHandler::AnnounceHandlerTask::AnnounceHandlerTask(CloudCommEngineBusObject* owner)
    : Runnable(), ownerBusObject(owner)
{

}

CloudCommEngineBusObject::LocalServiceAnnounceHandler::AnnounceHandlerTask::~AnnounceHandlerTask()
{

}

void CloudCommEngineBusObject::LocalServiceAnnounceHandler::AnnounceHandlerTask::SetAnnounceContent(uint16_t version, uint16_t port, const char* busName, const ObjectDescriptions& objectDescs, const AboutData& aboutData)
{
    announceData.version = version;
    announceData.port = port;
    announceData.busName = busName;
    announceData.objectDescs = objectDescs;
    announceData.aboutData = aboutData;
}

void CloudCommEngineBusObject::LocalServiceAnnounceHandler::AnnounceHandlerTask::Run(void)
{
    QStatus status = ER_OK;

    if (!ownerBusObject || !ownerBusObject->proxyContext.bus) {
        status = ER_BUS_NO_TRANSPORTS;
        CHECK_STATUS_AND_LOG("No BusAttachment is available for processing announcements");
    }
    if (!ownerBusObject->proxyContext.bus->IsStarted() || !ownerBusObject->proxyContext.bus->IsConnected()) {
        status = ER_BUS_NOT_CONNECTED;
        CHECK_STATUS_AND_LOG("The BusAttachment is not started or connected");
    }
    /* First Ping the bus to make sure it is alive and responding correctly */
/*
    status = ownerBusObject->proxyContext.bus->Ping(announceData.busName.c_str(), PING_BUS_TIMEOUT);
    CHECK_STATUS_AND_LOG("Pinging the bus of the remote object failed");
*/

    /* Then try to join the session at the given port */
    SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
    SessionId sessionId;
    status = ownerBusObject->proxyContext.bus->JoinSession(announceData.busName.c_str(), (SessionPort)announceData.port, 
        NULL, sessionId, opts);
    CHECK_STATUS_AND_LOG("Joining session failed");

    /* If the session is successfully joined then saved the session ID and create the ProxyBusObjects for all announced Objects */
    String introspectionXml("<service name=\"");
//     String normalizedBusName;
//     IllegalStringToObjPathString(announceData.busName, normalizedBusName);
//     introspectionXml += normalizedBusName;
    introspectionXml += announceData.busName;
    introspectionXml += "\">\n";
    /* Save the announcement data to XML */
    introspectionXml += "<about>\n";
    for (services::AnnounceHandler::AboutData::const_iterator itAboutData = announceData.aboutData.begin();
        itAboutData != announceData.aboutData.end(); ++itAboutData) {
            introspectionXml += "<aboutData key=\"";
            introspectionXml += itAboutData->first;
            introspectionXml += "\" value=\"";
            MsgArg aboutValue = itAboutData->second;
            switch (aboutValue.typeId) {
            case ALLJOYN_STRING:
                {
                    introspectionXml += aboutValue.v_string.str;
                }
                break;
            case ALLJOYN_BYTE_ARRAY:
                {
                    uint8_t* buf = NULL;
                    size_t numElem = 0;
                    aboutValue.Get("ay", &numElem, &buf);
                    if (buf && 0 < numElem) {
                        for (size_t i = 0; i < numElem; i++) {
                            char destBuf[8];
                            sprintf(destBuf, "%X", buf[i]);
                            introspectionXml += (const char*)destBuf;
                        }
                    }
                }
                break;
            default:
                break;
            }
            introspectionXml += "\"/>\n";
    }
    introspectionXml += "</about>\n";

    for (services::AnnounceHandler::ObjectDescriptions::const_iterator itObjDesc = announceData.objectDescs.begin();
        itObjDesc != announceData.objectDescs.end(); ++itObjDesc) {
            const String& objPath = itObjDesc->first;
            // Ignore the About interfaces
            if (objPath == org::alljoyn::About::ObjectPath || objPath == org::alljoyn::Icon::ObjectPath) {
                continue;
            }
            /* Create the ProxyBusObject according to this object description */
            const char* serviceName = announceData.busName.c_str();
            const char* objPathName = objPath.c_str();
            bool secure = false;
            _ProxyBusObject _pbo(*ownerBusObject->proxyContext.bus, serviceName, objPathName, sessionId, secure);
            _ProximalProxyBusObjectWrapper proxyWrapper(_pbo, ownerBusObject->proxyContext.bus, ownerBusObject);
            status = proxyWrapper->IntrospectProxyChildren();
            if (ER_OK != status) {
                QCC_LogError(status, ("Error while introspecting the remote BusObject"));
                continue;
            }
            // We try to look into the bus and get the interface SIPE2E_CLOUDSERVICEAGENT_ALLJOYNENGINE_INTERFACE, if successful,
            // we think it is a CloudServiceAgentBusObject, then we'll disconnect the session and ignore it
            const InterfaceDescription* agentInfc = _pbo->GetInterface(gwConsts::SIPE2E_CLOUDSERVICEAGENT_ALLJOYNENGINE_INTERFACE.c_str());
            if (agentInfc) {
                // this is a CloudServiceAgentBusObject
                ownerBusObject->proxyContext.bus->LeaveSession(sessionId);
                return;
            }

            /* Try to publish the object and its interfaces/services to the cloud */
            /* First we should compose the introspection XML string */
            SessionPort wellKnownPort = SESSION_PORT_ANY;
            introspectionXml += proxyWrapper->GenerateProxyIntrospectionXml(wellKnownPort, true);
            if (wellKnownPort != SESSION_PORT_ANY) {
                status = ownerBusObject->proxyContext.bus->JoinSession(announceData.busName.c_str(), wellKnownPort, NULL, sessionId, opts);
            }

            /* And save this ProxyBusObject and all its children to map for later usage */
            ownerBusObject->SaveProxyBusObject(proxyWrapper);
    }
    introspectionXml += "</service>\n";

    /* Then we call PublishLocalServiceToCloud() to publish local services */
    /* Since this is a separate thread, we can simply use synchronous method call */
#ifndef NDEBUG
    printf("%s", introspectionXml.c_str());
#endif // DEBUG

    ownerBusObject->proxyContext.bus->LeaveSession(sessionId);

    status = ownerBusObject->PublishLocalServiceToCloud(introspectionXml);
    CHECK_STATUS_AND_LOG("Error publishing local service to cloud");

    // Here we may get errors from publishing local services to cloud, in case of which we should
    // cache the announcements from local service objects, and try to publish it later
    // TBD
}


CloudCommEngineBusObject::LocalServiceAnnounceHandler::LocalServiceAnnounceHandler(CloudCommEngineBusObject* owner)
    : services::AnnounceHandler(), ownerBusObject(owner), taskPool("LocalServiceAnnounceHandler", LOCAL_SERVICE_ANNOUNCE_HANDLER_TASK_POOL_SIZE)
{


}

CloudCommEngineBusObject::LocalServiceAnnounceHandler::~LocalServiceAnnounceHandler()
{

}

void CloudCommEngineBusObject::LocalServiceAnnounceHandler::Announce(uint16_t version, uint16_t port, const char* busName, const ObjectDescriptions& objectDescs, const AboutData& aboutData)
{
    /* 
      * Since we'll have to ping the bus, and try to join the session when receiving announcements, and 
      * pinging and joining actions are synchronous calls that may block. So, here we  use ThreadPool
      * to issue a new task to process the announcement and keep the incoming announcements are 
      * not blocked by later processing.
      *
      * Here we may receive the announcements from self (ProximalCommEngine), so we have to
      * filter out and ignore the announcements from self
      */
    if (port == gwConsts::SIPE2E_CLOUDCOMMENGINE_SESSION_PORT)
        return;
    Ptr<AnnounceHandlerTask> task(new AnnounceHandlerTask(ownerBusObject));
    task->SetAnnounceContent(version, port, busName, objectDescs, aboutData);
    taskPool.WaitForAvailableThread();
    taskPool.Execute(task);
}


QStatus CloudCommEngineBusObject::Init(BusAttachment& cloudCommBus, services::AboutService& cloudCommAboutService)
{
    QStatus status = ER_OK;

    /* Prepare the AllJoyn context for all ProxyBusObjects */
    if (proxyContext.bus) {
        Cleanup();
    }

    proxyContext.bus = new BusAttachment((this->GetName() + "ForProxy").c_str(), true);
    if (!proxyContext.bus) {
        Cleanup();
        status = ER_OUT_OF_MEMORY;
        return status;
    }
    status = proxyContext.bus->Start();
    if (ER_OK != status) {
        Cleanup();
        return status;
    }
    status = proxyContext.bus->Connect();
    if (ER_OK != status) {
        Cleanup();
        return status;
    }

    /* Prepare the AnnounceHandler */
    proxyContext.aboutHandler = new LocalServiceAnnounceHandler(this);
    status = services::AnnouncementRegistrar::RegisterAnnounceHandler(*proxyContext.bus, *proxyContext.aboutHandler, NULL, 0);
    if (ER_OK != status) {
        Cleanup();
        return status;
    }

    /* Prepare interfaces to register in the bus */
    InterfaceDescription* intf = NULL;
    status = cloudCommBus.CreateInterface(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_INTERFACE.c_str(), intf, false);
    if (ER_OK != status || !intf) {
        Cleanup();
        return status;
    }
    intf->AddMethod(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_SUBSCRIBE.c_str(),
        "s", NULL, "remoteAccount");
    intf->AddMethod(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_UNSUBSCRIBE.c_str(),
        "s", NULL, "remoteAccount");
    intf->Activate();
    this->AddInterface(*intf, BusObject::ANNOUNCED);
    const MethodEntry methodEntries[] = {
        { intf->GetMember(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_SUBSCRIBE.c_str()), 
        static_cast<MessageReceiver::MethodHandler>(&CloudCommEngineBusObject::AJSubscribe) },
        { intf->GetMember(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_UNSUBSCRIBE.c_str()), 
        static_cast<MessageReceiver::MethodHandler>(&CloudCommEngineBusObject::AJUnsubscribe) }
    };
    status = this->AddMethodHandlers(methodEntries, sizeof(methodEntries) / sizeof(methodEntries[0]));
    if (ER_OK != status) {
        Cleanup();
        return status;
    }

    status = cloudCommBus.RegisterBusObject(*this, false);
    if (ER_OK != status) {
        Cleanup();
        return status;
    }

    /* Prepare the About announcement object descriptions */
    vector<String> intfNames;
    intfNames.push_back(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_INTERFACE);
    status = cloudCommAboutService.AddObjectDescription(SIPE2E_CLOUDCOMMENGINE_OBJECTPATH, intfNames);
    if (ER_OK != status) {
        Cleanup();
        return status;
    }
    aboutService = &cloudCommAboutService;

    messageReceiverThread.Start(this);
    notificationReceiverThread.Start(this);

    return status;
}


QStatus CloudCommEngineBusObject::Cleanup()
{
    QStatus status = ER_OK;

    /* Delete all CloudServiceAgentBusObject */
    for (map<String, CloudServiceAgentBusObject*>::iterator itCSABO = cloudBusObjects.begin();
        itCSABO != cloudBusObjects.end(); itCSABO++) {
            CloudServiceAgentBusObject*& cloudBusObject = itCSABO->second;
            if (cloudBusObject) {
                cloudBusObject->Cleanup(true);
                delete cloudBusObject;
                cloudBusObject = NULL;
            }
    }
    cloudBusObjects.clear();
    proxyBusObjects.clear();
    if (proxyContext.busListener) {
        delete proxyContext.busListener;
        proxyContext.busListener = NULL;
    }
    if (proxyContext.propertyStore) {
        delete proxyContext.propertyStore;
        proxyContext.propertyStore = NULL;
    }
    if (proxyContext.aboutHandler) {
        if (proxyContext.bus) {
            services::AnnouncementRegistrar::UnRegisterAllAnnounceHandlers(*proxyContext.bus);
        }
        delete proxyContext.aboutHandler;
        proxyContext.aboutHandler = NULL;
    }
    if (proxyContext.about) {
        delete proxyContext.about;
        proxyContext.about = NULL;
    }

    /* Delete all ProxyBusObjects and the proxy AllJoynContext */
    if (proxyContext.bus && proxyContext.busListener) {
        proxyContext.bus->UnregisterBusListener(*proxyContext.busListener);
        proxyContext.bus->UnbindSessionPort(proxyContext.busListener->getSessionPort());
    }

    signalHandlersInfo.clear();

    /* Prepare the About announcement object descriptions */
    vector<String> intfNames;
    intfNames.push_back(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_INTERFACE);
    if (aboutService) {
        status = aboutService->RemoveObjectDescription(SIPE2E_CLOUDCOMMENGINE_OBJECTPATH, intfNames);
        aboutService = NULL;
    }

    ITStopReadCloudMessage();
    messageReceiverThread.Join();
    ITStopReadServiceNotification();
    notificationReceiverThread.Join();

    return status;
}


void CloudCommEngineBusObject::AJSubscribe(const ajn::InterfaceDescription::Member* member, ajn::Message& msg)
{
    QStatus status = ER_OK;
    size_t numArgs = 0;
    const MsgArg* args = NULL;
    msg->GetArgs(numArgs, args);
    if (1 != numArgs || !args) {
        status = ER_BAD_ARG_COUNT;
        CHECK_STATUS_AND_REPLY("Error arg number");
    }

    // The first and the only argument is the remote account address, e.g. lyh@nane.cn
    String remoteAccount = args[0].v_string.str;

    int iStatus = ITSubscribe(remoteAccount.c_str());
    if (iStatus == 0) {
        status = MethodReply(msg, ER_OK);
        if (ER_OK != status) {
            QCC_LogError(status, ("Method Reply did not complete successfully"));
        }
    } else {
        status = ER_FAIL;
        CHECK_STATUS_AND_REPLY("Error while subscribing to remote account");
    }
}

void CloudCommEngineBusObject::AJUnsubscribe(const ajn::InterfaceDescription::Member* member, ajn::Message& msg)
{
    QStatus status = ER_OK;
    size_t numArgs = 0;
    const MsgArg* args = NULL;
    msg->GetArgs(numArgs, args);
    if (1 != numArgs || !args) {
        status = ER_BAD_ARG_COUNT;
        CHECK_STATUS_AND_REPLY("Error arg number");
    }

    // The first and the only argument is the remote account address, e.g. lyh@nane.cn
    String remoteAccount = args[0].v_string.str;

    int iStatus = ITUnsubscribe(remoteAccount.c_str());
    if (iStatus == 0) {
        status = MethodReply(msg, ER_OK);
        if (ER_OK != status) {
            QCC_LogError(status, ("Method Reply did not complete successfully"));
        }
    } else {
        status = ER_FAIL;
        CHECK_STATUS_AND_REPLY("Error while unsubscribing to remote account");
    }
}


CloudCommEngineBusObject::CloudMethodCallRunable::CloudMethodCallRunable(CloudMethodCallThreadArg* _arg)
    : arg(_arg)
{

}

CloudCommEngineBusObject::CloudMethodCallRunable::~CloudMethodCallRunable()
{
    if (arg) {
        delete arg;
        arg = NULL;
    }
}

void CloudCommEngineBusObject::CloudMethodCallRunable::Run()
{
    if (!arg) {
        return;
    }
    QStatus status = ER_OK;
    CloudCommEngineBusObject* cceBusObject = arg->owner;
    CloudServiceAgentBusObject* agent = arg->agent;
    if (!cceBusObject || !agent) {
        return;
    }
/*
    String& calledAddr = arg->calledAddr;
    size_t slash = calledAddr.find_first_of('/');
    if (String::npos == slash || slash >= calledAddr.size() - 1) {
        // The called addr format is not correct
        QCC_LogError(ER_FAIL, ("The format of called address is not correct"));
        status = agent->MethodReply(arg->msg, ER_FAIL);
        if (ER_OK != status) {
            QCC_LogError(status, ("Method Reply did not complete successfully"));
        }
        return;
    }
    String normalizedPeer = calledAddr.substr(0, slash);
    String peer;
    ObjPathStringToIllegalString(normalizedPeer, peer);
    String addr = calledAddr.substr(slash + 1, calledAddr.size() - slash - 1);
*/
    String argsStr("<args>\n");
    if (arg->inArgs && arg->inArgsNum > 0) {
        for (unsigned int i = 0; i < arg->inArgsNum; i++) {
            argsStr += ArgToXml(&arg->inArgs[i], 0);
        }
    }
    argsStr +=  "</args>";

    char* resBuf = NULL;
    int itStatus = ITSendCloudMessage(arg->callType, arg->peer.c_str(), NULL, arg->calledAddr.c_str(), argsStr.c_str(), &resBuf);
    if (0 != itStatus) {
        if (resBuf) {
            ITReleaseBuf(resBuf);
        }
        QCC_LogError(ER_FAIL, ("Failed to send message to cloud"));
        status = agent->MethodReply(arg->msg, (QStatus)itStatus);
        if (ER_OK != status) {
            QCC_LogError(status, ("Method Reply did not complete successfully"));
        }
        return;
    }
    if (resBuf) {
        MsgArg cloudReplyArgs[2];

        StringSource source(resBuf);

        XmlParseContext pc(source);
        status = XmlElement::Parse(pc);
        if (ER_OK != status) {
            QCC_LogError(status, ("Error parsing arguments XML string"));
            status = agent->MethodReply(arg->msg, status);
            if (ER_OK != status) {
                QCC_LogError(status, ("Method Reply did not complete successfully"));
            }
            ITReleaseBuf(resBuf);
            return;
        }
        const XmlElement* rootEle = pc.GetRoot();
        if (!rootEle) {
            QCC_LogError(ER_FAIL, ("The message format is not correct"));
            status = agent->MethodReply(arg->msg, ER_FAIL);
            if (ER_OK != status) {
                QCC_LogError(status, ("Method Reply did not complete successfully"));
            }
            ITReleaseBuf(resBuf);
            return;
        }

        const std::vector<XmlElement*>& argsEles = rootEle->GetChildren();
        size_t argsNum = argsEles.size();
        MsgArg* argsArray = NULL;
        if (argsNum > 0) {
            argsArray = new MsgArg[argsNum];
            for (size_t argIndx = 0; argIndx < argsNum; argIndx++) {
                XmlToArg(argsEles[argIndx], argsArray[argIndx]);
            }
        }

        status = agent->MethodReply(arg->msg, argsArray, argsNum);
        if (ER_OK != status) {
            QCC_LogError(status, ("Method Reply did not complete successfully"));
        }
        ITReleaseBuf(resBuf);
    } else {
        // No result received
        status = cceBusObject->MethodReply(arg->msg, ER_OK);
    }
}


CloudCommEngineBusObject::LocalMethodCallRunable::LocalMethodCallRunable(LocalMethodCallThreadArg* _arg)
    : arg(_arg)
{

}

CloudCommEngineBusObject::LocalMethodCallRunable::~LocalMethodCallRunable()
{
    if (arg) {
        delete arg;
        arg = NULL;
    }
}

void CloudCommEngineBusObject::LocalMethodCallRunable::Run(void)
{
    QStatus status = ER_OK;

    if (!arg || !arg->owner) {
        return;
    }

    size_t outArgsNum = 0;
    ajn::MsgArg* outArgsArray = NULL;
    unsigned int localSessionId = 0;

    status = arg->owner->LocalMethodCall(arg->msgType, arg->addr, arg->inArgsNum, arg->inArgs, arg->cloudSessionId, outArgsNum, outArgsArray, localSessionId);

    if (ER_OK != status) {
        QCC_LogError(status, ("Error executing local method call"));
        ITSendCloudMessage(arg->msgType+1, arg->peer.c_str(), arg->cloudSessionId.c_str(), arg->addr.c_str(), "", NULL);
        return;
    }

    if (outArgsArray && outArgsNum > 0) {
        String argsStr = String("<args>\n");
        for (size_t outArgIndx = 0; outArgIndx < outArgsNum; outArgIndx++) {
            argsStr += ArgToXml(&outArgsArray[outArgIndx], 0);
        }
        argsStr += String("</args>");

        int itStatus = ITSendCloudMessage(arg->msgType+1, arg->peer.c_str(), arg->cloudSessionId.c_str(), arg->addr.c_str(), argsStr.c_str(), NULL);
        if (0 != itStatus) {
            QCC_LogError(ER_FAIL, ("Failed to send message to cloud"));
        }
    } else {
        // The reply message is not correct
        // Reply with empty message
        ITSendCloudMessage(arg->msgType+1, arg->peer.c_str(), arg->cloudSessionId.c_str(), arg->addr.c_str(), "", NULL);
    }

}


void CloudCommEngineBusObject::SaveProxyBusObject(_ProximalProxyBusObjectWrapper proxyWrapper)
{
/*
    String normalizedBusName;
    IllegalStringToObjPathString(proxyWrapper->proxy->GetServiceName(), normalizedBusName);
    String busNameAndObjPath = normalizedBusName / *+ "/"* / + proxyWrapper->proxy->GetPath();
*/
    String busNameAndObjPath = proxyWrapper->proxy->GetServiceName() + proxyWrapper->proxy->GetPath();
    proxyBusObjects.insert(pair<String, _ProximalProxyBusObjectWrapper>(busNameAndObjPath, proxyWrapper));
    for (size_t i = 0; i < proxyWrapper->children.size(); i++) {
        _ProximalProxyBusObjectWrapper child = proxyWrapper->children[i];
        SaveProxyBusObject(child);
    }
}


ThreadReturn CloudCommEngineBusObject::MessageReceiverThreadFunc(void* arg)
{
    CloudCommEngineBusObject* cceBusObject = (CloudCommEngineBusObject*)arg;
    char* msgBuf = NULL;
    while (0 == ITReadCloudMessage(&msgBuf)) {
        if (msgBuf) {
            QStatus status = ER_OK;
            // There are maybe two types of incoming MESSAGE, one of which is Method Calls and the other is Signal
            // Here we'll have to deal with these two situations
            char* msgType = msgBuf;
            char* tmp = strchr(msgType, '^');
            if (!tmp) {
                // the format is not correct
                QCC_LogError(ER_FAIL, ("The message format is not correct"));
                ITReleaseBuf(msgBuf);
                continue;
            }
            *tmp = '\0';
            int msgTypeN = atoi(msgType);

            char* peer = tmp + 1;;
            tmp = strchr(peer, '^');
            if (!tmp) {
                // the format is not correct
                QCC_LogError(ER_FAIL, ("The message format is not correct"));
                ITReleaseBuf(msgBuf);
                continue;
            }
            *tmp = '\0';

            if (msgTypeN < gwConsts::customheader::RPC_MSG_TYPE_UPDATE_SIGNAL_HANDLER) {
                // If the request is METHOD CALL or SIGNAL CALL
                char* callId = tmp + 1;
                tmp = strchr(callId, '^');
                if (!tmp) {
                    // the format is not correct
                    QCC_LogError(ER_FAIL, ("The message format is not correct"));
                    ITSendCloudMessage(msgTypeN+1, peer, callId, NULL, "", NULL);
                    ITReleaseBuf(msgBuf);
                    continue;
                }
                *tmp = '\0';
                char* addr = tmp + 1;
                tmp = strchr(addr, '^');
                if (!tmp) {
                    // the format is not correct
                    QCC_LogError(ER_FAIL, ("The message format is not correct"));
                    ITSendCloudMessage(msgTypeN+1, peer, callId, addr, "", NULL);
                    ITReleaseBuf(msgBuf);
                    continue;
                }
                *tmp = '\0';
                char* msgContent = tmp + 1;

                StringSource source(msgContent);
                XmlParseContext pc(source);
                status = XmlElement::Parse(pc);
                if (ER_OK != status) {
                    QCC_LogError(status, ("Error parsing the message xml content: %s", msgContent));
                    ITSendCloudMessage(msgTypeN+1, peer, callId, addr, "", NULL);
                    ITReleaseBuf(msgBuf);
                    continue;
                }
                const XmlElement* rootEle = pc.GetRoot();
                if (!rootEle) {
                    // the format is not correct
                    QCC_LogError(ER_FAIL, ("The message format is not correct"));
                    ITSendCloudMessage(msgTypeN+1, peer, callId, addr, "", NULL);
                    ITReleaseBuf(msgBuf);
                    continue;
                }

                const std::vector<XmlElement*>& argsEles = rootEle->GetChildren();

                switch (msgTypeN) {
                case gwConsts::customheader::RPC_MSG_TYPE_METHOD_CALL:
                case gwConsts::customheader::RPC_MSG_TYPE_PROPERTY_CALL:
                    {
                        LocalMethodCallThreadArg* localMethodCallArg = new LocalMethodCallThreadArg();
                        localMethodCallArg->msgType = (gwConsts::customheader::RPC_MSG_TYPE_ENUM)msgTypeN;

                        localMethodCallArg->inArgsNum = argsEles.size();
                        if (localMethodCallArg->inArgsNum > 0) {
                            localMethodCallArg->inArgs = new MsgArg[localMethodCallArg->inArgsNum];
                            for (size_t argIndx = 0; argIndx < localMethodCallArg->inArgsNum; argIndx++) {
                                XmlElement* argEle = argsEles[argIndx];
                                if (argEle) {
                                    XmlToArg(argEle, localMethodCallArg->inArgs[argIndx]);
                                }
                            }
                        }

                        localMethodCallArg->addr = addr;
                        localMethodCallArg->cloudSessionId = callId;
                        localMethodCallArg->peer = peer;
                        localMethodCallArg->owner = cceBusObject;

                        LocalMethodCallRunable* localMethodCallTask = new LocalMethodCallRunable(localMethodCallArg);
                        status = cceBusObject->methodCallThreadPool.Execute(localMethodCallTask);
                        if (ER_OK != status) {
                            QCC_LogError(status, ("Error executing local method call"));
                            ITSendCloudMessage(msgTypeN+1, peer, callId, addr, "", NULL);
                            ITReleaseBuf(msgBuf);
                            continue;
                        }
                    }
                    break;
                case gwConsts::customheader::RPC_MSG_TYPE_SIGNAL_CALL:
                    {
                        size_t argsNum = argsEles.size();
                        MsgArg* argsArray = NULL;
                        if (argsNum > 0) {
                            argsArray = new MsgArg[argsNum];
                            for (size_t argIndx = 0; argIndx < argsNum; argIndx++) {
                                MsgArg arg;
                                XmlElement* argEle = argsEles[argIndx];
                                if (argEle) {
                                    XmlToArg(argEle, argsArray[argIndx]);
                                }
                            }
                        }

                        String senderReceiverAddr(addr);
                        size_t dot = senderReceiverAddr.find_first_of('`');
                        if (dot == String::npos) {
                            QCC_LogError(ER_FAIL, ("The message format is not correct"));
                            ITReleaseBuf(msgBuf);
                            continue;
                        }
/*
                        String senderAddr(peer);
                        senderAddr += "/";
*/
                        String senderAddr = senderReceiverAddr.substr(0, dot);
                        String receiverAddr = senderReceiverAddr.substr(dot + 1, senderReceiverAddr.size() - dot - 1);

                        // Signaling the local BusObject
                        status = cceBusObject->LocalSignalCall(peer, senderAddr, receiverAddr, argsNum, argsArray, callId);
                        if (argsArray) {
                            delete[] argsArray;
                            argsArray = NULL;
                        }
                        if (ER_OK != status) {
                            QCC_LogError(status, ("Error executing local signal call"));
                            ITReleaseBuf(msgBuf);
                            continue;
                        }
                    }
                    break;
                default:
                    {
                        // the format is not correct
                        QCC_LogError(ER_FAIL, ("The message format is not correct"));
                        ITReleaseBuf(msgBuf);
                        continue;
                    }
                    break;
                }


            } else if (msgTypeN == gwConsts::customheader::RPC_MSG_TYPE_UPDATE_SIGNAL_HANDLER) {
                // If the request type is UPDATE SIGNAL HANDLER
                char* callId = tmp + 1;
                tmp = strchr(callId, '^');
                if (!tmp) {
                    // the format is not correct
                    QCC_LogError(ER_FAIL, ("The message format is not correct"));
                    ITReleaseBuf(msgBuf);
                    continue;
                }
                *tmp = '\0';
                char* addr = tmp + 1;
                tmp = strchr(addr, '^');
                if (!tmp) {
                    // the format is not correct
                    QCC_LogError(ER_FAIL, ("The message format is not correct"));
                    ITReleaseBuf(msgBuf);
                    continue;
                }
                *tmp = '\0';
                char* msgContent = tmp + 1;

                StringSource source(msgContent);
                XmlParseContext pc(source);
                status = XmlElement::Parse(pc);
                if (ER_OK != status) {
                    QCC_LogError(status, ("Error parsing the xml content: %s", msgContent));
                    ITReleaseBuf(msgBuf);
                    continue;
                }
                const XmlElement* rootNode = pc.GetRoot();
                if (rootNode == NULL) {
                    QCC_LogError(ER_XML_MALFORMED, ("Can not get the root node of the xml content: %s", msgContent));
                    ITReleaseBuf(msgBuf);
                    continue;
                }
                if (rootNode->GetName() == "SignalHandlerInfo") {
                    const String& peerBusName = rootNode->GetAttribute("busName");
                    const String& peerSessionId = rootNode->GetAttribute("sessionId");

                // Updating
                    status = cceBusObject->UpdateSignalHandlerInfoToLocal(addr, peer, peerBusName, StringToU32(peerSessionId));
                    if (ER_OK != status) {
                        QCC_LogError(status, ("Error updating signal handler info to local"));
                    }
                } else {
                    // the root node is not SignalHandlerInfo node, just ignore it
                    QCC_DbgPrintf(("The root node is not SignalHandlerInfo."));
                    ITReleaseBuf(msgBuf);
                    continue;
                }

            }
            ITReleaseBuf(msgBuf);

        }
    }
    return NULL;
}


ThreadReturn CloudCommEngineBusObject::NotificationReceiverThreadFunc(void* arg)
{
    CloudCommEngineBusObject* cceBusObject = (CloudCommEngineBusObject*)arg;
    char* msgBuf = NULL;
    while (0 == ITReadServiceNotification(&msgBuf)) {
        if (msgBuf) {
            QStatus status = ER_OK;
            char* tmp = strchr(msgBuf, '^');
            if (!tmp) {
                // the format is not correct
                QCC_LogError(ER_FAIL, ("The notification message format is not correct"));
                continue;
            }
            *tmp = '\0';
            char* peer = msgBuf;
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

            StringSource source(notificationContent);
            XmlParseContext pc(source);
            status = XmlElement::Parse(pc);
            if (ER_OK != status) {
                // Maybe the content is totally empty, in which case all services from the peer will be unsubscribed
                if (nSubState == 0) {
                    // service terminated
                    status = cceBusObject->UnsubscribeCloudServiceFromLocal(peer, String(""));
                    if (ER_OK != status) {
                        QCC_LogError(status, ("Error unsubscribing cloud service from local"));
                    }
                }
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

                    // TBD: in case of duplicate subscriptions, here introspection XML should be stored for later
                    // comparison, if it's exactly the same as some previous subscription, should just ingnore it
                    if (nSubState && _status.GetBasicStatus() == ims::basic::open) {
                        // if the status is open, then subscribe it
                        status = cceBusObject->SubscribeCloudServiceToLocal(peer, serviceIntrospectionXml);
                        if (ER_OK != status) {
                            QCC_LogError(status, ("Error subscribing cloud service to local"));
                        }
                    } else {
                        // if the status is closed, then unsubscribe it
                        status = cceBusObject->UnsubscribeCloudServiceFromLocal(peer, serviceIntrospectionXml);
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
    }
    return NULL;
}

String GetServiceRootPath(const String& serviceXML)
{
    String serviceRootPath("");
    StringSource source(serviceXML);

    XmlParseContext pc(source);
    QStatus status = XmlElement::Parse(pc);
    if (ER_OK == status) {
        const XmlElement* root = pc.GetRoot();
        if (root) {
            if (root->GetName() == "service") {
                serviceRootPath = root->GetAttribute("name");
            }
        }
    }

    return serviceRootPath;
}

QStatus CloudCommEngineBusObject::SubscribeCloudServiceToLocal(const qcc::String& serviceAddr, const qcc::String& serviceIntrospectionXml)
{
    QStatus status = ER_OK;

/*
    qcc::String normalizedServiceAddr;
    IllegalStringToObjPathString(serviceAddr, normalizedServiceAddr);

    String agentObjPath;
    if (normalizedServiceAddr[0] != '/') {
        agentObjPath = "/" + normalizedServiceAddr;
    } else {
        agentObjPath = normalizedServiceAddr;
    }

    String serviceRootPath = GetServiceRootPath(serviceIntrospectionXml);
    if (!serviceRootPath.empty()) {
        if (serviceRootPath[0] != '/') {
            agentObjPath += "/" + serviceRootPath;
        } else {
            agentObjPath += serviceRootPath;
        }
    }
*/

    // ServiceIndex will be like: thierry_luo@nane.cn/BusName
    String serviceIndex = serviceAddr + "/";
    String serviceBusName = GetServiceRootPath(serviceIntrospectionXml);
    serviceIndex += serviceBusName;
    

    /* Check if the cloud service has been subscribed to local already */
    map<String, CloudServiceAgentBusObject*>::iterator cloudBusObjectIt = cloudBusObjects.find(serviceIndex);
    if (cloudBusObjectIt != cloudBusObjects.end()) {
        /* if the cloud service has been subscribed to local, then deleted the service mapping */
        CloudServiceAgentBusObject* oldCloudBusObject = cloudBusObjectIt->second;
        if (oldCloudBusObject) {
            oldCloudBusObject->Cleanup(true);
            delete oldCloudBusObject;
        }
        cloudBusObjects.erase(cloudBusObjectIt);
    }

    /* Create the new Agent BusObject and parse it through the introspection XML string */
    CloudServiceAgentBusObject* agentBusObject = new CloudServiceAgentBusObject(String("/"), serviceAddr, serviceBusName, this);
    services::AboutPropertyStoreImpl* propertyStore = new services::AboutPropertyStoreImpl();
/*
    status = agentBusObject->ParseXml(serviceIntrospectionXml.c_str(), *propertyStore);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while trying to ParseXml to prepare CloudServiceAgentBusObject"));
        agentBusObject->Cleanup(true);
        delete agentBusObject;
        agentBusObject = NULL;
        return status;
    }
*/

    /* Prepare the Agent Service BusObject environment */
    SessionPort sp = SESSION_PORT_ANY;
    status = agentBusObject->PrepareAgent(NULL, propertyStore, sp, serviceIntrospectionXml);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while preparing CloudServiceAgentBusObject"));
        agentBusObject->Cleanup(true);
        delete agentBusObject;
        agentBusObject = NULL;
        return status;
    }
    status = agentBusObject->Announce();
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while announcing the Agent BusObjects"));
        agentBusObject->Cleanup(true);
        delete agentBusObject;
        agentBusObject = NULL;
        return status;
    }
    cloudBusObjects.insert(pair<String, CloudServiceAgentBusObject*>(serviceIndex, agentBusObject));

    return status;
}

QStatus CloudCommEngineBusObject::UnsubscribeCloudServiceFromLocal(const qcc::String& serviceAddr, const qcc::String& serviceIntrospectionXml)
{
    QStatus status = ER_OK;

    qcc::String normalizedServiceAddr;
    IllegalStringToObjPathString(serviceAddr, normalizedServiceAddr);

    String agentObjPath;
    if (normalizedServiceAddr[0] != '/') {
        agentObjPath = "/" + normalizedServiceAddr;
    } else {
        agentObjPath = normalizedServiceAddr;
    }
    if (serviceIntrospectionXml.size() > 0) {
        String serviceRootPath = GetServiceRootPath(serviceIntrospectionXml);
        if (!serviceRootPath.empty()) {
            if (serviceRootPath[0] != '/') {
                agentObjPath += "/" + serviceRootPath;
            } else {
                agentObjPath += serviceRootPath;
            }
        }

        /* Check if the cloud service has been subscribed to local already */
        map<String, CloudServiceAgentBusObject*>::iterator cloudBusObjectIt = cloudBusObjects.find(agentObjPath);
        if (cloudBusObjectIt != cloudBusObjects.end()) {
            /* if the cloud service has been subscribed to local, then deleted the service mapping */
            CloudServiceAgentBusObject* oldCloudBusObject = cloudBusObjectIt->second;
            if (oldCloudBusObject) {
                oldCloudBusObject->Cleanup(true);
                delete oldCloudBusObject;
            }
            cloudBusObjects.erase(cloudBusObjectIt);
        }
    } else {
        // if the serviceIntrospectionXml is empty, which means unsubscribing all services registered previously under the peer address
        vector<map<String, CloudServiceAgentBusObject*>::iterator> vecDelete;
        map<String, CloudServiceAgentBusObject*>::iterator cloudBusObjectIt = cloudBusObjects.begin();
        while (cloudBusObjectIt != cloudBusObjects.end()) {
            if (!cloudBusObjectIt->first.compare(0, normalizedServiceAddr.size(), normalizedServiceAddr)) {
                CloudServiceAgentBusObject* oldCloudBusObject = cloudBusObjectIt->second;
                if (oldCloudBusObject) {
                    oldCloudBusObject->Cleanup(true);
                    delete oldCloudBusObject;
                }
                vecDelete.push_back(cloudBusObjectIt);
            }
            cloudBusObjectIt++;
        }
        for (size_t i = 0; i < vecDelete.size(); i++) {
            cloudBusObjects.erase(vecDelete[i]);
        }
    }

    return status;
}

QStatus CloudCommEngineBusObject::LocalMethodCall(gwConsts::customheader::RPC_MSG_TYPE_ENUM callType, const qcc::String& addr, size_t inArgsNum, const ajn::MsgArg* inArgsArray,
                                                  const qcc::String& cloudSessionId, size_t& outArgsNum, ajn::MsgArg*& outArgsArray, unsigned int& localSessionId)
{
    QStatus status = ER_OK;

    // The first arg is the called address which is combination of BusName/ObjectPath/InterfaceName/MethodName
    String methodAddr(addr);
    while ('/' == methodAddr[methodAddr.length() - 1]) {
        methodAddr.erase(methodAddr.length() - 1, 1);
    }
    size_t off = methodAddr.find_last_of('/');
    if (String::npos == off) {
        status = ER_BAD_ARG_1;
        QCC_LogError(status, ("No method name in the call address"));
        return status;
    }
    String methodName = methodAddr.substr(off + 1, methodAddr.length() - off - 1);
    methodAddr.erase(off, methodAddr.length() - off);
    off = methodAddr.find_last_of('/');
    if (String::npos == off) {
        status = ER_BAD_ARG_1;
        QCC_LogError(status, ("No interface name in the call address"));
        return status;
    }
    String intfName = methodAddr.substr(off + 1, methodAddr.length() - off - 1);
    String busNameAndObjPath = methodAddr.erase(off, methodAddr.length() - off);

    /* Find the ProxyBusObject for this path */
    /**
     *    Find the ProxyBusObject for this path
     * Basically when receiving announcements from local objects, which in most cases are only for top-level object,
     * we should first BusAttachment::JoinSession() and save the session ID, and then create the ProxyBusObject and 
     * IntrospectRemoteObject() to get the whole introspection XML to be published to the cloud. After that, we get
     * all children ProxyBusObjects by calling ProxyBusObject::GetChildren() and save them to proxyBusObjects map.
     *
     * Here we try to find the ProxyBusObject, and hopefully it should be in proxyBusObjects map. If not found, what
     * should we do? Just simply respond with errors, or find some ways to fix it?
     */
    _ProximalProxyBusObjectWrapper proxyWrapper;
    map<String, _ProximalProxyBusObjectWrapper>::const_iterator proxyWrapperIt = proxyBusObjects.find(busNameAndObjPath);

    if (proxyBusObjects.end() == proxyWrapperIt) {
        /* Simply respond with error */
        status = ER_BAD_ARG_1;
        QCC_LogError(status, ("No ProxyBusObject Available for this call"));
        return status;
    } else {
        proxyWrapper = proxyWrapperIt->second;
        if (!proxyWrapper.unwrap()) {
            status = ER_BAD_ARG_1;
            QCC_LogError(status, ("No ProxyBusObject Available for this call"));
            return status;
        }
    }

    switch (callType) {
    case gwConsts::customheader::RPC_MSG_TYPE_METHOD_CALL:
        {
            MsgArg* inArgs = NULL;
            if (inArgsNum > 0 && inArgsArray) {
                inArgs = new MsgArg[inArgsNum];
                for (size_t i = 0; i < inArgsNum; i++) {
                    inArgs[i] = inArgsArray[i];
                }
            }

            // cloudSessionId: not used yet

            // Now call the local method through the ProxyBusObject
            // Here we use synchronous call because we issued a thread every time to process each call
            Message replyMsg(*proxyWrapper->proxyBus);
            status = proxyWrapper->proxy->MethodCall(intfName.c_str(), methodName.c_str(),
                inArgs, inArgsNum, replyMsg);

            if (inArgs) {
                delete[] inArgs;
            }
            if (ER_OK != status) {
                QCC_LogError(status, ("Error making the local method call"));
                return status;
            }

            const MsgArg* outArgsArrayTmp = NULL;
            replyMsg->GetArgs(outArgsNum, outArgsArrayTmp);
            if (outArgsArrayTmp) {
                for (size_t i = 0; i < outArgsNum; i++) {
                    outArgsArray[i] = outArgsArrayTmp[i];
                }
            }

            localSessionId = replyMsg->GetSessionId();
        }
        break;
    case gwConsts::customheader::RPC_MSG_TYPE_PROPERTY_CALL:
        {
            if (methodName == "Get") {
                outArgsArray = new MsgArg(ALLJOYN_VARIANT); // memory leaks, TBD
                outArgsArray->v_variant.val = new MsgArg();
                status = proxyWrapper->proxy->GetProperty(intfName.c_str(), inArgsArray[1].v_string.str, *(outArgsArray->v_variant.val));
                outArgsNum = 1;
            } else if (methodName == "GetAll") {
                outArgsArray = new MsgArg(ALLJOYN_VARIANT); // memory leaks, TBD
                outArgsArray->v_variant.val = new MsgArg();
                status = proxyWrapper->proxy->GetAllProperties(intfName.c_str(), *(outArgsArray->v_variant.val));
                outArgsNum = 1;
            } else if (methodName == "Set") {
                status = proxyWrapper->proxy->SetProperty(intfName.c_str(), inArgsArray[1].v_string.str, *(inArgsArray[2].v_variant.val));
                outArgsNum = 0;
                outArgsArray = NULL;
            }
        }
        break;
    default:
        {
            status = ER_FAIL;
            QCC_LogError(status, ("Wrong CallType, not MethodCall or Property Get/GetAll/Set"));
            return status;
        }
        break;
    }

    return status;
}


QStatus CloudCommEngineBusObject::LocalSignalCall(const qcc::String& peer, const qcc::String& senderAddr, const qcc::String& receiverAddr, 
                                                  size_t inArgsNum, ajn::MsgArg* inArgsArray, const qcc::String& cloudSessionId)
{
    QStatus status = ER_OK;

   // The first arg is the sender address: BusName/ObjectPath/InterfaceName/MemberName
    size_t firstSlash = senderAddr.find_first_of('/');
    if (String::npos == firstSlash) {
        status = ER_FAIL;
        QCC_LogError(status, ("The format of sender address is not correct"));
        return status;
    }
/*
    String senderAccount = senderAddr.substr(0, firstSlash);
    String normalizedSenderAccount;
    IllegalStringToObjPathString(senderAccount, normalizedSenderAccount);

    size_t secondSlash = senderAddr.find_first_of('/', firstSlash + 1);
    if (String::npos == secondSlash) {
        status = ER_FAIL;
        QCC_LogError(status, ("The format of sender address is not correct"));
        return status;
    }
    String rootAgentBusObjectPath = normalizedSenderAccount + senderAddr.substr(firstSlash, secondSlash - firstSlash);
*/
    String rootAgentBusObjectPath = peer + "/" + senderAddr.substr(0, firstSlash);
    std::map<qcc::String, CloudServiceAgentBusObject*>::iterator itCBO = cloudBusObjects.find(rootAgentBusObjectPath);
    if (itCBO == cloudBusObjects.end() || !itCBO->second) {
        status = ER_FAIL;
        QCC_LogError(status, ("The format of sender address is not correct"));
        return status;
    }
    CloudServiceAgentBusObject* rootAgent = itCBO->second;

    firstSlash = senderAddr.find_last_of('/');
    if (String::npos == firstSlash) {
        status = ER_FAIL;
        QCC_LogError(status, ("The format of sender address is not correct"));
        return status;
    }
    String memberName = senderAddr.substr(firstSlash + 1, senderAddr.size() - firstSlash - 1);
    size_t secondSlash = senderAddr.find_last_of('/', firstSlash - 1);
    if (String::npos == secondSlash) {
        status = ER_FAIL;
        QCC_LogError(status, ("The format of sender address is not correct"));
        return status;
    }
    String intfName = senderAddr.substr(secondSlash + 1, firstSlash - secondSlash - 1);
    const InterfaceDescription* signalIntf = rootAgent->GetBusAttachment().GetInterface(intfName.c_str());
    if (!signalIntf) {
        status = ER_BUS_NO_SUCH_INTERFACE;
        QCC_LogError(status, ("Cannot find the interface corresponding to the signal sender"));
        return status;
    }
    const InterfaceDescription::Member* signalMember = signalIntf->GetMember(memberName.c_str());
    if (!signalMember) {
        status = ER_BUS_INTERFACE_NO_SUCH_MEMBER;
        QCC_LogError(status, ("Cannot find the member corresponding to the signal sender"));
        return status;
    }

    firstSlash = receiverAddr.find_first_of('/');
    if (String::npos == firstSlash) {
        status = ER_FAIL;
        QCC_LogError(status, ("The format of receiver address is not correct"));
        return status;
    }
/*
    String normalizedReceiverBusName = receiverAddr.substr(0, firstSlash);
    String receiverBusName;
    ObjPathStringToIllegalString(normalizedReceiverBusName, receiverBusName);
*/
    String receiverBusName = receiverAddr.substr(0, firstSlash);
    SessionId receiverSessionId = StringToU32(receiverAddr.substr(firstSlash + 1, receiverAddr.size() - firstSlash - 1), 10);

    status = rootAgent->Signal(receiverBusName.c_str(), receiverSessionId, *signalMember, inArgsArray, inArgsNum);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error sending out the signal"));
    }

    return status;
}

QStatus CloudCommEngineBusObject::UpdateSignalHandlerInfoToLocal(const qcc::String& localBusNameObjPath, const qcc::String& peerAddr, const qcc::String& peerBusName, unsigned int peerSessionId)
{
    QStatus status = ER_OK;

   // The first arg is the local BusName and ObjPath: BusName/ObjPath (actually it's only BusName)
    String localBusName(localBusNameObjPath);
    while ('/' == localBusName[localBusName.length() - 1]) {
        localBusName.erase(localBusName.length() - 1, 1);
    }

    // Store the SignalHandler Info, with localBusNameObjPath as key and peerBusName/sessionId as value
    std::map<qcc::String, std::map<qcc::String, std::vector<SignalHandlerInfo>>>::iterator itShiMap = signalHandlersInfo.find(localBusName);
    if (itShiMap == signalHandlersInfo.end()) {
        std::map<qcc::String, std::vector<SignalHandlerInfo>> shiMap;
        signalHandlersInfo.insert(std::make_pair(localBusName, shiMap));
    }
    std::map<qcc::String, std::vector<SignalHandlerInfo>>& currShiMap = signalHandlersInfo[localBusName];
    std::map<qcc::String, std::vector<SignalHandlerInfo>>::iterator itSHI = currShiMap.find(peerAddr);
    if (itSHI == currShiMap.end()) {
        std::vector<SignalHandlerInfo> shiVec;
        currShiMap.insert(std::make_pair(peerAddr, shiVec));
    }
    std::vector<SignalHandlerInfo>& currShiVec = currShiMap[peerAddr];

    SignalHandlerInfo peerSignalHandlerInfo = {peerAddr, peerBusName, peerSessionId};
    currShiVec.push_back(peerSignalHandlerInfo);

   return status;
}

QStatus CloudCommEngineBusObject::CloudMethodCall(gwConsts::customheader::RPC_MSG_TYPE_ENUM callType, const qcc::String& peer, const qcc::String& addr, size_t inArgsNum, const ajn::MsgArg* inArgsArray, unsigned int localSessionId, 
                                                  CloudServiceAgentBusObject* agent, ajn::Message msg)
{
    QStatus status = ER_OK;

    // The first arg is the called address
/*
    String calledAddr(addr);
    while ('/' == calledAddr[calledAddr.length() - 1]) {
        calledAddr.erase(calledAddr.length() - 1, 1);
    }
*/

    // Now call the cloud method by sending IMS message out

    // Firstly try to find the stub for this method call, if failed, create one stub for it
    CloudMethodCallThreadArg* argList = new CloudMethodCallThreadArg(msg);
    argList->callType = callType;
    argList->inArgsNum = inArgsNum;
    if (inArgsNum > 0 && inArgsArray) {
        argList->inArgs = new MsgArg[inArgsNum];
        for (size_t i = 0; i < inArgsNum; i++) {
            argList->inArgs[i] = inArgsArray[i];
        }
    }
    argList->peer = peer;
    argList->calledAddr = addr;
    argList->agent = agent;
    argList->owner = this;

    CloudMethodCallRunable* cmcRunable = new CloudMethodCallRunable(argList);
    status = methodCallThreadPool.Execute(cmcRunable);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error running the CloudMethodCall thread maybe because of resource constraint"));
    }
    return status;
}

QStatus CloudCommEngineBusObject::CloudSignalCall(const qcc::String& peer, const qcc::String& senderAddr, const qcc::String& receiverAddr,
                                                  size_t inArgsNum, const ajn::MsgArg* inArgsArray, unsigned int localSessionId)
{
    QStatus status = ER_OK;

/*
    size_t slash = receiverAddr.find_first_of('/');
    if (String::npos == slash) {
        status = ER_FAIL;
        QCC_LogError(status, ("The format of receiver address is not correct"));
        return status;
    }
    String peer = receiverAddr.substr(0, slash);
*/
    String peerAddr(senderAddr);
    peerAddr += "`";
//     peerAddr += receiverAddr.substr(slash + 1, receiverAddr.size() - slash - 1);
    peerAddr += receiverAddr;

    // The third arg is the parameter array for this cloud signal call 
    String argsStr("<args>\n");
    for (size_t i = 0; i < inArgsNum; i++) {
        argsStr += ArgToXml(&inArgsArray[i], 0);
    }
    argsStr +=  "</args>";
    
    int itStatus = ITSendCloudMessage(gwConsts::customheader::RPC_MSG_TYPE_SIGNAL_CALL, peer.c_str(), NULL, peerAddr.c_str(), argsStr.c_str(), NULL);
    if (0 != itStatus) {
        status = ER_FAIL;
        QCC_LogError(status, ("Failed to send message to cloud"));
    }
    return status;
}

QStatus CloudCommEngineBusObject::PublishLocalServiceToCloud(const qcc::String& serviceIntrospectionXml)
{
    QStatus status = ER_OK;

    /**
     * The first and only argument is the introspection XML for the service
     * The root node of the XML string is 'service' node which has a name attribute
     * which will be treated as root path of the service
     */

    int iStatus = ITPublishService(serviceIntrospectionXml.c_str());
    if (iStatus != 0) {
        status = ER_FAIL;
        QCC_LogError(status, ("Error while publishing service to cloud"));
    }
    return status;
}

QStatus CloudCommEngineBusObject::DeleteLocalServiceFromCloud(const qcc::String& serviceIntrospectionXml)
{
    QStatus status = ER_OK;

    /**
     * The first and only argument is the introspection XML for the service
     * The root node of the XML string is 'service' node which has a name attribute
     * which will be treated as root path of the service
     */
    int iStatus = ITDeleteService(serviceIntrospectionXml.c_str());
    if (iStatus != 0) {
        status = ER_FAIL;
        QCC_LogError(status, ("Error while publishing service to cloud"));
    }
    return status;
}

QStatus CloudCommEngineBusObject::UpdateSignalHandlerInfoToCloud(const qcc::String& peerAddr, const qcc::String& peerBusNameObjPath, 
                                                                 const qcc::String& localBusName, unsigned int localSessionId)
{
    QStatus status = ER_OK;

/*
    String peer;
    ObjPathStringToIllegalString(peerAddr, peer);
    // The second arg is the peer BusName and ObjPath: BusName/ObjPath
    String peerBusName(peerBusNameObjPath);
    while ('/' == peerBusName[peerBusName.length() - 1]) {
        peerBusName.erase(peerBusName.length() - 1, 1);
    }
*/

   String msgBuf = "<SignalHandlerInfo busName=\"";
/*
   String normalizedLocalBusName;
   IllegalStringToObjPathString(localBusName, normalizedLocalBusName);
   msgBuf += normalizedLocalBusName;
*/
   msgBuf += localBusName;
   msgBuf += "\"";
   msgBuf += " sessionId=\"";
   msgBuf += qcc::U32ToString(localSessionId, 10);
   msgBuf += "\"></SignalHandlerInfo>";

   int itStatus = ITSendCloudMessage(gwConsts::customheader::RPC_MSG_TYPE_UPDATE_SIGNAL_HANDLER, peerAddr.c_str(), NULL, peerBusNameObjPath.c_str(), msgBuf.c_str(), NULL);
   if (0 != itStatus) {
       status = ER_FAIL;
       QCC_LogError(status, ("Failed to send message to cloud"));
   }
   return status;
}


} // namespace gateway
} // namespace sipe2e