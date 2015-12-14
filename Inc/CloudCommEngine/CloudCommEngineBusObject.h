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
#include <qcc/Thread.h>
#include <qcc/ThreadPool.h>

#include <alljoyn/Status.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/BusObject.h>
#include <alljoyn/ProxyBusObject.h>
#include <alljoyn/InterfaceDescription.h>
#include <alljoyn/about/AnnounceHandler.h>

#include "Common/GatewayStd.h"

struct axutil_env;
typedef struct axutil_env axutil_env_t;
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
struct _LocalMethodCallThreadArg;
typedef struct _LocalMethodCallThreadArg LocalMethodCallThreadArg;

class CloudServiceAgentBusObject;
class ProximalProxyBusObjectWrapper;
typedef qcc::ManagedObj<ProximalProxyBusObjectWrapper> _ProximalProxyBusObjectWrapper;

/**
 * CloudCommEngineBusObject class. Base class to provide cloud communication functionality
 */
class CloudCommEngineBusObject : public ajn::BusObject
{
    friend class CloudServiceAgentBusObject;
    friend class ProximalProxyBusObjectWrapper;
public:
    CloudCommEngineBusObject(qcc::String const& objectPath, uint32_t threadPoolSize);
    virtual ~CloudCommEngineBusObject();

    typedef struct  
    {
        qcc::String addr;
        qcc::String busName;
        unsigned int sessionId;
    } SignalHandlerInfo;

        /**
     *    This AnnounceHandler is receiving all announcements from local objects, and then create
     * ProxyBusObjects for every service (app/device)
     */
    class LocalServiceAnnounceHandler : public ajn::services::AnnounceHandler
    {
        friend class CloudCommEngineBusObject;

        class AnnounceHandlerTask : public qcc::Runnable
        {
            friend class CloudCommEngineBusObject;
        public:
            AnnounceHandlerTask(CloudCommEngineBusObject* owner);
            virtual ~AnnounceHandlerTask();

            /**
             * Set the AnnounceContent data
             * @param -    
             */
            void SetAnnounceContent(uint16_t version, uint16_t port, const char* busName, const ObjectDescriptions& objectDescs,
                const AboutData& aboutData);

            /**
              * This method is called by the ThreadPool when the Runnable object is
              * dispatched to a thread.  A client of the thread pool is expected to
              * define a method in a derived class that does useful work when Run() is
              * called.
              */
            virtual void Run(void);

        private:
            /* The parent BusObject that owns this AnnounceHandler instance */
            CloudCommEngineBusObject* ownerBusObject;
            /* The announcement content to be precessed by this task */
            AnnounceContent announceData;
        };

    public:
        LocalServiceAnnounceHandler(CloudCommEngineBusObject* owner);
        ~LocalServiceAnnounceHandler();

        /**
         * Receiving all announcements from local apps/devices/services, and create ProxyBusObjects for them
         * 
         * @param[in] version of the AboutService.
         * @param[in] port used by the AboutService
         * @param[in] busName unique bus name of the service
         * @param[in] objectDescs map of ObjectDescriptions using qcc::String as key std::vector<qcc::String>   as value, describing interfaces
         * @param[in] aboutData map of AboutData using qcc::String as key and ajn::MsgArg as value
         */
        virtual void Announce(uint16_t version, uint16_t port, const char* busName, const ObjectDescriptions& objectDescs,
            const AboutData& aboutData);

    protected:
        /* The parent BusObject that owns this AnnounceHandler instance */
        CloudCommEngineBusObject* ownerBusObject;
        /* The ThreadPool that manages the pinging bus/joining sessions/creating ProxyBusObjects tasks */
        qcc::ThreadPool taskPool;
    };


public:
    /**
     * @param cloudCommBus -     the BusAttachment that connects CloudCommEngine and CloudCommEngine
     */
    QStatus Init(ajn::BusAttachment& cloudCommBus, ajn::services::AboutService& cloudCommAboutService);

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
    QStatus SubscribeCloudServiceToLocal(const qcc::String& serviceAddr, const qcc::String& serviceIntrospectionXml);

    QStatus UnsubscribeCloudServiceFromLocal(const qcc::String& serviceAddr, const qcc::String& serviceIntrospectionXml);

    QStatus LocalMethodCall(const qcc::String& addr, size_t inArgsNum, const ajn::MsgArg* inArgsArray, const qcc::String& cloudSessionId, 
        size_t& outArgsNum, const ajn::MsgArg*& outArgsArray, unsigned int& localSessionId);

    QStatus LocalSignalCall(const qcc::String& senderAddr, const qcc::String& receiverAddr,
        size_t inArgsNum, const ajn::MsgArg* inArgsArray, const qcc::String& cloudSessionId);

    QStatus UpdateSignalHandlerInfoToLocal(const qcc::String& localBusNameObjPath, const qcc::String& peerAddr, 
        const qcc::String& peerBusName, unsigned int peerSessionId);

    QStatus CloudMethodCall(const qcc::String& addr, size_t inArgsNum, const ajn::MsgArg* inArgsArray, unsigned int localSessionId, 
        CloudServiceAgentBusObject* agent, ajn::Message msg);

    QStatus CloudSignalCall(const qcc::String& senderAddr, const qcc::String& receiverAddr,
        size_t inArgsNum, const ajn::MsgArg* inArgsArray, unsigned int localSessionId);

    QStatus PublishLocalServiceToCloud(const qcc::String& serviceIntrospectionXml);

    QStatus DeleteLocalServiceFromCloud(const qcc::String& serviceIntrospectionXml);

    QStatus UpdateSignalHandlerInfoToCloud(const qcc::String& peerAddr, const qcc::String& peerBusNameObjPath, 
        const qcc::String& localBusName, unsigned int localSessionId);

private:
    /**
     * @internal
     * Save the ProxyBusObject and all its children to the map 'proxyBusObjects'
     * @param proxy - the ProxyBusObject that will be saved, including its children 
     * @param busName -  the name of the bus that owns the remote BusObject
     */
    void SaveProxyBusObject(_ProximalProxyBusObjectWrapper proxyWrapper);

    static qcc::ThreadReturn STDCALL MessageReceiverThreadFunc(void* arg);
    static qcc::ThreadReturn STDCALL NotificationReceiverThreadFunc(void* arg);

protected:

    class CloudMethodCallRunable : public qcc::Runnable {
    public:
        CloudMethodCallRunable(CloudMethodCallThreadArg* _arg);
        virtual ~CloudMethodCallRunable();
        virtual void Run(void);
    protected:
        CloudMethodCallThreadArg* arg;
    };

    class LocalMethodCallRunable : public qcc::Runnable {
    public:
        LocalMethodCallRunable(LocalMethodCallThreadArg* _arg);
        virtual ~LocalMethodCallRunable();
        virtual void Run(void);
    protected:
        LocalMethodCallThreadArg* arg;
    };

    qcc::ThreadPool methodCallThreadPool;

    ajn::services::AboutService* aboutService;

    qcc::Thread messageReceiverThread, notificationReceiverThread;

protected:
    /**
     *    All ProxyBusObjects for handling incoming calls towards proximal AllJoyn service
     * The key is the string "BusName/ObjectPath"
     *
     * Once an announcement from a proximal device is received, corresponding ProxyBusObject should be
     * created to later execute the calls on behalf of calls from cloud
     * 
     * Be noted that, this map not only contains top-level ProxyBusObjects but also all children, not like
     * the map cloudBusObjects which only contains top-level BusObjects.
     * So, while session is lost or cleanup the ProxyBusObject and all its children should be removed and 
     * deleted. However, when a ProxyBusObject is deleted, all children will be deleted in the destructor. 
     * So, here when we try to cleanup the ProxyBusObjects map, we need to only retrieve the top-level 
     * ProxyBusObject and delete it. That's all needed to be done.
     */
    std::map<qcc::String, _ProximalProxyBusObjectWrapper> proxyBusObjects;

    /* The AllJoyn context for all ProxyBusObjects to access local services */
    AllJoynContext proxyContext;

    /**
     *    All BusObjects for exposing cloud services to proximal AllJoyn network on behalf of cloud service providers
     * The key is the cloud service address, e.g. weather_server_A@nane.cn, lyh@nane.cn
     *
     * Be noted that, this map only contains top-level BusObjects, but not their children BusOjbects. Actually, the
     * top-level BusObjects are empty with no interfaces exposed. 
     */
    std::map<qcc::String, CloudServiceAgentBusObject*> cloudBusObjects;

    /**
     * This map will stores all remote signal handlers information, with local BusName as the key, and a list of SignalHanderInfo as value
     */
    std::map<qcc::String, std::map<qcc::String, std::vector<SignalHandlerInfo>>> signalHandlersInfo;

};

} // namespace gateway

} // namespace sipe2e

#endif // CLOUDCOMMENGINEBUSOBJECT_H_