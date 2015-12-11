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

#include <assert.h>
#include <vector>
#include <map>
#include <set>

#include <qcc/Debug.h>
#include <qcc/String.h>
#include <qcc/StringSource.h>
#include <qcc/XmlElement.h>
#include <qcc/ThreadPool.h>
#include <qcc/StringUtil.h>

#include <alljoyn/AllJoynStd.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/Status.h>
#include <alljoyn/MsgArg.h>
#include <alljoyn/Message.h>
#include <alljoyn/InterfaceDescription.h>

#include <alljoyn/about/AboutPropertyStoreImpl.h>
#include <alljoyn/about/AboutService.h>
#include <alljoyn/about/AnnouncementRegistrar.h>

// #include <SignatureUtils.h>
// #include <BusUtil.h>

#include "Common/GatewayConstants.h"
#include "Common/CommonUtils.h"
#include "ProximalCommEngine/ProximalCommEngineBusObject.h"
#include "ProximalCommEngine/ProximalProxyBusObjectWrapper.h"

#define QCC_MODULE "SIPE2E"


using namespace ajn;
using namespace qcc;
using namespace std;


namespace sipe2e {
namespace gateway {
using namespace gwConsts;

String GetServiceRootPath(String& serviceXML)
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


ProximalCommEngineBusObject::LocalServiceAnnounceHandler::AnnounceHandlerTask::AnnounceHandlerTask(ProximalCommEngineBusObject* owner)
    : Runnable(), ownerBusObject(owner)
{

}

ProximalCommEngineBusObject::LocalServiceAnnounceHandler::AnnounceHandlerTask::~AnnounceHandlerTask()
{

}

void ProximalCommEngineBusObject::LocalServiceAnnounceHandler::AnnounceHandlerTask::SetAnnounceContent(uint16_t version, uint16_t port, const char* busName, const ObjectDescriptions& objectDescs, const AboutData& aboutData)
{
    announceData.version = version;
    announceData.port = port;
    announceData.busName = busName;
    announceData.objectDescs = objectDescs;
    announceData.aboutData = aboutData;
}

void ProximalCommEngineBusObject::LocalServiceAnnounceHandler::AnnounceHandlerTask::Run(void)
{
    QStatus status = ER_OK;
    /* First we check if ownerBusObject->cloudEngineProxyBusObject is ready, if not, just simply return */
    if (!ownerBusObject->cloudEngineProxyBusObject.unwrap() || !ownerBusObject->cloudEngineProxyBusObject->IsValid()) {
        QCC_LogError(ER_OK, ("The CloudCommEngine ProxyBusObject is not ready"));
        return;
    }
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
        String objPath = itObjDesc->first;
        /* Create the ProxyBusObject according to this object description */
        _ProximalProxyBusObjectWrapper proxyWrapper(_ProxyBusObject::wrap(new ProxyBusObject(*ownerBusObject->proxyContext.bus, 
            announceData.busName.c_str(), objPath.c_str(), sessionId, false)), ownerBusObject->proxyContext.bus, ownerBusObject->cloudEngineProxyBusObject, ownerBusObject);
        status = proxyWrapper->IntrospectProxyChildren();
        if (ER_OK != status) {
            QCC_LogError(status, ("Error while introspecting the remote BusObject"));
            continue;
        }
        /* Try to publish the object and its interfaces/services to the cloud */
        /* First we should compose the introspection XML string */
        introspectionXml += proxyWrapper->GenerateProxyIntrospectionXml();

        /* And save this ProxyBusObject and all its children to map for later usage */
        ownerBusObject->SaveProxyBusObject(proxyWrapper);
    }
    introspectionXml += "</service>\n";

    /* Then we call cloudEngineProxyBusObject::AJPublishLocalServiceToCloud() to publish local services */
    /* Since this is a separate thread, we can simply use synchronous method call */
#ifndef NDEBUG
    printf("%s", introspectionXml.c_str());
#endif // DEBUG

    MsgArg cloudCallArg("s", introspectionXml.c_str());
    status = ownerBusObject->cloudEngineProxyBusObject->MethodCall(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_INTERFACE.c_str(),
        SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_PUBLISH.c_str(),
        &cloudCallArg, 1);
    CHECK_STATUS_AND_LOG("Error publishing local service to cloud");

    // Here we may get errors from publishing local services to cloud, in case of which we should
    // cache the announcements from local service objects, and try to publish it later
    // TBD
}



ProximalCommEngineBusObject::LocalServiceAnnounceHandler::LocalServiceAnnounceHandler(ProximalCommEngineBusObject* parent)
    : services::AnnounceHandler(), ownerBusObject(parent), taskPool("LocalServiceAnnounceHandler", LOCAL_SERVICE_ANNOUNCE_HANDLER_TASK_POOL_SIZE)
{

}

ProximalCommEngineBusObject::LocalServiceAnnounceHandler::~LocalServiceAnnounceHandler()
{

}

void ProximalCommEngineBusObject::LocalServiceAnnounceHandler::Announce(uint16_t version, 
                                                                        uint16_t port, const char* busName, 
                                                                        const ObjectDescriptions& objectDescs, 
                                                                        const AboutData& aboutData)
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
    if (port == gwConsts::SIPE2E_PROXIMALCOMMENGINE_SESSION_PORT
        || port == gwConsts::SIPE2E_CLOUDCOMMENGINE_SESSION_PORT)
        return;
    Ptr<AnnounceHandlerTask> task(new AnnounceHandlerTask(ownerBusObject));
    task->SetAnnounceContent(version, port, busName, objectDescs, aboutData);
    taskPool.WaitForAvailableThread();
    taskPool.Execute(task);
}


ProximalCommEngineBusObject::ProximalCommEngineBusObject(String const& objectPath)
    : BusObject(objectPath.c_str()),
    aboutService(NULL)
{
}


ProximalCommEngineBusObject::~ProximalCommEngineBusObject()
{
}

QStatus ProximalCommEngineBusObject::Init(BusAttachment& proximalCommBus, services::AboutService& proximalCommAboutService)
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
    status = proximalCommBus.CreateInterface(SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_INTERFACE.c_str(), intf, false);
    if (ER_OK != status || !intf) {
        Cleanup();
        return status;
    }
    intf->AddMethod(SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_LOCALMETHODCALL.c_str(),
        "savs", "avx", "addr,para,cloudSessionID,reply,localSessionID");
    intf->AddMethod(SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_LOCALSIGNALCALL.c_str(),
        "ssavs", NULL, "senderAddr,receiverAddr,para,cloudSessionID");
    intf->AddMethod(SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_SUBSCRIBE.c_str(),
        "ss", NULL, "serviceAddr,introspectionXML");
    intf->AddMethod(SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_UNSUBSCRIBE.c_str(),
        "ss", NULL, "serviceAddr,introspectionXML");
    intf->AddMethod(SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_UPDATESIGNALHANDLERINFO.c_str(),
        "sssu", NULL, "localBusNameObjPath,peerAddr,peerBusName,peerSessionId");
    intf->Activate();
    this->AddInterface(*intf, BusObject::ANNOUNCED);
    const MethodEntry methodEntries[] = {
        { intf->GetMember(SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_LOCALMETHODCALL.c_str()), 
        static_cast<MessageReceiver::MethodHandler>(&ProximalCommEngineBusObject::AJLocalMethodCall) },
        { intf->GetMember(SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_LOCALSIGNALCALL.c_str()), 
        static_cast<MessageReceiver::MethodHandler>(&ProximalCommEngineBusObject::AJLocalSignalCall) },
        { intf->GetMember(SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_SUBSCRIBE.c_str()), 
        static_cast<MessageReceiver::MethodHandler>(&ProximalCommEngineBusObject::AJSubscribeCloudServiceToLocal) },
        { intf->GetMember(SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_UNSUBSCRIBE.c_str()), 
        static_cast<MessageReceiver::MethodHandler>(&ProximalCommEngineBusObject::AJUnsubscribeCloudServiceFromLocal) },
        { intf->GetMember(SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_UPDATESIGNALHANDLERINFO.c_str()), 
            static_cast<MessageReceiver::MethodHandler>(&ProximalCommEngineBusObject::AJUpdateSignalHandlerInfoToLocal) },
    };
    status = this->AddMethodHandlers(methodEntries, sizeof(methodEntries) / sizeof(methodEntries[0]));
    if (ER_OK != status) {
        Cleanup();
        return status;
    }

    status = proximalCommBus.RegisterBusObject(*this, false);
    if (ER_OK != status) {
        Cleanup();
        return status;
    }

    /* Prepare the About announcement object descriptions */
    vector<String> intfNames;
    intfNames.push_back(SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_INTERFACE);
    status = proximalCommAboutService.AddObjectDescription(SIPE2E_PROXIMALCOMMENGINE_OBJECTPATH, intfNames);
    if (ER_OK != status) {
        Cleanup();
        return status;
    }
    aboutService = &proximalCommAboutService;

    return status;
}


void ProximalCommEngineBusObject::ResetCloudEngineProxyBusObject(qcc::ManagedObj<ajn::ProxyBusObject> obj)
{
    cloudEngineProxyBusObject = obj;
}


QStatus ProximalCommEngineBusObject::Cleanup()
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

    /* Delete all ProxyBusObjects and the proxy AllJoynContext */
    if (proxyContext.bus && proxyContext.busListener) {
        proxyContext.bus->UnregisterBusListener(*proxyContext.busListener);
        proxyContext.bus->UnbindSessionPort(proxyContext.busListener->getSessionPort());
    }
/*
    for (map<String, ProxyBusObject*>::iterator itPBO = proxyBusObjects.begin();
        itPBO != proxyBusObjects.end(); itPBO++) {
        String objPath = itPBO->first;
        size_t pos = objPath.find_last_of('/');
        while (String::npos != pos) {
            String parentObjPath = objPath.substr(0, pos);
            if (proxyBusObjects.find(parentObjPath) == proxyBusObjects.end()) {
                ProxyBusObject*& topLevelParentPBO = proxyBusObjects[objPath];
                if (topLevelParentPBO) {
                    delete topLevelParentPBO;
                    topLevelParentPBO = NULL;
                }
                break;
            }
            objPath = parentObjPath;
            pos = objPath.find_last_of('/');
        }
    }
*/
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

    /* UnregisterBusObject self will be in the main function of the program */
//     BusObject::GetBusAttachment().UnregisterBusObject(*this);

    /* Delete object descriptions from AboutService */
    vector<String> intfNames;
    intfNames.push_back(SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_INTERFACE);
    if (aboutService) {
        status = aboutService->RemoveObjectDescription(SIPE2E_PROXIMALCOMMENGINE_OBJECTPATH, intfNames);
        aboutService = NULL;
    }

    signalHandlersInfo.clear();

    return status;
}


void ProximalCommEngineBusObject::AJLocalMethodCall(const InterfaceDescription::Member* member, Message& msg)
{
    QStatus status = ER_OK;

    /* Retrieve all arguments of the method call */
    size_t  numArgs = 0;
    const MsgArg* args = 0;
    msg->GetArgs(numArgs, args);
    if (numArgs != 3) {
        status = ER_BAD_ARG_COUNT;
        CHECK_STATUS_AND_REPLY("Error argument count");
    }
    /**
     *    The first arg is the called address which is combination of BusName/ObjectPath/InterfaceName/MethodName
     */
    if (args[0].typeId != ALLJOYN_STRING || args[0].v_string.len <= 1) {
        status = ER_BAD_ARG_1;
        CHECK_STATUS_AND_REPLY("Error call address");
    }
    String addr(args[0].v_string.str);
    while ('/' == addr[addr.length() - 1]) {
        addr.erase(addr.length() - 1, 1);
    }
    size_t off = addr.find_last_of('/');
    if (String::npos == off) {
        status = ER_BAD_ARG_1;
        CHECK_STATUS_AND_REPLY("No method name in the call address");
    }
    String methodName = addr.substr(off + 1, addr.length() - off - 1);
    addr.erase(off, addr.length() - off);
    off = addr.find_last_of('/');
    if (String::npos == off) {
        status = ER_BAD_ARG_1;
        CHECK_STATUS_AND_REPLY("No interface name in the call address");
    }
    String intfName = addr.substr(off + 1, addr.length() - off - 1);
    String busNameAndObjPath = addr.erase(off, addr.length() - off);
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
    _ProximalProxyBusObjectWrapper proxyWrapper = _ProximalProxyBusObjectWrapper::wrap(NULL);
    map<String, _ProximalProxyBusObjectWrapper>::const_iterator proxyWrapperIt = proxyBusObjects.find(busNameAndObjPath);
    if (proxyBusObjects.end() == proxyWrapperIt) {
        /* No ProxyBusObject available, then create it and save it */
/*
        off = busNameAndObjPath.find_first_of('/');
        if (String::npos == off) {
            status = ER_BAD_ARG_1;
            CHECK_STATUS_AND_REPLY("No bus name in the call address");
        }
        String busName = busNameAndObjPath.substr(0, off);
        String objPath = busNameAndObjPath.substr(off + 1, busNameAndObjPath.length() - off - 1);

        proxy = new ProxyBusObject(*bus, busName.c_str(), objPath.c_str(), 0, false); // sessionID and security will be considered in the future
        status = proxy->IntrospectRemoteObject();
        CHECK_STATUS_AND_REPLY("Error while introspecting the remote BusObject");
        proxyBusObjects.insert(pair<String, ProxyBusObject*>(busNameAndObjPath, proxy));
*/
        /* Simply respond with error */
        status = ER_BAD_ARG_1;
        CHECK_STATUS_AND_REPLY("No ProxyBusObject Available for this call");
    } else {
        proxyWrapper = proxyWrapperIt->second;
        if (!proxyWrapper.unwrap()) {
            status = ER_BAD_ARG_1;
            CHECK_STATUS_AND_REPLY("No ProxyBusObject Available for this call");
        }
    }

    /* The second arg is the parameter array for this local method call */
    MsgArg* inArgsVariant = NULL;
    size_t numInArgs = 0;
//     status = MarshalUtils::UnmarshalAllJoynArrayArgs(args[1], inArgs, numInArgs);
    status = args[1].Get("av", &numInArgs, &inArgsVariant);
    CHECK_STATUS_AND_REPLY("Error unmarshaling the array args");
    MsgArg* inArgs = NULL;
    if (numInArgs && inArgsVariant) {
        inArgs = new MsgArg[numInArgs];
        for (size_t i = 0; i < numInArgs; i++) {
            inArgs[i] = *inArgsVariant[i].v_variant.val;
        }
    }

    /* The third arg is the cloud session ID */
    const char* cloudSessionID = NULL;
    status = args[2].Get("s", &cloudSessionID);
    if (ER_OK == status && cloudSessionID) {
        /* Store the cloud session ID, and map it to local session ID */
        // to be continued
    }

    /* Now call the local method through the ProxyBusObject */
    AsyncReplyContext* replyContext = new AsyncReplyContext(msg, member);
    status = proxyWrapper->proxy->MethodCallAsync(intfName.c_str(), methodName.c_str(),
        const_cast<MessageReceiver*>(static_cast<const MessageReceiver* const>(this)), 
        static_cast<MessageReceiver::ReplyHandler>(&ProximalCommEngineBusObject::LocalMethodCallReplyHandler),
        inArgs, numInArgs);
    if (inArgs) {
        delete[] inArgs;
    }
    CHECK_STATUS_AND_REPLY("Error making the local method call");
}

void ProximalCommEngineBusObject::AJLocalSignalCall(const InterfaceDescription::Member* member, Message& msg)
{
    // We'll have to find the local CloudServiceAgentBusObject, and using this to send signals out
    QStatus status = ER_OK;

    /* Retrieve all arguments of the method call */
    size_t  numArgs = 0;
    const MsgArg* args = 0;
    msg->GetArgs(numArgs, args);
    if (numArgs != 4) {
        status = ER_BAD_ARG_COUNT;
        CHECK_STATUS_AND_REPLY("Error argument count");
    }

    // The first arg is the sender address: thierry_luo@nane.cn/BusName/ObjectPath/InterfaceName/MemberName
    String senderAddr(args[0].v_string.str);
    size_t firstSlash = senderAddr.find_first_of('/');
    if (String::npos == firstSlash) {
        status = ER_FAIL;
        CHECK_STATUS_AND_REPLY("The format of sender address is not correct");
    }
    size_t secondSlash = senderAddr.find_first_of('/', firstSlash + 1);
    if (String::npos == secondSlash) {
        status = ER_FAIL;
        CHECK_STATUS_AND_REPLY("The format of sender address is not correct");
    }
    String rootAgentBusObjectPath = senderAddr.substr(0, secondSlash);

    std::map<qcc::String, CloudServiceAgentBusObject*>::iterator itCBO = cloudBusObjects.find(rootAgentBusObjectPath);
    if (itCBO == cloudBusObjects.end() || !itCBO->second) {
        status = ER_FAIL;
        CHECK_STATUS_AND_REPLY("The format of sender address is not correct");
    }
    CloudServiceAgentBusObject* rootAgent = itCBO->second;

    firstSlash = senderAddr.find_last_of('/');
    if (String::npos == firstSlash) {
        status = ER_FAIL;
        CHECK_STATUS_AND_REPLY("The format of sender address is not correct");
    }
    String memberName = senderAddr.substr(firstSlash + 1, senderAddr.size() - firstSlash - 1);
    secondSlash = senderAddr.find_last_of('/', firstSlash - 1);
    if (String::npos == secondSlash) {
        status = ER_FAIL;
        CHECK_STATUS_AND_REPLY("The format of sender address is not correct");
    }
    String intfName = senderAddr.substr(secondSlash + 1, firstSlash - secondSlash - 1);
    const InterfaceDescription* signalIntf = rootAgent->GetBusAttachment().GetInterface(intfName.c_str());
    if (!signalIntf) {
        status = ER_BUS_NO_SUCH_INTERFACE;
        CHECK_STATUS_AND_REPLY("Cannot find the interface corresponding to the signal sender");
    }
    const InterfaceDescription::Member* signalMember = signalIntf->GetMember(memberName.c_str());
    if (!signalMember) {
        status = ER_BUS_INTERFACE_NO_SUCH_MEMBER;
        CHECK_STATUS_AND_REPLY("Cannot find the member corresponding to the signal sender");
    }

    // The second arg is receiver address: BusName/SessionId
    String receiverAddr(args[1].v_string.str);
    firstSlash = receiverAddr.find_first_of('/');
    if (String::npos == firstSlash) {
        status = ER_FAIL;
        CHECK_STATUS_AND_REPLY("The format of receiver address is not correct");
    }
    String receiverBusName = receiverAddr.substr(0, firstSlash);
    SessionId receiverSessionId = StringToU32(receiverAddr.substr(firstSlash + 1, receiverAddr.size() - firstSlash - 1), 10);

    MsgArg* inArgsVariant = NULL;
    size_t numInArgs = 0;
    args[2].Get("av", &numInArgs, &inArgsVariant);
    MsgArg* inArgs = NULL;
    if (numInArgs && inArgsVariant) {
        inArgs = new MsgArg[numInArgs];
        for (size_t i = 0; i < numInArgs; i++) {
            inArgs[i] = *inArgsVariant[i].v_variant.val;
        }
    }

    status = rootAgent->Signal(receiverBusName.c_str(), receiverSessionId, *signalMember, inArgs, numInArgs);
    if (inArgs) {
        delete[] inArgs;
    }
    CHECK_STATUS_AND_REPLY("Error sending out the signal");
}

void ProximalCommEngineBusObject::AJSubscribeCloudServiceToLocal(const InterfaceDescription::Member* member, Message& msg)
{
    QStatus status = ER_OK;

    size_t numArgs = 0;
    const MsgArg* args = NULL;
    msg->GetArgs(numArgs, args);
    if (2 != numArgs || !args) {
        status = ER_BAD_ARG_COUNT;
        CHECK_STATUS_AND_REPLY("Error arg number");
    }
    /**
     *    The first argument is the cloud service address, such as
     * "weather_server_A@nane.cn" or "lyh@nane.cn"
     */
    String serviceAddr = args[0].v_string.str;
    /**
     *    The second argument is the introspection XML for the service
     * The root node of the XML string is 'service' node which has a name attribute
     * which will be treated as root path of the service
     */
    String serviceXML = args[1].v_string.str;

    String agentObjPath;
    if (serviceAddr[0] != '/') {
        agentObjPath = "/" + serviceAddr;
    } else {
        agentObjPath = serviceAddr;
    }
    String serviceRootPath = GetServiceRootPath(serviceXML);
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

    /* Create the new Agent BusObject and parse it through the introspection XML string */
    CloudServiceAgentBusObject* agentBusObject = new CloudServiceAgentBusObject(agentObjPath, cloudEngineProxyBusObject, this);
    services::AboutPropertyStoreImpl* propertyStore = new services::AboutPropertyStoreImpl();
    status = agentBusObject->ParseXml(serviceXML.c_str(), *propertyStore);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while trying to ParseXml to prepare CloudServiceAgentBusObject"));
        status = MethodReply(msg, status);
        if (ER_OK != status) {
            QCC_LogError(status, ("Method Reply did not complete successfully"));
        }
        delete agentBusObject;
        agentBusObject = NULL;
        return;
    }

    /* Prepare the Agent Service BusObject environment */
    status = agentBusObject->PrepareAgent(NULL, propertyStore);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while preparing CloudServiceAgentBusObject"));
        status = MethodReply(msg, status);
        if (ER_OK != status) {
            QCC_LogError(status, ("Method Reply did not complete successfully"));
        }
        delete agentBusObject;
        agentBusObject = NULL;
        return;
    }
    status = agentBusObject->Announce();
    CHECK_STATUS_AND_REPLY("Error while announcing the Agent BusObjects");
    cloudBusObjects.insert(pair<String, CloudServiceAgentBusObject*>(serviceAddr, agentBusObject));

    status = MethodReply(msg, status);
    if (ER_OK != status) {
        QCC_LogError(status, ("Method Reply did not complete successfully"));
/*
        agentBusObject->Cleanup(true);
        delete agentBusObject;
        agentBusObject = NULL;
*/
    }
}

void ProximalCommEngineBusObject::AJUnsubscribeCloudServiceFromLocal(const ajn::InterfaceDescription::Member* member, ajn::Message& msg)
{
    QStatus status = ER_OK;

    size_t numArgs = 0;
    const MsgArg* args = NULL;
    msg->GetArgs(numArgs, args);
    if (2 != numArgs || !args) {
        status = ER_BAD_ARG_COUNT;
        CHECK_STATUS_AND_REPLY("Error arg number");
    }
    /**
     *    The first argument is the cloud service address, such as
     * "weather_server_A@nane.cn" or "lyh@nane.cn"
     */
    String serviceAddr = args[0].v_string.str;
    /**
     *    The second argument is the introspection XML for the service
     * The root node of the XML string is 'service' node which has a name attribute
     * which will be treated as root path of the service
     */
    String serviceXML = args[1].v_string.str;

    String agentObjPath;
    if (serviceAddr[0] != '/') {
        agentObjPath = "/" + serviceAddr;
    } else {
        agentObjPath = serviceAddr;
    }
    String serviceRootPath = GetServiceRootPath(serviceXML);
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
    status = MethodReply(msg, status);
    if (ER_OK != status) {
        QCC_LogError(status, ("Method Reply did not complete successfully"));
    }
}


void ProximalCommEngineBusObject::AJUpdateSignalHandlerInfoToLocal(const ajn::InterfaceDescription::Member* member, ajn::Message& msg)
{
    QStatus status = ER_OK;
     
    // Retrieve all arguments of the method call 
    size_t  numArgs = 0;
    const MsgArg* args = 0;
    msg->GetArgs(numArgs, args);
    if (numArgs != 4) {
        status = ER_BAD_ARG_COUNT;
        CHECK_STATUS_AND_REPLY("Error argument count");
    }

    // The first arg is the local BusName and ObjPath: BusName/ObjPath (actually it's only BusName)
    String localBusNameObjPath(args[0].v_string.str);
    while ('/' == localBusNameObjPath[localBusNameObjPath.length() - 1]) {
        localBusNameObjPath.erase(localBusNameObjPath.length() - 1, 1);
    }

    // The second arg is the peer address
    String peerAddr(args[1].v_string.str);

    // The second arg is the peer BusName
    String peerBusName(args[2].v_string.str);
    // The third arg is the peer session id
    unsigned int peerSessionId = args[3].v_uint32;

    // Store the SignalHandler Info, with localBusNameObjPath as key and peerBusName/sessionId as value
    std::map<qcc::String, std::map<qcc::String, std::vector<SignalHandlerInfo>>>::iterator itShiMap = signalHandlersInfo.find(localBusNameObjPath);
    if (itShiMap == signalHandlersInfo.end()) {
        std::map<qcc::String, std::vector<SignalHandlerInfo>> shiMap;
        signalHandlersInfo.insert(std::make_pair(localBusNameObjPath, shiMap));
    }
    std::map<qcc::String, std::vector<SignalHandlerInfo>>& currShiMap = signalHandlersInfo[localBusNameObjPath];
    std::map<qcc::String, std::vector<SignalHandlerInfo>>::iterator itSHI = currShiMap.find(peerAddr);
    if (itSHI == currShiMap.end()) {
        std::vector<SignalHandlerInfo> shiVec;
        currShiMap.insert(std::make_pair(peerAddr, shiVec));
    }
    std::vector<SignalHandlerInfo>& currShiVec = currShiMap[peerAddr];

    SignalHandlerInfo peerSignalHandlerInfo = {peerAddr, peerBusName, peerSessionId};
    currShiVec.push_back(peerSignalHandlerInfo);
}


void ProximalCommEngineBusObject::SaveProxyBusObject(_ProximalProxyBusObjectWrapper proxyWrapper)
{
    String busNameAndObjPath = proxyWrapper->proxy->GetServiceName() /*+ "/"*/ + proxyWrapper->proxy->GetPath();
    proxyBusObjects.insert(pair<String, _ProximalProxyBusObjectWrapper>(busNameAndObjPath, proxyWrapper));
    for (size_t i = 0; i < proxyWrapper->children.size(); i++) {
        _ProximalProxyBusObjectWrapper child = proxyWrapper->children[i];
        SaveProxyBusObject(child);
    }
}

void ProximalCommEngineBusObject::LocalMethodCallReplyHandler(ajn::Message& msg, void* context)
{
    if (!context) {
        QCC_LogError(ER_BAD_ARG_2, ("No context available in the LocalMethodCall ReplyHandler"));
        return;
    }
    QStatus status = ER_OK;

    AsyncReplyContext* replyContext = reinterpret_cast<AsyncReplyContext*>(context);

    size_t numArgs = 0;
    const MsgArg* args = NULL;
    msg->GetArgs(numArgs, args);
    MsgArg localReplyArgs[2];
    //     status = MarshalUtils::MarshalAllJoynArrayArgs(args, numArgs, cloudReplyArgs[0]);
    status = localReplyArgs[0].Set("av", numArgs, args);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error marshaling the array args"));
        status = MethodReply(replyContext->msg, status);
        if (ER_OK != status) {
            QCC_LogError(status, ("Method Reply did not complete successfully"));
            return;
        }
    }
    /* The second reply arg is the local session ID */
    status = localReplyArgs[1].Set("x", replyContext->msg->GetSessionId());
    if (ER_OK != status) {
        QCC_LogError(status, ("Error setting the arg value"));
        status = MethodReply(replyContext->msg, status);
        if (ER_OK != status) {
            QCC_LogError(status, ("Method Reply did not complete successfully"));
            return;
        }
    }
    /* Reply to the cloud caller */
    status = MethodReply(replyContext->msg, localReplyArgs, 2);
    if (ER_OK != status) {
        QCC_LogError(status, ("Method Reply did not complete successfully"));
        return;
    }
}

/*
QStatus ProximalCommEngineBusObject::IntrospectProxyChildren(ProxyBusObject* proxy)
{
    if (!proxy) {
        QCC_DbgHLPrintf(("Top-level ProxyBusObject is not NULL"));
        return ER_OK;
    }

    QStatus status = proxy->IntrospectRemoteObject();
    if (status != ER_OK) {
        QCC_LogError(status, ("Could not introspect RemoteObject"));
        return status;
    }

    size_t numChildren = proxy->GetChildren();
    if (numChildren == 0) {
        return ER_OK;
    }

    ProxyBusObject** proxyBusObjectChildren = new ProxyBusObject *[numChildren];
    numChildren = proxy->GetChildren(proxyBusObjectChildren, numChildren);

    for (size_t i = 0; i < numChildren; i++) {
#ifndef NDEBUG
        String const& objectPath = proxyBusObjectChildren[i]->GetPath();
        QCC_DbgPrintf(("ObjectPath is: %s", objectPath.c_str()));
#endif

        status = IntrospectProxyChildren(proxyBusObjectChildren[i]);
        if (status != ER_OK) {
            QCC_LogError(status, ("Could not introspect RemoteObjectChild"));
            delete[] proxyBusObjectChildren;
            return status;
        }
    }
    delete[] proxyBusObjectChildren;
    return ER_OK;
}

String ProximalCommEngineBusObject::GenerateProxyIntrospectionXml(ProxyBusObject* proxy, const String& objName)
{
    if (!proxy) {
        QCC_DbgHLPrintf(("Top-level ProxyBusObject is not NULL"));
        return ER_OK;
    }

    String introXml("");
    const String close = "\">\n";

    introXml += "<node name=\"";
    introXml += objName + close;

    / * First we add all interface description * /
    size_t numIntf = proxy->GetInterfaces();
    if (0 < numIntf) {
        const InterfaceDescription** intfs = new const InterfaceDescription*[numIntf];
        if (numIntf == proxy->GetInterfaces(intfs, numIntf)) {
            for (size_t i = 0; i < numIntf; i++) {
                const InterfaceDescription* intf = intfs[i];
                //delete all interface description xml string including the following interfaces:
                //    1. org::freedesktop::DBus::Peer::InterfaceName
                //    2. org::freedesktop::DBus::Properties::InterfaceName
                //    3. org::freedesktop::DBus::Introspectable::InterfaceName
                //    4. org::allseen::Introspectable::InterfaceName
                // because these interfaces will by default be implemented by every BusObject
                const char* intfName = intf->GetName();
                if (intf && intfName 
                    && strcmp(intfName, org::freedesktop::DBus::Peer::InterfaceName)
                    && strcmp(intfName, org::freedesktop::DBus::Properties::InterfaceName)
                    && strcmp(intfName, org::freedesktop::DBus::Introspectable::InterfaceName)
                    && strcmp(intfName, org::allseen::Introspectable::InterfaceName)) {

                        String intfXml = intf->Introspect();

                        StringSource source(intfXml);
                        XmlParseContext pc(source);
                        if (ER_OK == XmlElement::Parse(pc)) {
                            const XmlElement* intfEle = pc.GetRoot();
                            if (intfEle) {
                                std::vector<const XmlElement*> signalEles = intfEle->GetChildren(String("signal"));
                                for (size_t i = 0; i < signalEles.size(); i++) {
                                    const XmlElement* signalEle = signalEles[i];
                                    QStatus status = RegisterCommonSignalHandler(intf, signalEle->GetAttribute("name").c_str());
                                    if (ER_OK != status) {
                                        QCC_LogError(status, ("Could not Register Signal Handler for ProximalServiceProxyBusObject"));
                                    }
                                }
                            }
                        }

                        introXml += intfXml;
                        introXml += "\n";
                }
            }
        }
        delete[] intfs;
    }

    / * Secondly we add all children * /
    size_t numChildren = proxy->GetChildren();
    if (0 < numChildren) {
        ProxyBusObject** children = new ProxyBusObject*[numChildren];
        if (numChildren == proxy->GetChildren(children, numChildren)) {
            for (size_t i = 0; i < numChildren; i++) {
                // This conversion is dangerous!
                String childObjName = children[i]->GetPath();
                size_t pos = childObjName.find_last_of('/');
                if (String::npos != pos) {
                    childObjName = childObjName.substr(pos + 1, childObjName.length() - pos - 1);
                    introXml += GenerateProxyIntrospectionXml(children[i], childObjName);
                }
            }
        }
        delete[] children;
    }

    introXml += "</node>\n";
    return introXml;
}
*/



} //namespace gateway
} //namespace sipe2e