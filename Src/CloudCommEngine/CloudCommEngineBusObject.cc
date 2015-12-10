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

// Delete dependency on Axis2, 20151019, LYH
/*
#include <platforms/axutil_platform_auto_sense.h>
#include <axiom.h>
#include <axis2_util.h>
#include <axiom_soap.h>
#include <axis2_client.h>
*/

#include "CloudCommEngine/CloudCommEngineBusObject.h"
#include "Common/GatewayStd.h"
#include "Common/GatewayConstants.h"

#include "CloudCommEngine/IMSTransport/IMSTransportExport.h"
#include "CloudCommEngine/IMSTransport/IMSTransportConstants.h"

#define QCC_MODULE "SIPE2E"


using namespace ajn;
using namespace qcc;
using namespace std;

namespace sipe2e {
namespace gateway {

using namespace gwConsts;

typedef struct _CloudMethodCallThreadArg
{
    // Delete dependency on Axis2, 20151019, LYH
//     axis2_stub_t* cloudStub;
    // Delete the dependency on ServiceInfo, 20151021, LYH
//     size_t inArgsNum, outArgsNum;
//     char* inArgsSignature, * outArgsSignature;
    MsgArg* inArgs;
    unsigned int inArgsNum;
    String calledAddr;
    AsyncReplyContext* replyContext;
    CloudCommEngineBusObject* cceBusObject;
    _CloudMethodCallThreadArg()
        : /*cloudStub(NULL),*/ // Delete dependency on Axis2, 20151019, LYH 
        // Delete the dependency on ServiceInfo, 20151021, LYH
//         inArgsNum(0), outArgsNum(0), 
//         inArgsSignature(NULL), outArgsSignature(NULL), 
        inArgs(NULL), inArgsNum(0),
        replyContext(NULL), cceBusObject(NULL)
    {

    }
    ~_CloudMethodCallThreadArg()
    {
        // Delete dependency on Axis2, 20151019, LYH
//         cloudStub = NULL;
        // Delete the dependency on ServiceInfo, 20151021, LYH
//         if (inArgsSignature) {
//             SIReleaseBuf(inArgsSignature);
//         }
//         if (outArgsSignature) {
//             SIReleaseBuf(outArgsSignature);
//         }
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
    : BusObject(objectPath.c_str())/*, axis2Env(NULL)*//* Delete dependency on Axis2, 20151019, LYH */
    , cloudMethodCallThreadPool("CloudMethodCallThreadPool", threadPoolSize)
    , aboutService(NULL)
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
        static_cast<MessageReceiver::MethodHandler>(&CloudCommEngineBusObject::AJPublishLocalServiceToCloud) }
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

    /* Prepare the Axis2 environment */
/*
#ifdef _DEBUG
    axis2Env = axutil_env_create_all("axis2.log", AXIS2_LOG_LEVEL_TRACE);
#else
    axis2Env = axutil_env_create_all("axis2.log", AXIS2_LOG_LEVEL_ERROR);
#endif
    if (!axis2Env) {
        Cleanup();
        return ER_FAIL;
    }
*/

    return status;
}


void CloudCommEngineBusObject::ResetProximalEngineProxyBusObject(qcc::ManagedObj<ProxyBusObject> obj)
{
    proximalEngineProxyBusObject = obj;
}

// Delete dependency on Axis2, 20151019, LYH
/*
void CloudCommEngineBusObject::SetAxis2Env(axutil_env_t* env)
{
    axis2Env = env;
}

axutil_env_t* CloudCommEngineBusObject::GetAxis2Env()
{
    return axis2Env;
}
*/


QStatus CloudCommEngineBusObject::Cleanup()
{
    QStatus status = ER_OK;

    // Delete dependency on Axis2, 20151019, LYH
/*
    std::map<qcc::String, axis2_stub_t*>::iterator itrStub = cloudStubs.begin();
    while (itrStub != cloudStubs.end()) {
        axis2_stub_t* cloudStub = itrStub->second;
        if (cloudStub) {
            axis2_stub_free(cloudStub, axis2Env);
        }
        itrStub++;
    }

    if (axis2Env) {
//         axutil_env_free(axis2Env);
        // DO NOT free the axis2 environment here, which is the responsibility of the main function
        axis2Env = NULL;
    }
*/

    /* Prepare the About announcement object descriptions */
    vector<String> intfNames;
    intfNames.push_back(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_INTERFACE);
    if (aboutService) {
        status = aboutService->RemoveObjectDescription(SIPE2E_CLOUDCOMMENGINE_OBJECTPATH, intfNames);
        aboutService = NULL;
    }

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
    // Delete dependency on Axis2, 20151019, LYH
/*
    axis2_stub_t* cloudStub = NULL;
    std::map<String, axis2_stub_t*>::iterator itrStub = cloudStubs.find(addr);
    if (itrStub != cloudStubs.end()) {
        cloudStub = itrStub->second;
    } else {
        String endpointUri("ims://");
        endpointUri += addr;
        cloudStub = axis2_stub_create_with_endpoint_uri_and_client_home(axis2Env, (const axis2_char_t*)endpointUri.c_str(),(const axis2_char_t*)gwConsts::AXIS2_DEPLOY_HOME);
        if (!cloudStub) {
            status = ER_OUT_OF_MEMORY;
            CHECK_STATUS_AND_REPLY("Out of memory while creating client stub");
        }
        // now populate the axis service
        axis2_svc_client_t* svcClient = axis2_stub_get_svc_client(cloudStub, axis2Env);
        axis2_svc_t* svc = axis2_svc_client_get_svc(svcClient, axis2Env);
        // creating the operations
        axutil_qname_t* opQname = axutil_qname_create(axis2Env, "ggs", gwConsts::DEFAULT_NAMESPACE_URI, gwConsts::DEFAULT_NAMESPACE_PREFIX);
        axis2_op_t* op = axis2_op_create_with_qname(axis2Env, opQname);
        axis2_op_set_msg_exchange_pattern(op, axis2Env, AXIS2_MEP_URI_OUT_IN);
        axis2_svc_add_op(svc, axis2Env, op);
        axutil_qname_free(opQname, axis2Env);

        cloudStubs.insert(std::pair<qcc::String, axis2_stub_t*>(addr, cloudStub));
    }
    if (!cloudStub) {
        status = ER_FAIL;
        CHECK_STATUS_AND_REPLY("No client stub available for executing cloud method call");
    }
    // Now prepare the input arguments node
//     axiom_node_t* inputNode = BuildOmProgramatically(inArgsVariant, numInArgs);

    // Issue a new thread to execute the actual cloud call
    // Here we use Axis2 thread pool
    CloudMethodCallThreadArg* argList = new CloudMethodCallThreadArg();
    argList->cloudStub = cloudStub;
    argList->inArgsNum = numInArgs;
    argList->outArgsNum = outArgsNum;
    argList->inArgsSignature = inArgsSignature;
    argList->outArgsSignature = outArgsSignature;
    argList->inArgs = inArgs;
    argList->cceBusObject = this;
    argList->replyContext = new AsyncReplyContext(msg, member);
    axutil_thread_t* workerThread = axutil_thread_pool_get_thread(axis2Env->thread_pool,
        CloudMethodCallThreadFunc,
        (void *)argList);
    if (!workerThread) {
        delete argList;
        status = ER_FAIL;
        CHECK_STATUS_AND_REPLY("Error while issuing a new thread for executing cloud method call");
    }
    axutil_thread_pool_thread_detach(axis2Env->thread_pool, workerThread);
*/
    CloudMethodCallThreadArg* argList = new CloudMethodCallThreadArg();
    // Delete the dependency on ServiceInfo, 20151021, LYH
/*
    argList->inArgsNum = numInArgs;
    argList->outArgsNum = outArgsNum;
    argList->inArgsSignature = inArgsSignature;
    argList->outArgsSignature = outArgsSignature;
*/
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

// Delete dependency on Axis2, 20151019, LYH
/*
void* CloudCommEngineBusObject::CloudMethodCallThreadFunc(axutil_thread_t * thd, void *data)
{
    CloudMethodCallThreadArg* argList = (CloudMethodCallThreadArg*)data;
    QStatus status = ER_OK;
    if (!argList) {
        QCC_LogError(ER_FAIL, ("No arg found while executing cloud method call thread"));
        return NULL;
    }
    axis2_stub_t* cloudStub = argList->cloudStub;
    CloudCommEngineBusObject* cceBusObject = argList->cceBusObject;
    axutil_env_t* env = cceBusObject->GetAxis2Env();
    AsyncReplyContext* replyContext = argList->replyContext;
    // Marshal
    axiom_node_t* inputNode = cceBusObject->BuildOmProgramatically(argList->inArgs, argList->inArgsNum);

    axis2_svc_client_t* svcClient = axis2_stub_get_svc_client(cloudStub, env);
    axutil_qname_t* opQname = axutil_qname_create(env, "ggs", gwConsts::DEFAULT_NAMESPACE_URI, gwConsts::DEFAULT_NAMESPACE_PREFIX);
    axiom_node_t* retNode = axis2_svc_client_send_receive_with_op_qname(svcClient, env, opQname, inputNode);

    // Reply with the content of the retNode
    // Unmarshal
    MsgArg cloudReplyArgs[2];
    cceBusObject->ParseOmProgramatically(retNode, argList->outArgsNum, argList->outArgsSignature, &cloudReplyArgs[0]);
    // The cloud session ID is not returned. To be continued.
    cloudReplyArgs[1].Set("s", "");

    status = cceBusObject->MethodReply(replyContext->msg, cloudReplyArgs, 2);
    if (ER_OK != status) {
        QCC_LogError(status, ("Method Reply did not complete successfully"));
    }

    axutil_qname_free(opQname, env);

    delete argList;
    return NULL;
}
*/

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

// Delete dependency on Axis2, 20151019, LYH
/*
axiom_node_t* CloudCommEngineBusObject::BuildOmProgramatically(MsgArg* args, size_t argsNum)
{
    if (!args) {
        argsNum = 0;
    }
    axiom_node_t* retNode = NULL;
    axiom_namespace_t* retNodeNs = axiom_namespace_create(axis2Env, DEFAULT_NAMESPACE_URI, DEFAULT_NAMESPACE_PREFIX);
    axiom_element_t* retNodeRootEle = axiom_element_create(axis2Env, NULL, "ggs", retNodeNs, &retNode);

    size_t argIndex = 0;
    while (argIndex < argsNum) {
        axiom_text_t *argText = NULL;
        axiom_node_t *argTextNode = NULL;
        axiom_node_t* argNode = NULL;
        axiom_element_t* argNodeEle = axiom_element_create(axis2Env, retNode, "arg", retNodeNs, &argNode);
        if (argNode) {
            const MsgArg& arg = args[argIndex];
            uint8_t v_byte = 0;
            int16_t v_int16 = 0;
            uint16_t v_uint16 = 0;
            int32_t v_int32 = 0;
            uint32_t v_uint32 = 0;
            int64_t v_int64 = 0;
            uint64_t v_uint64 = 0;
            char buf[32];
            switch (arg.typeId) {
            case ALLJOYN_BOOLEAN:
                {
                    bool v_bool = arg.v_bool;
                    argText = axiom_text_create(axis2Env, argNode, v_bool?"1":"0", &argTextNode);
                }
                break;
            case ALLJOYN_DOUBLE:
                {
                    double v_double = arg.v_double;
                    snprintf(buf, 32, "%f", v_double);
                    argText = axiom_text_create(axis2Env, argNode, buf, &argTextNode);
                }
                break;
            case ALLJOYN_BYTE:
                {
                    v_byte = arg.v_byte;
                    snprintf(buf, 32, "%u", v_byte);
                    argText = axiom_text_create(axis2Env, argNode, buf, &argTextNode);
                }
                break;
            case ALLJOYN_UINT16:
                {
                    v_uint16 = arg.v_uint16;
                    snprintf(buf, 32, "%u", v_uint16);
                    argText = axiom_text_create(axis2Env, argNode, buf, &argTextNode);
                }
                break;
            case ALLJOYN_INT16:
                {
                    v_int16 = arg.v_int16;
                    snprintf(buf, 32, "%d", v_int16);
                    argText = axiom_text_create(axis2Env, argNode, buf, &argTextNode);
                }
                break;
            case ALLJOYN_INT32:
                {
                    v_int32 = arg.v_int32;
                    snprintf(buf, 32, "%d", v_int32);
                    argText = axiom_text_create(axis2Env, argNode, buf, &argTextNode);
                }
                break;
            case ALLJOYN_UINT32:
                {
                    v_uint32 = arg.v_uint32;
                    snprintf(buf, 32, "%u", v_uint32);
                    argText = axiom_text_create(axis2Env, argNode, buf, &argTextNode);
                }
                break;
            case ALLJOYN_INT64:
                {
                    v_int64 = arg.v_int64;
                    snprintf(buf, 32, "%lld", v_int64);
                    argText = axiom_text_create(axis2Env, argNode, buf, &argTextNode);
                }
                break;
            case ALLJOYN_UINT64:
                {
                    v_uint64 = arg.v_uint64;
                    snprintf(buf, 32, "%llu", v_uint64);
                    argText = axiom_text_create(axis2Env, argNode, buf, &argTextNode);
                }
                break;
            case ALLJOYN_STRING:
                {
                    const char *v_str = arg.v_string.str;
                    argText = axiom_text_create(axis2Env, argNode, v_str, &argTextNode);
                }
                break;
            case ALLJOYN_ARRAY:
                {

                }
                break;
            case ALLJOYN_STRUCT:
                {

                }
                break;
            case ALLJOYN_DICT_ENTRY: // all dictionaries are also arrays of dictionary entries, so this type may be unnecessary
                {

                }
                break;
            default:
                {

                }
                break;
            }
        }
        argIndex++;
    }

    return retNode;
}

void CloudCommEngineBusObject::ParseOmProgramatically(axiom_node_t* node, size_t argsNum, char* argsSignature, MsgArg* args)
{
    QStatus status = ER_OK;
    MsgArg* methodArgs = NULL;
    args->Set("av", argsNum, methodArgs);
    if (argsNum == 0 || argsSignature) {
        return;
    }
    methodArgs = new MsgArg[argsNum];

    char* stopStr = NULL;
    unsigned int argIndex = 0;
    char* argSignature = strtok(argsSignature, ",");
    axiom_node_t* argNode = axiom_node_get_first_child(node, axis2Env);
    while (argNode && argSignature) {
        if (axiom_node_get_node_type(argNode, axis2Env) == AXIOM_ELEMENT) {
            axiom_node_t* argContentNode = axiom_node_get_first_child(argNode, axis2Env);
            if (argContentNode) {
                switch (axiom_node_get_node_type(argContentNode, axis2Env)) {
                case AXIOM_TEXT: // Basic parameter type
                    {
                        axiom_text_t* text = (axiom_text_t*)axiom_node_get_data_element(argContentNode, axis2Env);
                        if (text) {
                            const axis2_char_t* argStr = axiom_text_get_value(text, axis2Env);
                            if (argStr) {
                                if (argIndex < argsNum) {
                                    switch (argSignature[0])
                                    {
                                    case ALLJOYN_BOOLEAN:
                                        {
                                            bool v_bool;
                                            if (!axutil_strcmp(argStr, "true") || !axutil_strcmp(argStr, "1"))
                                            {
                                                v_bool = true;
                                            }
                                            else if (!axutil_strcmp(argStr, "false") || !axutil_strcmp(argStr, "0"))
                                            {
                                                v_bool = false;
                                            }
                                            else
                                            {
                                                QCC_LogError(ER_FAIL, ("Error unmarshaling boolean parameter"));
                                                delete[] methodArgs;
                                                return;
                                            }
                                            MsgArg* arg = new MsgArg(argSignature, v_bool);
                                            methodArgs[argIndex].Set("v", arg);
                                        }
                                        break;
                                    case ALLJOYN_DOUBLE:
                                        {
                                            double v_double = strtod(argStr, &stopStr);
                                            MsgArg* arg = new MsgArg(argSignature, v_double);
                                            methodArgs[argIndex].Set("v", arg);
                                        }
                                        break;
                                    case ALLJOYN_BYTE:
                                    case ALLJOYN_UINT16:
                                    case ALLJOYN_INT16:
                                    case ALLJOYN_INT32:
                                        {
                                            int32_t v_int = strtol(argStr, &stopStr, 10);
                                            MsgArg* arg = new MsgArg(argSignature, v_int);
                                            methodArgs[argIndex].Set("v", arg);
                                        }
                                        break;
                                    case ALLJOYN_UINT32:
                                        {
                                            uint32_t v_int = strtoul(argStr, &stopStr, 10);
                                            MsgArg* arg = new MsgArg(argSignature, v_int);
                                            methodArgs[argIndex].Set("v", arg);
                                        }
                                        break;
                                    case ALLJOYN_INT64:
                                        {
                                            int64_t v_int64 = strtoll(argStr, &stopStr, 10);
                                            MsgArg* arg = new MsgArg(argSignature, v_int64);
                                            methodArgs[argIndex].Set("v", arg);
                                        }
                                        break;
                                    case ALLJOYN_UINT64:
                                        {
                                            int64_t v_int64 = strtoull(argStr, &stopStr, 10);
                                            MsgArg* arg = new MsgArg(argSignature, v_int64);
                                            methodArgs[argIndex].Set("v", arg);
                                        }
                                        break;
                                    case ALLJOYN_STRING:
                                        {
                                            MsgArg* arg = new MsgArg(argSignature, argStr);
                                            methodArgs[argIndex].Set("v", arg);
                                        }
                                        break;
                                    default:
                                        {
                                            QCC_LogError(ER_FAIL, ("Parameter type not supported"));
                                            delete[] methodArgs;
                                            return;
                                        }
                                        break;
                                    }
                                }
                            } else {
                                QCC_LogError(ER_XML_MALFORMED, ("Error getting parameter node text"));
                                delete[] methodArgs;
                                return;
                            }
                        } else {
                            QCC_LogError(ER_XML_MALFORMED, ("Error getting parameter node text"));
                            delete[] methodArgs;
                            return;
                        }
                    }
                    break;
                case AXIOM_ELEMENT: // Composite parameter type, like array, struct and dictionary (map)
                    {

                    }
                    break;
                default:
                    {
                        QCC_LogError(ER_XML_MALFORMED, ("Parameter node type not supported"));
                        delete[] methodArgs;
                        return;
                    }
                    break;
                }
            } else {
                QCC_LogError(ER_XML_MALFORMED, ("Error getting parameter node information"));
                delete[] methodArgs;
                return;
            }

            argIndex++;
            argSignature = strtok(NULL, ",");
        }
        argNode = axiom_node_get_next_sibling(argNode, axis2Env);
    }
    status = args->Set("av", argsNum, methodArgs);
    args->SetOwnershipFlags(MsgArg::OwnsArgs, true);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error setting the arg value"));
        return;
    }
}
*/


} // namespace gateway
} // namespace sipe2e