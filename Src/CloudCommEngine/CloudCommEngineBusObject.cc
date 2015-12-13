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

#include <alljoyn/about/AboutService.h>

#include "CloudCommEngine/CloudCommEngineBusObject.h"
#include "Common/GatewayStd.h"
#include "Common/GatewayConstants.h"

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
    MsgArg* inArgs;
    unsigned int inArgsNum;
    String calledAddr;
    AsyncReplyContext* replyContext;
    CloudCommEngineBusObject* cceBusObject;
    _CloudMethodCallThreadArg()
        inArgs(NULL), inArgsNum(0),
        replyContext(NULL), cceBusObject(NULL)
    {

    }
    ~_CloudMethodCallThreadArg()
    {
        if (inArgs) {
            delete[] inArgs;
            inArgs = NULL;
        }
        if (replyContext) {
            delete replyContext;
        }
    }
} CloudMethodCallThreadArg;


CloudCommEngineBusObject::CloudCommEngineBusObject(qcc::String const& objectPath, uint32_t threadPoolSize)
    : BusObject(objectPath.c_str()),
    , cloudMethodCallThreadPool("CloudMethodCallThreadPool", threadPoolSize)
    , aboutService(NULL),
    messageReceiverThread("MessageReceiverThread", CloudCommEngineBusObject::MessageReceiverThreadFunc),
    notificationReceiverThread("NotificationReceiverThread", CloudCommEngineBusObject::NotificationReceiverThreadFunc)
{

}

CloudCommEngineBusObject::~CloudCommEngineBusObject()
{

}

QStatus CloudCommEngineBusObject::Init(BusAttachment& cloudCommBus, services::AboutService& cloudCommAboutService)
{
    QStatus status = ER_OK;

    /* Prepare interfaces to register in the bus */
    InterfaceDescription* intf = NULL;
    status = cloudCommBus.CreateInterface(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_INTERFACE.c_str(), intf, false);
    if (ER_OK != status || !intf) {
        Cleanup();
        return status;
    }
    intf->AddMethod(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_CLOUDMETHODCALL.c_str(),
        "savx", "avs", "addr,para,localSessionID,reply,cloudSessionID");
    intf->AddMethod(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_CLOUDSIGNALCALL.c_str(),
        "ssavx", NULL, "senderAddr,receiverAddr,para,localSessionID");
    intf->AddMethod(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_PUBLISH.c_str(),
        "s", NULL, "introspectionXML");
    intf->AddMethod(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_DELETE.c_str(),
        "s", NULL, "introspectionXML");
    intf->AddMethod(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_UPDATESIGNALHANDLERINFO.c_str(),
        "sssu", NULL, "peerAddr,peerBusNameObjPath,localBusName,localSessionId");
    intf->Activate();
    this->AddInterface(*intf, BusObject::ANNOUNCED);
    const MethodEntry methodEntries[] = {
        { intf->GetMember(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_CLOUDMETHODCALL.c_str()), 
        static_cast<MessageReceiver::MethodHandler>(&CloudCommEngineBusObject::AJCloudMethodCall) },
        { intf->GetMember(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_CLOUDSIGNALCALL.c_str()), 
        static_cast<MessageReceiver::MethodHandler>(&CloudCommEngineBusObject::AJCloudSignalCall) },
        { intf->GetMember(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_PUBLISH.c_str()), 
        static_cast<MessageReceiver::MethodHandler>(&CloudCommEngineBusObject::AJPublishLocalServiceToCloud) },
        { intf->GetMember(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_DELETE.c_str()), 
        static_cast<MessageReceiver::MethodHandler>(&CloudCommEngineBusObject::AJDeleteLocalServiceFromCloud) },
        { intf->GetMember(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_UPDATESIGNALHANDLERINFO.c_str()), 
        static_cast<MessageReceiver::MethodHandler>(&CloudCommEngineBusObject::AJUpdateSignalHandlerInfoToCloud) }
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


void CloudCommEngineBusObject::ResetProximalEngineProxyBusObject(qcc::ManagedObj<ProxyBusObject> obj)
{
    proximalEngineProxyBusObject = obj;
}

QStatus CloudCommEngineBusObject::Cleanup()
{
    QStatus status = ER_OK;

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

void CloudCommEngineBusObject::AJCloudMethodCall(const InterfaceDescription::Member* member, Message& msg)
{
    QStatus status = ER_OK;

    // Retrieve all arguments of the method call 
    size_t  numArgs = 0;
    const MsgArg* args = 0;
    msg->GetArgs(numArgs, args);
    if (numArgs != 3) {
        status = ER_BAD_ARG_COUNT;
        CHECK_STATUS_AND_REPLY("Error argument count");
    }

    //    The first arg is the called address

    if (args[0].typeId != ALLJOYN_STRING || args[0].v_string.len <= 1) {
        status = ER_BAD_ARG_1;
        CHECK_STATUS_AND_REPLY("Error call address");
    }
    String addr(args[0].v_string.str);
    while ('/' == addr[addr.length() - 1]) {
        addr.erase(addr.length() - 1, 1);
    }
    //
    // Up to now we know nothing about the interface we are calling into, so we have to first find out
    // enough information about the interface, by asking the module 'ServiceInfo' for help
    // What actually happens is that below we'll ask ServiceInfo for the details of the methods,
    // including parameters names & types, and return type. All types are denoted with AllJoyn type system
    //
    // Delete the dependency on ServiceInfo, 20151021, LYH
/*
    char* inArgsSignature = NULL;
    char* outArgsSignature = NULL;
    unsigned int inArgsNum = 0;
    unsigned int outArgsNum = 0;
    if (0 != SIGetMethodInfo(addr.c_str(), 
        &inArgsSignature, &inArgsNum,
        &outArgsSignature, &outArgsNum)) {
        status = ER_FAIL;
        CHECK_STATUS_AND_REPLY("Error getting method information");
    }
*/

    // The second arg is the parameter array for this cloud method call 
    MsgArg* inArgsVariant = NULL;
    size_t numInArgs = 0;
    status = args[1].Get("av", &numInArgs, &inArgsVariant);
    CHECK_STATUS_AND_REPLY("Error unmarshaling the array args");
    MsgArg* inArgs = NULL;
    if (numInArgs && inArgsVariant) {
        inArgs = new MsgArg[numInArgs];
        for (size_t i = 0; i < numInArgs; i++) {
            inArgs[i] = *inArgsVariant[i].v_variant.val;
        }
    }

    // The third arg is the cloud session ID 
    long long localSessionID = 0;
    status = args[2].Get("x", &localSessionID);
    if (ER_OK == status) {
        // Store the local session ID, and map it to cloud session ID 
        // to be continued
    }

    // Now call the cloud method by sending IMS message out

    // Firstly try to find the stub for this method call, if failed, create one stub for it
    CloudMethodCallThreadArg* argList = new CloudMethodCallThreadArg();
    argList->inArgs = inArgs;
    argList->inArgsNum = numInArgs;
    argList->calledAddr = addr;
    argList->cceBusObject = this;
    argList->replyContext = new AsyncReplyContext(msg, member);
    CloudMethodCallRunable* cmcRunable = new CloudMethodCallRunable(argList);
    status = cloudMethodCallThreadPool.Execute(cmcRunable);
    CHECK_STATUS_AND_REPLY("Error running the CloudMethodCall thread maybe because of resource constraint");
}

void CloudCommEngineBusObject::AJCloudSignalCall(const InterfaceDescription::Member* member, Message& msg)
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

    // The first arg is the sender address
    String senderAddr(args[0].v_string.str);
    // The second arg is the receiver address
    String receiverAddr(args[1].v_string.str);
    size_t slash = receiverAddr.find_first_of('/');
    if (String::npos == slash) {
        status = ER_FAIL;
        CHECK_STATUS_AND_REPLY("The format of receiver address is not correct");
    }
    String peer = receiverAddr.substr(0, slash);
    String peerAddr(senderAddr);
    peerAddr += "`";
    peerAddr += receiverAddr.substr(slash + 1, receiverAddr.size() - slash - 1);

    // The third arg is the parameter array for this cloud signal call 
    MsgArg* inArgsVariant = NULL;
    size_t numInArgs = 0;
    args[1].Get("av", &numInArgs, &inArgsVariant);
    String argsStr("<args>\n");
    for (size_t i = 0; i < numInArgs; i++) {
        argsStr += ArgToXml(inArgsVariant[i].v_variant.val, 0);
    }
    argsStr +=  "</args>";
    
    int itStatus = ITSendCloudMessage(gwConsts::customheader::RPC_MSG_TYPE_SIGNAL_CALL, peer.c_str(), NULL, peerAddr.c_str(), argsStr.c_str(), NULL);
    if (0 != itStatus) {
        status = ER_FAIL;
        CHECK_STATUS_AND_REPLY("Failed to send message to cloud");
    }
}

void CloudCommEngineBusObject::AJPublishLocalServiceToCloud(const InterfaceDescription::Member* member, Message& msg)
{
    QStatus status = ER_OK;
    size_t numArgs = 0;
    const MsgArg* args = NULL;
    msg->GetArgs(numArgs, args);
    if (1 != numArgs || !args) {
        status = ER_BAD_ARG_COUNT;
        CHECK_STATUS_AND_REPLY("Error arg number");
    }

    /**
     * The first and only argument is the introspection XML for the service
     * The root node of the XML string is 'service' node which has a name attribute
     * which will be treated as root path of the service
     */
    String serviceXML = args[0].v_string.str;

    int iStatus = ITPublishService(serviceXML.c_str());
    if (iStatus == 0) {
        status = MethodReply(msg, ER_OK);
        if (ER_OK != status) {
            QCC_LogError(status, ("Method Reply did not complete successfully"));
        }
    } else {
        status = ER_FAIL;
        CHECK_STATUS_AND_REPLY("Error while publishing service to cloud");
    }
}

void CloudCommEngineBusObject::AJDeleteLocalServiceFromCloud(const InterfaceDescription::Member* member, Message& msg)
{
    QStatus status = ER_OK;
    size_t numArgs = 0;
    const MsgArg* args = NULL;
    msg->GetArgs(numArgs, args);
    if (1 != numArgs || !args) {
        status = ER_BAD_ARG_COUNT;
        CHECK_STATUS_AND_REPLY("Error arg number");
    }

    /**
     * The first and only argument is the introspection XML for the service
     * The root node of the XML string is 'service' node which has a name attribute
     * which will be treated as root path of the service
     */
    String serviceXML = args[0].v_string.str;

    int iStatus = ITDeleteService(serviceXML.c_str());
    if (iStatus == 0) {
        status = MethodReply(msg, ER_OK);
        if (ER_OK != status) {
            QCC_LogError(status, ("Method Reply did not complete successfully"));
        }
    } else {
        status = ER_FAIL;
        CHECK_STATUS_AND_REPLY("Error while publishing service to cloud");
    }
}

void CloudCommEngineBusObject::AJUpdateSignalHandlerInfoToCloud(const ajn::InterfaceDescription::Member* member, ajn::Message& msg)
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

    //  The first arg is the peer address: thierry_luo@nane.cn
    if (args[0].typeId != ALLJOYN_STRING || args[0].v_string.len <= 1) {
        status = ER_BAD_ARG_1;
        CHECK_STATUS_AND_REPLY("Error call address");
    }
    String peerAddr(args[0].v_string.str);

    // The second arg is the peer BusName and ObjPath: BusName/ObjPath
    String peerBusNameObjPath(args[1].v_string.str);
    while ('/' == peerBusNameObjPath[peerBusNameObjPath.length() - 1]) {
        peerBusNameObjPath.erase(peerBusNameObjPath.length() - 1, 1);
    }

    // The third arg is the local BusName
   String localBusName(args[2].v_string.str);
   // The fourth arg is the local Session ID
   unsigned int localSessionId = args[3].v_uint32;

   String msgBuf = "<SignalHandlerInfo busName=\"";
   msgBuf += localBusName;
   msgBuf += "\"";
   msgBuf += " sessionId=\"";
   msgBuf += qcc::U32ToString(localSessionId, 10);
   msgBuf += "\"></SignalHandlerInfo>";

   int itStatus = ITSendCloudMessage(gwConsts::customheader::RPC_MSG_TYPE_UPDATE_SIGNAL_HANDLER, peerAddr.c_str(), NULL, peerBusNameObjPath.c_str(), msgBuf.c_str(), NULL);
   if (0 != itStatus) {
       QCC_LogError(ER_FAIL, ("Failed to send message to cloud"));
       status = MethodReply(msg, (QStatus)itStatus);
       if (ER_OK != status) {
           QCC_LogError(status, ("Method Reply did not complete successfully"));
       }
   }
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
    CloudCommEngineBusObject* cceBusObject = arg->cceBusObject;
    if (!cceBusObject) {
        return;
    }
    AsyncReplyContext* replyContext = arg->replyContext;
    if (!replyContext) {
        return;
    }
    String& calledAddr = arg->calledAddr;
    size_t slash = calledAddr.find_first_of('/');
    if (String::npos == slash || slash >= calledAddr.size() - 1) {
        // The called addr format is not correct
        QCC_LogError(ER_FAIL, ("The format of called address is not correct"));
        status = cceBusObject->MethodReply(replyContext->msg, ER_FAIL);
        if (ER_OK != status) {
            QCC_LogError(status, ("Method Reply did not complete successfully"));
        }
        return;
    }
    String peer = calledAddr.substr(0, slash);
    String addr = calledAddr.substr(slash + 1, calledAddr.size() - slash - 1);
    String argsStr("<args>\n");
    if (arg->inArgs && arg->inArgsNum > 0) {
        for (unsigned int i = 0; i < arg->inArgsNum; i++) {
            argsStr += ArgToXml(&arg->inArgs[i], 0);
        }
    }
    argsStr +=  "</args>";

    char* resBuf = NULL;
    int itStatus = ITSendCloudMessage(gwConsts::customheader::RPC_MSG_TYPE_METHOD_CALL, peer.c_str(), NULL, addr.c_str(), argsStr.c_str(), &resBuf);
    if (0 != itStatus) {
        if (resBuf) {
            ITReleaseBuf(resBuf);
        }
        QCC_LogError(ER_FAIL, ("Failed to send message to cloud"));
        status = cceBusObject->MethodReply(replyContext->msg, (QStatus)itStatus);
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
            status = cceBusObject->MethodReply(replyContext->msg, status);
            if (ER_OK != status) {
                QCC_LogError(status, ("Method Reply did not complete successfully"));
            }
            ITReleaseBuf(resBuf);
            return;
        }
        const XmlElement* rootEle = pc.GetRoot();
        if (!rootEle) {
            QCC_LogError(ER_FAIL, ("The message format is not correct"));
            status = cceBusObject->MethodReply(replyContext->msg, ER_FAIL);
            if (ER_OK != status) {
                QCC_LogError(status, ("Method Reply did not complete successfully"));
            }
            ITReleaseBuf(resBuf);
            return;
        }

        std::vector<MsgArg> argsVec;
        argsVec.clear();
        const std::vector<XmlElement*>& argsEles = rootEle->GetChildren();
        for (size_t argIndx = 0; argIndx < argsEles.size(); argIndx++) {
            MsgArg arg;
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
        cloudReplyArgs[0].Set("av", argsVec.size(), argsArray);

        // The cloud session ID is not returned. To be continued.
        cloudReplyArgs[1].Set("s", "");

        status = cceBusObject->MethodReply(replyContext->msg, cloudReplyArgs, 2);
        if (ER_OK != status) {
            QCC_LogError(status, ("Method Reply did not complete successfully"));
        }
        ITReleaseBuf(resBuf);
    } else {
        // No result received
        status = cceBusObject->MethodReply(replyContext->msg, ER_OK);
    }
}

ThreadReturn CloudCommEngineBusObject::MessageReceiverThreadFunc(void* arg)
{
    CloudCommEngineBusObject* cceBusObject = (CloudCommEngineBusObject*)arg;
    char* msgBuf = NULL;
    while (0 == ITReadCloudMessage(&msgBuf)) {
        if (!cceBusObject->proximalEngineProxyBusObject.unwrap()) {
            continue;
        }
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

            if (msgTypeN <= gwConsts::customheader::RPC_MSG_TYPE_SIGNAL_RET) {
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

                switch (msgTypeN) {
                case gwConsts::customheader::RPC_MSG_TYPE_METHOD_CALL:
                    {
                        MsgArg args[3];
                        args[0].Set("s", addr);
                        if (argsNum > 0) {
                            args[1].Set("av", argsNum, argsArray);
                        }
                        args[2].Set("s", callId);

                        BusAttachment& bus = (BusAttachment&)cceBusObject->GetBusAttachment();
                        ajn::Message replyMsg(bus);
                        // Calling the local method
                        // TBD: Change it to MethodCallAsync or start a new task thread to execute the MethodCall
                        status = cceBusObject->proximalEngineProxyBusObject->MethodCall(sipe2e::gateway::gwConsts::SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_INTERFACE.c_str(),
                            sipe2e::gateway::gwConsts::SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_LOCALMETHODCALL.c_str(),
                            args, 3, replyMsg);

                        if (ER_OK != status) {
                            QCC_LogError(status, ("Error executing local method call"));
                            ITSendCloudMessage(msgTypeN+1, peer, callId, addr, "", NULL);
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

                            int itStatus = ITSendCloudMessage(msgTypeN+1, peer, callId, addr, argsStr.c_str(), NULL);
                            if (0 != itStatus) {
                                QCC_LogError(ER_FAIL, ("Failed to send message to cloud"));
                            }
                        } else {
                            // The reply message is not correct
                            // Reply with empty message
                            ITSendCloudMessage(msgTypeN+1, peer, callId, addr, "", NULL);
                        }
                    }
                    break;
                case gwConsts::customheader::RPC_MSG_TYPE_SIGNAL_CALL:
                    {
                        MsgArg args[4];

                        String senderReceiverAddr(addr);
                        size_t dot = senderReceiverAddr.find_first_of('`');
                        if (dot == String::npos) {
                            QCC_LogError(ER_FAIL, ("The message format is not correct"));
                            ITReleaseBuf(msgBuf);
                            continue;
                        }
                        String senderAddr(peer);
                        senderAddr += "/";
                        senderAddr += senderReceiverAddr.substr(0, dot);
                        String receiverAddr = senderReceiverAddr.substr(dot + 1, senderReceiverAddr.size() - dot - 1);

                        args[0].Set("s", senderAddr);
                        args[1].Set("s", receiverAddr);

                        if (argsNum > 0) {
                            args[2].Set("av", argsNum, argsArray);
                        }
                        args[3].Set("s", callId);
                        // Signaling the local BusObject
                        // TBD: Change it to MethodCallAsync or start a new task thread to execute the MethodCall
                        status = cceBusObject->proximalEngineProxyBusObject->MethodCall(sipe2e::gateway::gwConsts::SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_INTERFACE.c_str(),
                            sipe2e::gateway::gwConsts::SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_LOCALSIGNALCALL.c_str(),
                            args, 4);
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
                char* msgContent = tmp + 1;

                MsgArg args[4];
                args[0].Set("s", addr);
                args[1].Set("s", peer);

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
                    args[2].Set("s", peerBusName.c_str());
                    args[3].Set("u", qcc::StringToU32(peerSessionId, 10, 0));
                } else {
                    // the root node is not SignalHandlerInfo node, just ignore it
                    QCC_DbgPrintf(("The root node is not SignalHandlerInfo."));
                    ITReleaseBuf(msgBuf);
                    continue;
                }

                // Updating
                status = cceBusObject->proximalEngineProxyBusObject->MethodCall(sipe2e::gateway::gwConsts::SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_INTERFACE.c_str(),
                    sipe2e::gateway::gwConsts::SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_UPDATESIGNALHANDLERINFO.c_str(),
                    args, 4);
                if (ER_OK != status) {
                    QCC_LogError(status, ("Error updating signal handler info to local"));
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
        if (!cceBusObject->proximalEngineProxyBusObject.unwrap()) {
            continue;
        }
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
                        status = cceBusObject->proximalEngineProxyBusObject->MethodCall(gwConsts::SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_INTERFACE.c_str(),
                            gwConsts::SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_SUBSCRIBE.c_str(),
                            &proximalCallArg, 2);
                        if (ER_OK != status) {
                            QCC_LogError(status, ("Error subscribing cloud service to local"));
                        }
                    } else {
                        // if the status is closed, then unsubscribe it
                        MsgArg proximalCallArg("ss", peer, serviceIntrospectionXml.c_str());
                        status = cceBusObject->proximalEngineProxyBusObject->MethodCall(gwConsts::SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_INTERFACE.c_str(),
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
    }
    return NULL;
}

} // namespace gateway
} // namespace sipe2e