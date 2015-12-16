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
#include <qcc/Debug.h>
#include <qcc/StringSource.h>
#include <qcc/XmlElement.h>
#include <qcc/StringUtil.h>

#include <alljoyn/AllJoynStd.h>
#include <alljoyn/BusAttachment.h>

#include "CloudCommEngine/ProximalProxyBusObjectWrapper.h"
#include "CloudCommEngine/CloudCommEngineBusObject.h"

#define QCC_MODULE "SIPE2E"

using namespace ajn;
using namespace qcc;

namespace sipe2e {
namespace gateway {

ProximalProxyBusObjectWrapper::ProximalProxyBusObjectWrapper()
    : proxyBus(NULL), ownerBusObject(NULL)
{

}

ProximalProxyBusObjectWrapper::ProximalProxyBusObjectWrapper(_ProxyBusObject _proxy, BusAttachment* bus, CloudCommEngineBusObject* owner)
    : proxy(_proxy), proxyBus(bus), ownerBusObject(owner)
{

}

ProximalProxyBusObjectWrapper::~ProximalProxyBusObjectWrapper()
{
    if (proxyBus) {
        proxyBus->UnregisterAllHandlers(this);
        proxyBus = NULL;
    }
    ownerBusObject = NULL;
}

QStatus ProximalProxyBusObjectWrapper::IntrospectProxyChildren()
{
    if (!proxy.unwrap()) {
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

    _ProxyBusObject** proxyBusObjectChildren = new _ProxyBusObject *[numChildren];
    numChildren = proxy->GetManagedChildren(proxyBusObjectChildren, numChildren);

    for (size_t i = 0; i < numChildren; i++) {
#ifndef NDEBUG
        String const& objectPath = (*proxyBusObjectChildren[i])->GetPath();
        QCC_DbgPrintf(("ObjectPath is: %s", objectPath.c_str()));
#endif
        _ProximalProxyBusObjectWrapper proxyWrapper(*proxyBusObjectChildren[i], proxyBus, ownerBusObject);
        children.push_back(proxyWrapper);
        status = proxyWrapper->IntrospectProxyChildren();
        delete proxyBusObjectChildren[i]; //  delete this? please see the implementation of ProxyBusObject::GetManagedChildren()
        if (status != ER_OK) {
            QCC_LogError(status, ("Could not introspect RemoteObjectChild"));
            delete[] proxyBusObjectChildren;
            return status;
        }
    }
    delete[] proxyBusObjectChildren;
    return ER_OK;
}

String ProximalProxyBusObjectWrapper::GenerateProxyIntrospectionXml()
{
    if (!proxy.unwrap()) {
        QCC_DbgHLPrintf(("Top-level ProxyBusObject is not NULL"));
        return ER_OK;
    }
    String introXml("");
    const String close = "\">\n";

    introXml += "<node name=\"";
    introXml += proxy->GetPath() + close;

    /* First we add all interface description */
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
                                    QStatus status = proxyBus->RegisterSignalHandler(this,
                                        static_cast<MessageReceiver::SignalHandler>(&ProximalProxyBusObjectWrapper::CommonSignalHandler),
                                        intf->GetMember(signalEle->GetAttribute("name").c_str()),
                                        0);
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

    /* Secondly we add all children */
    for (size_t i = 0; i < children.size(); i++) {
        // This conversion is dangerous!
        String childObjName = children[i]->proxy->GetPath();
        size_t pos = childObjName.find_last_of('/');
        if (String::npos != pos) {
            childObjName = childObjName.substr(pos + 1, childObjName.length() - pos - 1);
            introXml += children[i]->GenerateProxyIntrospectionXml();
        }
    }

    introXml += "</node>\n";
    return introXml;
}

void ProximalProxyBusObjectWrapper::CommonSignalHandler(const InterfaceDescription::Member* member, const char* srcPath, Message& msg)
{
    QStatus status = ER_OK;

    if (!ownerBusObject) {
        QCC_LogError(ER_FAIL, ("No CloudCommEngine present"));
        return;
    }

    /* Retrieve all arguments of the signal call */
    size_t  numArgs = 0;
    const MsgArg* args = 0;
    msg->GetArgs(numArgs, args);


    // Now we try to find the SignalHandlerInfo using the key BusName/ObjPath
    // Get the Bus Name of the signal sender which is saved as the service name of the ProxyBusObject at the time of creation
    const String& senderBusName = proxy->GetServiceName(); // it's the same as msg->GetSender() ?
    String busNameObjPath(senderBusName);
//     busNameObjPath += srcPath; // The Signal Handler Info is store with key as BusName (without ObjPath?)

    std::map<qcc::String, std::map<qcc::String, std::vector<CloudCommEngineBusObject::SignalHandlerInfo>>>::iterator itShiMap = ownerBusObject->signalHandlersInfo.find(busNameObjPath);
    if (itShiMap == ownerBusObject->signalHandlersInfo.end()) {
        // Could not find the Signal Handler Info for this signal, just ignore it
        QCC_LogError(ER_FAIL, ("No Signal Handler Info found"));
        return;
    }
    std::map<qcc::String, std::vector<CloudCommEngineBusObject::SignalHandlerInfo>>& shiMap = itShiMap->second;
    std::map<qcc::String, std::vector<CloudCommEngineBusObject::SignalHandlerInfo>>::iterator itShi = shiMap.begin();
    while (itShi != shiMap.end()) {
        const String& peerAddr = itShi->first;
        std::vector<CloudCommEngineBusObject::SignalHandlerInfo>& shiVec = itShi->second;
        // send signals to all possible interested peers
        for (size_t i = 0; i < shiVec.size(); i++) {
            CloudCommEngineBusObject::SignalHandlerInfo& shi = shiVec[i];
//             MsgArg cloudCallArgs[4];
            busNameObjPath += srcPath;
            busNameObjPath += "/";
            busNameObjPath += member->iface->GetName();
            busNameObjPath += "/";
            busNameObjPath += member->name;
//             cloudCallArgs[0].Set("s", busNameObjPath.c_str());
            String receiverAddr(peerAddr);
            receiverAddr += "/";
            receiverAddr += shi.busName;
            receiverAddr += "/";
            receiverAddr += qcc::U32ToString(shi.sessionId);
//             cloudCallArgs[1].Set("s", receiverAddr.c_str());
//             cloudCallArgs[2].Set("av", numArgs, args);
//             cloudCallArgs[3].Set("x", shi.sessionId);

            // Send out the signal
            status = ownerBusObject->CloudSignalCall(busNameObjPath, receiverAddr, numArgs, args, shi.sessionId);
            if (ER_OK != status) {
                QCC_LogError(status, ("Error sending signal to cloud"));
            }
       }
        itShi++;
    }
}

}
}