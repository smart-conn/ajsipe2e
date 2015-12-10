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
#ifndef CLOUDCOMMENGINEBUSOBJECT_H_
#define CLOUDCOMMENGINEBUSOBJECT_H_

#include <qcc/platform.h>

#include <map>
#include <vector>

#include <qcc/ManagedObj.h>
#include <qcc/ThreadPool.h>

#include <alljoyn/Status.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/BusObject.h>
#include <alljoyn/ProxyBusObject.h>
#include <alljoyn/InterfaceDescription.h>

struct axutil_env;
typedef struct axutil_env axutil_env_t;
// Delete dependency on Axis2, 20151019, LYH
/*
struct axis2_stub;
typedef struct axis2_stub axis2_stub_t;
*/
struct axutil_thread_t;
typedef struct axutil_thread_t axutil_thread_t;
struct axiom_node;
typedef struct axiom_node axiom_node_t;


namespace ajn {
    class MsgArg;
    namespace services {
        class AboutService;
    }
}

namespace qcc {
    class XmlElement;
}

namespace sipe2e {
namespace gateway {

struct _CloudMethodCallThreadArg;
typedef struct _CloudMethodCallThreadArg CloudMethodCallThreadArg;

/**
 * CloudCommEngineBusObject class. Base class to provide cloud communication functionality
 */
class CloudCommEngineBusObject : public ajn::BusObject
{
public:
    CloudCommEngineBusObject(qcc::String const& objectPath, uint32_t threadPoolSize);
    virtual ~CloudCommEngineBusObject();

public:
    /**
     * @param proximalCommBus -     the BusAttachment that connects ProximalCommEngine and CloudCommEngine
     */
    QStatus Init(ajn::BusAttachment& cloudCommBus, ajn::services::AboutService& cloudCommAboutService);

    void ResetProximalEngineProxyBusObject(qcc::ManagedObj<ajn::ProxyBusObject> obj);

    // Delete dependency on Axis2, 20151019, LYH
/*
    void SetAxis2Env(axutil_env_t* env);
    axutil_env_t* GetAxis2Env();
*/
    /**
     * Clean up the AllJoyn execution context
     * @param -    
     */
    QStatus Cleanup();

public:
    /************************************************************************/
    /* interface implementation for AllJoyn network */
    /************************************************************************/

    /**
    * Callback when local object call a method on some cloud interface
     * @param member - the member (method) of the interface that was executed
     * @param msg - the Message of the method
     */
    void AJCloudMethodCall(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);

    /**
    * Callback when local object call a method on some cloud interface
     * @param member - the member (method) of the interface that was executed
     * @param msg - the Message of the method
     */
    void AJCloudSignalCall(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);

    /**
     * Callback when publishing some local service to cloud
     * @param member - the member (method) of the interface that was executed
     * @param msg - the Message of the method
     */
    void AJPublishLocalServiceToCloud(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);

    /**
     * Callback when deleting some local service from cloud
     * @param member - the member (method) of the interface that was executed
     * @param msg - the Message of the method
     */
    void AJDeleteLocalServiceFromCloud(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);

    /**
     * Callback when updating local signal handler info to Cloud or Far Peer
     * @param member - the member (method) of the interface that was executed
     * @param msg - the Message of the method
     */
    void AJUpdateSignalHandlerInfoToCloud(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);


    /**
     * Callback when deleting some local service from cloud
     * @param member - the member (method) of the interface that was executed
     * @param msg - the Message of the method
     */
    void AJSubscribe(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);
    /**
     * 
     * @param member - the member (method) of the interface that was executed
     * @param msg - the Message of the method
     */
    void AJUnsubscribe(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);

private:
    // Delete dependency on Axis2, 20151019, LYH
//     static void* __stdcall CloudMethodCallThreadFunc(axutil_thread_t * thd, void *data);

    /**
     * Build XML content 
     * @param inArgsVariant - arguments of incoming method call
     * @param inArgsVariant - number of arguments of incoming method call
     */
    // Delete dependency on Axis2, 20151019, LYH
//     axiom_node_t* BuildOmProgramatically(ajn::MsgArg* args, size_t argsNum);
    /**
     * Parse the XML content and generate the reply args
     * @param - 
     */
    // Delete dependency on Axis2, 20151019, LYH
//     void ParseOmProgramatically(axiom_node_t* node, size_t argsNum, char* argsSignature, ajn::MsgArg* args);

protected:

    class CloudMethodCallRunable : public qcc::Runnable {
    public:
        CloudMethodCallRunable(CloudMethodCallThreadArg* _arg);
        virtual ~CloudMethodCallRunable();
        virtual void Run(void);
    protected:
        CloudMethodCallThreadArg* arg;
    };

    qcc::ThreadPool cloudMethodCallThreadPool;

    /**
     *    The CloudCommEngine BusObject, that is for re-marshaling the calls and forward to cloud
     */
    qcc::ManagedObj<ajn::ProxyBusObject> proximalEngineProxyBusObject;

    ajn::services::AboutService* aboutService;

    // Delete dependency on Axis2, 20151019, LYH
/*
    axutil_env_t* axis2Env;
    std::map<qcc::String, axis2_stub_t*> cloudStubs;
*/
};

} // namespace gateway

} // namespace sipe2e

#endif // CLOUDCOMMENGINEBUSOBJECT_H_