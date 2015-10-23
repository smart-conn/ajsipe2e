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

#ifndef ALLJOYNENGINEBUSOBJECT_H_
#define ALLJOYNENGINEBUSOBJECT_H_

#include <qcc/platform.h>

#include <map>
#include <vector>

#include <qcc/ManagedObj.h>
#include <qcc/XmlElement.h>
#include <qcc/ThreadPool.h>

#include <alljoyn/Status.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/BusObject.h>
#include <alljoyn/ProxyBusObject.h>
#include <alljoyn/InterfaceDescription.h>


#include "Common/GatewayConstants.h"
#include "Common/CommonBusListener.h"
#include "Common/GatewayStd.h"

#include "ProximalCommEngine/CloudServiceAgentBusObject.h"


namespace sipe2e {
namespace gateway {

class CloudServiceAgentBusObject;


/**
 * ProximalCommEngineBusObject class. Base class to provide proximal communication functionality
 * it is a management BusObject that manage the communication between cloud and AllJoyn network
 */
class ProximalCommEngineBusObject : public ajn::BusObject
{
	friend class CloudServiceAgentBusObject;
public:
	ProximalCommEngineBusObject(qcc::String const& objectPath);
	virtual ~ProximalCommEngineBusObject();


	/**
	 *	This AnnounceHandler is receiving all announcements from local objects, and then create
	 * ProxyBusObjects for every service (app/device)
	 */
	class LocalServiceAnnounceHandler : public ajn::services::AnnounceHandler
	{
		friend class ProximalCommEngineBusObject;

		class AnnounceHandlerTask : public qcc::Runnable
		{
			friend class ProximalCommEngineBusObject;
		public:
			AnnounceHandlerTask(ProximalCommEngineBusObject* owner);
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
			ProximalCommEngineBusObject* ownerBusObject;
			/* The announcement content to be precessed by this task */
			AnnounceContent announceData;
		};

	public:
		LocalServiceAnnounceHandler(ProximalCommEngineBusObject* owner);
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
		ProximalCommEngineBusObject* ownerBusObject;
		/* The ThreadPool that manages the pinging bus/joining sessions/creating ProxyBusObjects tasks */
		qcc::ThreadPool taskPool;
	};

public:
	/**
	 * Prepare the AllJoyn context for all ProxyBusObjects. Once announcements are received,
	 * corresponding ProxyBusObjects will be generated and saved
	 * Basically only one specific BusAttachment for ALL ProxyBusObjects
	 * Besides that, prepare the object description for About announcements
	 * @param proximalCommBus -	 the BusAttachment that connects ProximalCommEngine and CloudCommEngine
	 */
	QStatus Init(ajn::BusAttachment& proximalCommBus);

	void ResetCloudEngineProxyBusObject(qcc::ManagedObj<ajn::ProxyBusObject> obj);

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
	 * Callback when cloud call a method on some local interface
	 * @param member - the member (method) of the interface that was executed
	 * @param msg - the Message of the method
	 */
	void AJLocalMethodCall(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);

	/**
	 * Callback when subscribing some cloud service to local proximal network
	 * @param member - the member (method) of the interface that was executed
	 * @param msg - the Message of the method
	 */
	void AJSubscribeCloudServiceToLocal(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);

	/**
	 * Callback when unsubscribing some cloud service from local
	 * @param member - the member (method) of the interface that was executed
	 * @param msg - the Message of the method
	 */
	void AJUnsubscribeCloudServiceFromLocal(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);

    /************************************************************************/
    /*  interface implementation for IoTivity network */
    /************************************************************************/

    /************************************************************************/
	/*  interface implementation for MQTT network */
    /************************************************************************/

private:
	/**
	 * @internal
	 * Save the ProxyBusObject and all its children to the map 'proxyBusObjects'
	 * @param proxy - the ProxyBusObject that will be saved, including its children 
	 * @param busName -  the name of the bus that owns the remote BusObject
	 */
	void SaveProxyBusObject(ajn::ProxyBusObject* proxy, qcc::String& busName);

	/**
     * @internal
     * Method return handler used to process asynchronous local method call (ProximalCommEngineBusObject::AJLocalMethodCall).
     *
     * @param msg     Method return message
     * @param context Opaque context passed from method_call to method_return
     */
     void LocalMethodCallReplyHandler(ajn::Message& msg, void* context);

protected:
	/**
	 *	All ProxyBusObjects for handling incoming calls towards proximal AllJoyn service
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
	std::map<qcc::String, ajn::ProxyBusObject*> proxyBusObjects;

	/* The AllJoyn context for all ProxyBusObjects to access local services */
	AllJoynContext proxyContext;

	/**
	 *	All BusObjects for exposing cloud services to proximal AllJoyn network on behalf of cloud service providers
	 * The key is the cloud service address, e.g. weather_server_A@nane.cn, lyh@nane.cn
	 *
	 * Be noted that, this map only contains top-level BusObjects, but not their children BusOjbects. Actually, the
	 * top-level BusObjects are empty with no interfaces exposed. 
	 */
	std::map<qcc::String, CloudServiceAgentBusObject*> cloudBusObjects;

	/**
	 *	The CloudCommEngine BusObject, that is for re-marshaling the calls and forward to cloud
	 */
	qcc::ManagedObj<ajn::ProxyBusObject> cloudEngineProxyBusObject;
};

} /* namespace gateway */
} /* namespace sipe2e */

#endif /* ALLJOYNENGINEBUSOBJECT_H_ */