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
#ifndef CLOUDSERVICEAGENTBUSOBJECT_H_
#define CLOUDSERVICEAGENTBUSOBJECT_H_

#include <qcc/platform.h>

#include <qcc/XmlElement.h>

#include <alljoyn/Status.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/BusObject.h>
#include <alljoyn/ProxyBusObject.h>
#include <alljoyn/InterfaceDescription.h>
#include <alljoyn/ProxyBusObject.h>

#include "Common/GatewayConstants.h"
#include "Common/GatewayStd.h"

namespace sipe2e {

namespace gateway {

class ProximalCommEngineBusObject;

/**
    *    CloudServiceAgentBusObject class
    * This is the Agent BusObject exposing services/interfaces on behalf of cloud services
    * Every Agent BusObject is created while subscribing a remote service (either cloud service
    * or service provided by devices in other proximal networks).
    * Every Agent BusObject is on its specific BusAttachment which is different from the one
    * of ProximalCommEngineBusObject instance.
    */
class CloudServiceAgentBusObject : public ajn::BusObject
{
    friend class ProximalCommEngineBusObject;

public:
    CloudServiceAgentBusObject(qcc::String const& objectPath, qcc::ManagedObj<ajn::ProxyBusObject> cloudEnginePBO, ProximalCommEngineBusObject* owner);
    virtual ~CloudServiceAgentBusObject();

public:
    /**
        * Common method handler for all methods of cloud service
        * @param member - the member (method) of the interface that was executed
        * @param msg - the Message of the method
        */
    void CommonMethodHandler(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);

public:
    /**
        *    Parse the introspection XML string and create/activate/add interfaces included to this BusObject
        * Meantime add method handler for all methods
        * @param xml - the introspection XML string describing the interfaces and children interfaces
        * @param propertyStore - fill in the propertyStore based on the about data in the XML
        */
    QStatus ParseXml(const char* xml, ajn::services::AboutPropertyStoreImpl& propertyStore);

    /**
        * Prepare the agent AllJoyn executing environment, including BusAttachment, AboutService
        * @param _bus - the BusAttachment that the Agent BusObject is registers on. if it's NULL, 
        *                           then 
        */
    QStatus PrepareAgent(AllJoynContext* _context, ajn::services::AboutPropertyStoreImpl* propertyStore);

    /**
        * Announce the BusObject and all its children
        */
    QStatus Announce();

    /**
        * Clean up the AllJoyn execution context
        * @param topLevel - true indicates that this object is top level parent
        */
    QStatus Cleanup(bool topLevel = true);
private:
    /**
        * @internal
        *    Parse the node in the introspection XML and create/activate/add interfaces included to this BusObject
        * Meantime add method handler for all methods
        * @param root - the node xml root element of the introspection XML string
        * @param bus - the BusAttachment that the agent BusObject will be registered in
        */
    QStatus ParseNode(const qcc::XmlElement* root);

    /**
        * @internal
        *    Parse a single interface in the introspection XML and create/activate/add interfaces included to this BusObject
        * Meantime add method handler for all methods
        * @param root - the interface root element of the introspection XML string
        * @param bus - the BusAttachment that the agent BusObject will be registered in
        */
    QStatus ParseInterface(const qcc::XmlElement* root);


    /**
     * @internal
     * Method return handler used to process asynchronous cloud method call (CloudServiceAgentBusObject::CommonMethodHandler).
     *
     * @param msg     Method return message
     * @param context Opaque context passed from method_call to method_return
     */
     void CloudMethodCallReplyHandler(ajn::Message& msg, void* context);

protected:
    /* The AllJoyn execution environment */
    AllJoynContext context;
    /* The supported interfaces' names */
    std::vector<qcc::String> interfaces;
    /**
        *    The CloudCommEngine BusObject, that is for re-marshaling the calls and forward to cloud
        */
    qcc::ManagedObj<ajn::ProxyBusObject> cloudEngineProxyBusObject;
    /* All the children BusObjects */
    std::vector<CloudServiceAgentBusObject*> children;
    /* The parent BusObject that owns this agent */
    ProximalCommEngineBusObject* ownerBusObject;

};

} // namespace gateway

} // namespace sipe2e

#endif // CLOUDSERVICEAGENTBUSOBJECT_H_

