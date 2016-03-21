/******************************************************************************
 * Copyright AllSeen Alliance. All rights reserved.
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

#include <qcc/platform.h>
#include <qcc/Debug.h>
#include <qcc/StringSource.h>
#include <qcc/XmlElement.h>
#include <qcc/StringUtil.h>

#include <alljoyn/AllJoynStd.h>
#include <alljoyn/BusAttachment.h>
#include <ControlPanelConstants.h>
#include <NotificationConstants.h>

#include "CloudCommEngine/ProximalProxyBusObjectWrapper.h"
#include "CloudCommEngine/CloudCommEngineBusObject.h"
#include "Common/CommonUtils.h"

#define QCC_MODULE "ProximalProxyBusObjectWrapper"

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
        QCC_LogError(ER_OK, ("Top-level ProxyBusObject is not NULL"));
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

    _ProxyBusObject** proxyBusObjectChildren = new _ProxyBusObject*[numChildren];
    numChildren = proxy->GetManagedChildren(proxyBusObjectChildren, numChildren);

    for (size_t i = 0; i < numChildren; i++) {
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

String ProximalProxyBusObjectWrapper::GenerateProxyIntrospectionXml(ajn::SessionPort& wellKnownPort, bool topLevel)
{
    if (!proxy.unwrap()) {
        QCC_LogError(ER_OK, ("Top-level ProxyBusObject is not NULL"));
        return ER_OK;
    }
#ifndef NDEBUG
    QCC_DbgHLPrintf(("Entering ProximalProxyBusObjectWrapper::GenerateProxyIntrospectionXml(%i, %i)", wellKnownPort, topLevel));
#endif

    String introXml("");
    const String close = "\">\n";

    introXml += "<node name=\"";

    const String& objPath = proxy->GetPath();
    if (topLevel) {
        // if it's top level ProxyBusObject, name attribute is whole ObjPath
        introXml += proxy->GetPath() /* + close*/;
    } else {
        size_t slash = objPath.find_last_of('/');
        if (slash != String::npos) {
            introXml += objPath.substr(slash + 1, objPath.size() - slash - 1) /* + close*/;
        }
    }

    String intfsXml("");

    wellKnownPort = SESSION_PORT_ANY;

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

#ifndef NDEBUG
                QCC_DbgHLPrintf(("Iterate over the Interface '%s'", intfName));
#endif
                if (intf && intfName
                    && strcmp(intfName, org::freedesktop::DBus::InterfaceName)
                    && strcmp(intfName, org::freedesktop::DBus::Peer::InterfaceName)
                    && strcmp(intfName, org::freedesktop::DBus::Properties::InterfaceName)
                    && strcmp(intfName, org::freedesktop::DBus::Introspectable::InterfaceName)
                    && strcmp(intfName, org::allseen::Introspectable::InterfaceName)) {

                    // if the interface is "org.alljoyn.ControlPanel.ControlPanel" the port will be set to CONTROLPANELSERVICE_PORT (1000)
                    // if the interface is "org.alljoyn.Notification" the port will be set to AJ_NOTIFICATION_PRODUCER_SERVICE_PORT (1010)
                    if (!strcmp(intfName, services::cpsConsts::AJ_CONTROLPANEL_INTERFACE.c_str())) {
                        wellKnownPort = services::cpsConsts::CONTROLPANELSERVICE_PORT;
                    } else if (!strcmp(intfName, services::nsConsts::AJ_NOTIFICATION_INTERFACE_NAME.c_str())) {
                        wellKnownPort = services::nsConsts::AJ_NOTIFICATION_PRODUCER_SERVICE_PORT;
                    }

                    size_t numMembers = intf->GetMembers();
                    if (numMembers > 0) {
                        const InterfaceDescription::Member** members = new const InterfaceDescription::Member*[numMembers];
                        intf->GetMembers(members, numMembers);
                        for (size_t i = 0; i < numMembers; i++) {
                            const InterfaceDescription::Member* currMember = members[i];
#ifndef NDEBUG
                            QCC_DbgHLPrintf(("Iterate over the Member '%s' with memberType=%i", currMember->name.c_str(), currMember->memberType));
#endif
                            if (currMember && currMember->memberType == MESSAGE_SIGNAL) {
                                QStatus status = proxyBus->RegisterSignalHandler(this,
                                                                                 static_cast<MessageReceiver::SignalHandler>(&ProximalProxyBusObjectWrapper::CommonSignalHandler),
                                                                                 currMember, 0);
#ifndef NDEBUG
                                QCC_DbgHLPrintf(("Register Signal Handler for signal '%s' of interface '%s'", currMember->name.c_str(), intf->GetName()));
#endif
                                if (ER_OK != status) {
                                    QCC_LogError(status, ("Could not Register Signal Handler for ProximalServiceProxyBusObject"));
                                }
                            }
                        }
                        delete[] members;
                    }

                    String intfXml = intf->Introspect();
/*
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
   #ifndef NDEBUG
                                    QCC_DbgHLPrintf(("Register Signal Handler for signal '%s' of interface '%s'", signalEle->GetAttribute("name").c_str(), intfEle->GetAttribute("name").c_str()));
   #endif
                                    if (ER_OK != status) {
                                        QCC_LogError(status, ("Could not Register Signal Handler for ProximalServiceProxyBusObject"));
                                    }
                                }
                            }
                        }
 */

                    intfsXml += intfXml;
                    intfsXml += "\n";
                }
            }
        }
        delete[] intfs;
    }

    if (wellKnownPort != SESSION_PORT_ANY) {
        introXml += "\" port=\"";
        introXml += U32ToString(wellKnownPort);
    }
    introXml += close;
    introXml += intfsXml;

    /* Secondly we add all children */
    for (size_t i = 0; i < children.size(); i++) {
        ajn::SessionPort childPort = SESSION_PORT_ANY;
        introXml += children[i]->GenerateProxyIntrospectionXml(childPort, false);
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
#ifndef NDEBUG
    QCC_DbgHLPrintf(("Incoming Signal from member '%s' with srcPath '%s'", member->name.c_str(), srcPath));
#endif

    /* Retrieve all arguments of the signal call */
    size_t numArgs = 0;
    const MsgArg* args = 0;
    msg->GetArgs(numArgs, args);


    // Now we try to find the SignalHandlerInfo using the key BusName/ObjPath
    // Get the Bus Name of the signal sender which is saved as the service name of the ProxyBusObject at the time of creation
    const String& senderBusName = proxy->GetServiceName(); // it's the same as msg->GetSender() ?
/*
    String busNameObjPath;
    IllegalStringToObjPathString(senderBusName, busNameObjPath);
 */
    String busNameObjPath(senderBusName);
//     busNameObjPath += srcPath; // The Signal Handler Info is store with key as BusName (without ObjPath?)

    std::map<qcc::String, std::map<qcc::String, std::vector<CloudCommEngineBusObject::SignalHandlerInfo> > >::iterator itShiMap = ownerBusObject->signalHandlersInfo.find(busNameObjPath);
    if (itShiMap == ownerBusObject->signalHandlersInfo.end()) {
        // Could not find the Signal Handler Info for this signal, just ignore it
        QCC_LogError(ER_FAIL, ("No Signal Handler Info found"));
        return;
    }
    std::map<qcc::String, std::vector<CloudCommEngineBusObject::SignalHandlerInfo> >& shiMap = itShiMap->second;
    std::map<qcc::String, std::vector<CloudCommEngineBusObject::SignalHandlerInfo> >::iterator itShi = shiMap.begin();
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
/*
            String receiverAddr(peerAddr);
            receiverAddr += "/";
            receiverAddr += shi.busName;
 */
            String receiverAddr(shi.busName);
            receiverAddr += "/";
            receiverAddr += qcc::U32ToString(shi.sessionId);
//             cloudCallArgs[1].Set("s", receiverAddr.c_str());
//             cloudCallArgs[2].Set("av", numArgs, args);
//             cloudCallArgs[3].Set("x", shi.sessionId);

            // Send out the signal
            status = ownerBusObject->CloudSignalCall(peerAddr, busNameObjPath, receiverAddr, numArgs, args, shi.sessionId);
            if (ER_OK != status) {
                QCC_LogError(status, ("Error sending signal to cloud"));
            }
        }
        itShi++;
    }
}

}
}