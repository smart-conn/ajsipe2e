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
#include <qcc/StringUtil.h>
#include <qcc/XmlElement.h>
#include <qcc/StringSource.h>

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
    String calledAddr;
    AsyncReplyContext* replyContext;
    CloudCommEngineBusObject* cceBusObject;
    _CloudMethodCallThreadArg()
        : /*cloudStub(NULL),*/ // Delete dependency on Axis2, 20151019, LYH 
        // Delete the dependency on ServiceInfo, 20151021, LYH
//         inArgsNum(0), outArgsNum(0), 
//         inArgsSignature(NULL), outArgsSignature(NULL), 
        inArgs(NULL),
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

size_t StringSplit(const String& inStr, char delim, std::vector<String>& arrayOut)
{
    size_t arraySize = 0;
    size_t space = 0, startPos = 0;
    size_t arrayStrSize = inStr.size();
    space = inStr.find_first_of(delim, startPos);
    while (space != String::npos) {
        arraySize++;
        const String& currArgStr = inStr.substr(startPos, space - startPos);
        arrayOut.push_back(currArgStr);
        startPos = space + 1;
        if (startPos >= arrayStrSize) {
            break;
        }
        space = inStr.find_first_of(delim, startPos);
    }

    return arraySize;
}


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
        "savs", "avx", "addr,para,localSessionID,reply,cloudSessionID");
    intf->AddMethod(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_PUBLISH.c_str(),
        "s", NULL, "introspectionXML");
    intf->AddMethod(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_DELETE.c_str(),
        "s", NULL, "introspectionXML");
    intf->Activate();
    this->AddInterface(*intf, BusObject::ANNOUNCED);
    const MethodEntry methodEntries[] = {
        { intf->GetMember(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_CLOUDMETHODCALL.c_str()), 
        static_cast<MessageReceiver::MethodHandler>(&CloudCommEngineBusObject::AJCloudMethodCall) },
        { intf->GetMember(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_PUBLISH.c_str()), 
        static_cast<MessageReceiver::MethodHandler>(&CloudCommEngineBusObject::AJPublishLocalServiceToCloud) },
        { intf->GetMember(SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_DELETE.c_str()), 
        static_cast<MessageReceiver::MethodHandler>(&CloudCommEngineBusObject::AJDeleteLocalServiceFromCloud) }
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

    // The second arg is the parameter array for this local method call 
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
    const char* localSessionID = NULL;
    status = args[2].Get("s", &localSessionID);
    if (ER_OK == status && localSessionID) {
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
    argList->calledAddr = addr;
    argList->cceBusObject = this;
    argList->replyContext = new AsyncReplyContext(msg, member);
    CloudMethodCallRunable* cmcRunable = new CloudMethodCallRunable(argList);
    status = cloudMethodCallThreadPool.Execute(cmcRunable);
    CHECK_STATUS_AND_REPLY("Error running the CloudMethodCall thread maybe because of resource constraint");
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

qcc::String CloudCommEngineBusObject::ArgToXml(const ajn::MsgArg* args, size_t indent)
{
    qcc::String str;
    if (!args) {
        return str;
    }
#define CHK_STR(s)  (((s) == NULL) ? "" : (s))
    qcc::String in = qcc::String(indent, ' ');

    str = in;

    indent += 2;

    switch (args->typeId) {
    case ALLJOYN_ARRAY:
        str += "<array type_sig=\"" + qcc::String(CHK_STR(args->v_array.GetElemSig())) + "\">";
        for (uint32_t i = 0; i < args->v_array.GetNumElements(); i++) {
            str += "\n" + ArgToXml(&args->v_array.GetElements()[i], indent)/*args->v_array.elements[i].ToString(indent)*/;
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_BOOLEAN:
        str += args->v_bool ? "<boolean>1</boolean>" : "<boolean>0</boolean>";
        break;

    case ALLJOYN_DOUBLE:
        // To be bit-exact stringify double as a 64 bit hex value
        str += "<double>" "0x" + U64ToString(args->v_uint64, 16) + "</double>";
        break;

    case ALLJOYN_DICT_ENTRY:
        str += "<dict_entry>\n" +
            ArgToXml(args->v_dictEntry.key, indent) + "\n" + 
//             args->v_dictEntry.key->ToString(indent) + "\n" +
//             args->v_dictEntry.val->ToString(indent) + "\n" +
            ArgToXml(args->v_dictEntry.val, indent) + "\n" + 
            in + "</dict_entry>";
        break;

    case ALLJOYN_SIGNATURE:
        str += "<signature>" + qcc::String(CHK_STR(args->v_signature.sig)) + "</signature>";
        break;

    case ALLJOYN_INT32:
        str += "<int32>" + I32ToString(args->v_int32) + "</int32>";
        break;

    case ALLJOYN_INT16:
        str += "<int16>" + I32ToString(args->v_int16) + "</int16>";
        break;

    case ALLJOYN_OBJECT_PATH:
        str += "<object_path>" + qcc::String(CHK_STR(args->v_objPath.str)) + "</object_path>";
        break;

    case ALLJOYN_UINT16:
        str += "<uint16>" + U32ToString(args->v_uint16) + "</uint16>";
        break;

    case ALLJOYN_STRUCT:
        str += "<struct>\n";
        for (uint32_t i = 0; i < args->v_struct.numMembers; i++) {
            str += ArgToXml(&args->v_struct.members[i], indent)/*args->v_struct.members[i].ToString(indent)*/ + "\n";
        }
        str += in + "</struct>";
        break;

    case ALLJOYN_STRING:
        str += "<string>" + qcc::String(CHK_STR(args->v_string.str)) + "</string>";
        break;

    case ALLJOYN_UINT64:
        str += "<uint64>" + U64ToString(args->v_uint64) + "</uint64>";
        break;

    case ALLJOYN_UINT32:
        str += "<uint32>" + U32ToString(args->v_uint32) + "</uint32>";
        break;

    case ALLJOYN_VARIANT:
        str += "<variant signature=\"" + args->v_variant.val->Signature() + "\">\n";
        str += ArgToXml(args->v_variant.val, indent)/*args->v_variant.val->ToString(indent)*/;
        str += "\n" + in + "</variant>";
        break;

    case ALLJOYN_INT64:
        str += "<int64>" + I64ToString(args->v_int64) + "</int64>";
        break;

    case ALLJOYN_BYTE:
        str += "<byte>" + U32ToString(args->v_byte) + "</byte>";
        break;

    case ALLJOYN_HANDLE:
        str += "<handle>" + qcc::BytesToHexString((const uint8_t*)&args->v_handle.fd, sizeof(args->v_handle.fd)) + "</handle>";
        break;

    case ALLJOYN_BOOLEAN_ARRAY:
        str += "<array type=\"boolean\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += args->v_scalarArray.v_bool[i] ? "1 " : "0 ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_DOUBLE_ARRAY:
        str += "<array type=\"double\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += U64ToString((uint64_t)args->v_scalarArray.v_double[i]) + " ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_INT32_ARRAY:
        str += "<array type=\"int32\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += I32ToString(args->v_scalarArray.v_int32[i]) + " ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_INT16_ARRAY:
        str += "<array type=\"int16\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += I32ToString(args->v_scalarArray.v_int16[i]) + " ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_UINT16_ARRAY:
        str += "<array type=\"uint16\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += U32ToString(args->v_scalarArray.v_uint16[i]) + " ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_UINT64_ARRAY:
        str += "<array type=\"uint64\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += U64ToString(args->v_scalarArray.v_uint64[i]) + " ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_UINT32_ARRAY:
        str += "<array type=\"uint32\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += U32ToString(args->v_scalarArray.v_uint32[i]) + " ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_INT64_ARRAY:
        str += "<array type=\"int64\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += I64ToString(args->v_scalarArray.v_int64[i]) + " ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_BYTE_ARRAY:
        str += "<array type=\"byte\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += U32ToString(args->v_scalarArray.v_byte[i]) + " ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    default:
        str += "<invalid/>";
        break;
    }
    str += "\n";
#undef CHK_STR
    return str;
}

QStatus CloudCommEngineBusObject::XmlToArg(const XmlElement* argEle, MsgArg& arg)
{
    QStatus status = ER_OK;

    const String& argType = argEle->GetName();
    // here calculate a simple hash value to use switch
    int32_t hashVal = 0;
    for (size_t i = 0; i < argType.size(); i++) {
        hashVal += argType[i];
    }

    switch (hashVal) {
    case 543: // array
        {
            const String& arrayType = argEle->GetAttribute("type");
            int32_t hashValArray = 0;
            for (size_t j = 0; j < arrayType.size(); j++) {
                hashValArray += arrayType[j];
            }

#define ParseArrayArg(StringToFunc, arraySig, argType) \
    const String& argStr = argEle->GetContent(); \
    std::vector<String> arrayTmp; \
    size_t arraySize = StringSplit(argStr, ' ', arrayTmp); \
    if (arraySize > 0) { \
        argType* argArray = new argType[arraySize]; \
        for (size_t indx = 0; indx < arraySize; indx++) { \
            argArray[indx] = StringToFunc(arrayTmp[indx]); \
        } \
        status = arg.Set(arraySig, arraySize, argArray); \
    }

            switch (hashValArray) {
            case 736: // boolean array
                {
                    const String& booleanStr = argEle->GetContent();
                    size_t booleanArraySize = booleanStr.size();
                    if (booleanArraySize > 0) {
                        bool* booleanArray = new bool[booleanArraySize];
                        for (size_t boolIndx = 0; boolIndx < booleanArraySize; boolIndx++) {
                            booleanArray[boolIndx] = (booleanStr[boolIndx] == '1' ? true : false);
                        }
                        status = arg.Set("ab", booleanArraySize, booleanArray);
                    } else {}
                }
                break;
            case 635: // double array
                {
                    ParseArrayArg(StringToDouble, "ad", double);
                }
                break;
            case 432: // int32 array
                {
                    ParseArrayArg(StringToI32, "ai", int32_t);
                }
                break;
            case 434: // int16 array
                {
                    ParseArrayArg(StringToI32, "an", int16_t);
                }
                break;
            case 551: // uint16 array
                {
                    ParseArrayArg(StringToU32, "aq", uint16_t);
                }
                break;
            case 554: // uint64 array
                {
                    ParseArrayArg(StringToU64, "at", uint64_t);
                }
                break;
            case 549: // uint32 array
                {
                    ParseArrayArg(StringToU32, "au", uint32_t);
                }
                break;
            case 437: // int64 array
                {
                    ParseArrayArg(StringToI64, "ax", int64_t);
                }
                break;
            case 436: // byte array
                {
                    ParseArrayArg(StringToU32, "ay", uint8_t);
                }
                break;
            default: // array of variant
                {
                    // fill the array of variant by traversing its children
                    const std::vector<XmlElement*>& argsVec = argEle->GetChildren();
                    size_t argsNum = argsVec.size();
                    if (argsNum > 0) {
                        MsgArg* argsArray = new MsgArg[argsNum];
                        for (size_t argIndx = 0; argIndx < argsNum; argIndx++) {
                            const XmlElement* currArgEle = argsVec[argIndx];
                            status = XmlToArg(currArgEle, argsArray[argIndx]);
                            if (ER_OK != status) {
                                return status;
                            }
                        }
                        status = arg.Set("av", argsNum, argsArray);
                    } else {}
                }
                break;
            }
        }
        break;
    case 736: // boolean
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("b", (argStr[0] == '1' ? true : false));
        }
        break;
    case 635: // double
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("d", StringToDouble(argStr));
        }
        break;
    case 1077: // dict_entry
        {
            const std::vector<XmlElement*>& entryEleVec = argEle->GetChildren();
            if (entryEleVec.size() != 2) {// should be one key and one value
                break;
            }
            arg.v_dictEntry.key = new MsgArg();
            arg.v_dictEntry.val = new MsgArg();
            status = XmlToArg(entryEleVec[0], *arg.v_dictEntry.key);
            if (ER_OK != status) {
                return status;
            }
            status = XmlToArg(entryEleVec[1], *arg.v_dictEntry.val);
        }
        break;
    case 978: // signature
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("g", strdup(argStr.c_str()));
        }
        break;
    case 432: // int32
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("i", StringToI32(argStr));
        }
        break;
    case 434: // int16
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("n", StringToI32(argStr));
        }
        break;
    case 1155: // object_path
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("o", strdup(argStr.c_str()));
        }
        break;
    case 551: // uint16
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("q", StringToU32(argStr));
        }
        break;
    case 677: // struct
        {
            // fill the array of variant by traversing its children
            const std::vector<XmlElement*>& argsVec = argEle->GetChildren();
            size_t argsNum = argsVec.size();
            if (argsNum > 0) {
                MsgArg* argsArray = new MsgArg[argsNum];
                for (size_t argIndx = 0; argIndx < argsNum; argIndx++) {
                    const XmlElement* currArgEle = argsVec[argIndx];
                    status = XmlToArg(currArgEle, argsArray[argIndx]);
                    if (ER_OK != status) {
                        return status;
                    }
                }
                arg.typeId = ALLJOYN_STRUCT;
                arg.v_struct.numMembers = argsNum;
                arg.v_struct.members = argsArray;
            } else {}
        }
        break;
    case 663: // string
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("s", strdup(argStr.c_str()));
        }
        break;
    case 554: // uint64
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("t", StringToU64(argStr));
        }
        break;
    case 549: // uint32
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("u", StringToU32(argStr));
        }
        break;
    case 757: // variant
        {
            // fill the array of variant by traversing its children
            const std::vector<XmlElement*>& argsVec = argEle->GetChildren();
            arg.typeId = ALLJOYN_VARIANT;
            arg.v_variant.val = new MsgArg();
            status = XmlToArg(argsVec[0], *arg.v_variant.val);
        }
        break;
    case 437: // int64
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("x", StringToI64(argStr));
        }
        break;
    case 436: // byte
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("y", StringToU32(argStr));
        }
        break;
    case 620: // handle
        {
            const String& argStr = argEle->GetContent();
            SocketFd fd;
            HexStringToBytes(argStr, (uint8_t*)&fd, 32);
            status = arg.Set("h", fd);
        }
        break;
    default:
        break;
    }
    return status;
}


CloudCommEngineBusObject::CloudMethodCallRunable::CloudMethodCallRunable(CloudMethodCallThreadArg* _arg)
    : arg(_arg)
{

}

CloudCommEngineBusObject::CloudMethodCallRunable::~CloudMethodCallRunable()
{

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
    String argsStr;
    if (arg->inArgs) {
        argsStr = String("<args>\n") + ArgToXml(arg->inArgs, 0) + String("</args>");
    }
    char* resBuf = NULL;
    int itStatus = ITSendCloudMessage(1, peer.c_str(), NULL, addr.c_str(), argsStr.c_str(), &resBuf);
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

        status = cceBusObject->XmlToArg(pc.GetRoot(), cloudReplyArgs[0]);
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