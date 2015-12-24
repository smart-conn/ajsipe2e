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
#include <qcc/platform.h>

#include <qcc/StringSource.h>
#include <qcc/StringUtil.h>

#include "Common/CommonUtils.h"
#include "Common/GatewayStd.h"
#include "CloudCommEngine/CloudServiceAgentBusObject.h"
#include "CloudCommEngine/CloudCommEngineBusObject.h"

#include <alljoyn/AllJoynStd.h>
#include <alljoyn/DBusStd.h>
#include <SignatureUtils.h>
#include <BusUtil.h>


#define QCC_MODULE "CloudServiceAgentBusObject"


using namespace ajn;
using namespace qcc;
using namespace std;


namespace sipe2e {

namespace gateway {

const String& GetSecureAnnotation(const XmlElement* elem)
{
    vector<XmlElement*>::const_iterator ifIt = elem->GetChildren().begin();
    while (ifIt != elem->GetChildren().end()) {
        const XmlElement* ifChildElem = *ifIt++;
        String ifChildName = ifChildElem->GetName();
        if ((ifChildName == "annotation") && (ifChildElem->GetAttribute("name") == org::alljoyn::Bus::Secure)) {
            return ifChildElem->GetAttribute("value");
        }
    }
    return String::Empty;
}

bool GetDescription(const XmlElement* elem, String& description)
{
    vector<XmlElement*>::const_iterator it = elem->GetChildren().begin();
    for (; it != elem->GetChildren().end(); it++) {
        if ((*it)->GetName().compare("description")) {
            description = (*it)->GetContent();
            return true;
        }
    }

    return false;
}


CloudServiceAgentBusObject::CloudServiceAgentBusObject(::String const& objectPath, 
                                                       const String& remoteAccount, const String& remoteBusName, 
                                                       CloudCommEngineBusObject* owner)
    : BusObject(objectPath.c_str()), 
    aboutDataHandler(this),
    sp(SESSION_PORT_ANY),
    remoteAccount(remoteAccount), remoteBusName(remoteBusName),
    ownerBusObject(owner)
{

}

CloudServiceAgentBusObject::~CloudServiceAgentBusObject()
{

}

void CloudServiceAgentBusObject::CommonMethodHandler(const InterfaceDescription::Member* member, Message& msg)
{
    QStatus status = ER_OK;

    /* Retrieve all arguments of the method call */
    size_t  numArgs = 0;
    const MsgArg* args = 0;
    msg->GetArgs(numArgs, args);


    /* Parsing all arguments and re-marshaling them to send out to CloudCommEngine */
    MsgArg cloudCallArgs[3];

    /**
     *  The first arg: thierry_luo@nane.cn/BusName/ObjectPath/InterfaceName/MethodName
     *  the first two parts of the path will be converted to ascii string since there are maybe illegal
     *  characters in account name or BusName, like '@', or any characters that are not numeric
     *  or alphabetic
     */
/*
    String calledAddr(this->GetPath());
    calledAddr.erase(0, 1); // since the first character is definitely '/'
    if (calledAddr[calledAddr.length() - 1] != '/') {
        calledAddr += "/";
    }
    calledAddr += member->iface->GetName();
    calledAddr += "/";
    calledAddr += member->name;
*/

    // calledAddress will be like: BusName/ObjPath/InterfaceName/MethodName
    String calledAddr = remoteBusName + this->GetPath();
    calledAddr += "/";
    calledAddr += member->iface->GetName();
    calledAddr += "/";
    calledAddr += member->name;

    /**
      * Now we're ready to send out the cloud call by calling CloudCommEngine::CloudMethodCall
      * Be noted that, since all calls to this BusObject are handled in this common handler, from
      * performance's perspective, in CloudMethodCall() implentation, multi-thread feature should
      * be implemented.
      */
    status = ownerBusObject->CloudMethodCall(gwConsts::customheader::RPC_MSG_TYPE_METHOD_CALL, remoteAccount, calledAddr, numArgs, args, msg->GetSessionId(), this, msg);
    CHECK_STATUS_AND_REPLY("Error making the cloud method call");
}


void CloudServiceAgentBusObject::GetProp(const InterfaceDescription::Member* member, Message& msg)
{
    QStatus status = ER_OK;

    // The method call 'Get' consists of two arguments and one return argument
    /* Retrieve all arguments of the method call */
    size_t  numArgs = 0;
    const MsgArg* args = 0;
    msg->GetArgs(numArgs, args);

    // The first arg: thierry_luo@nane.cn/BusName/ObjectPath/InterfaceName/MethodName
/*
    String calledAddr(this->GetPath());
    calledAddr.erase(0, 1); // since the first character is definitely '/'
    if (calledAddr[calledAddr.length() - 1] != '/') {
        calledAddr += "/";
    }
    calledAddr += args[0].v_string.str;
    calledAddr += "/";
    calledAddr += "Get";
*/

    // calledAddress will be like: BusName/ObjPath/InterfaceName/MethodName
    String calledAddr = remoteBusName + this->GetPath();
    calledAddr += "/";
    calledAddr += args[0].v_string.str;
    calledAddr += "/";
    calledAddr += "Get";

    status = ownerBusObject->CloudMethodCall(gwConsts::customheader::RPC_MSG_TYPE_PROPERTY_CALL, remoteAccount, calledAddr, numArgs, args, msg->GetSessionId(), this, msg);

    CHECK_STATUS_AND_REPLY("Error making the cloud method call");
}

void CloudServiceAgentBusObject::SetProp(const InterfaceDescription::Member* member, Message& msg)
{
    QStatus status = ER_OK;

    // The method call 'Get' consists of two arguments and one return argument
    /* Retrieve all arguments of the method call */
    size_t  numArgs = 0;
    const MsgArg* args = 0;
    msg->GetArgs(numArgs, args);

    // The first arg: thierry_luo@nane.cn/BusName/ObjectPath/InterfaceName/MethodName
/*
    String calledAddr(this->GetPath());
    calledAddr.erase(0, 1); // since the first character is definitely '/'
    if (calledAddr[calledAddr.length() - 1] != '/') {
        calledAddr += "/";
    }
    calledAddr += args[0].v_string.str;
    calledAddr += "/";
    calledAddr += "Set";
*/
    // calledAddress will be like: BusName/ObjPath/InterfaceName/MethodName
    String calledAddr = remoteBusName + this->GetPath();
    calledAddr += "/";
    calledAddr += args[0].v_string.str;
    calledAddr += "/";
    calledAddr += "Set";



    status = ownerBusObject->CloudMethodCall(gwConsts::customheader::RPC_MSG_TYPE_PROPERTY_CALL, remoteAccount, calledAddr, numArgs, args, msg->GetSessionId(), this, msg);

    CHECK_STATUS_AND_REPLY("Error making the cloud method call");
}

void CloudServiceAgentBusObject::GetAllProps(const InterfaceDescription::Member* member, Message& msg)
{
    QStatus status = ER_OK;

    // The method call 'Get' consists of two arguments and one return argument
    /* Retrieve all arguments of the method call */
    size_t  numArgs = 0;
    const MsgArg* args = 0;
    msg->GetArgs(numArgs, args);

    // The first arg: thierry_luo@nane.cn/BusName/ObjectPath/InterfaceName/MethodName
/*
    String calledAddr(this->GetPath());
    calledAddr.erase(0, 1); // since the first character is definitely '/'
    if (calledAddr[calledAddr.length() - 1] != '/') {
        calledAddr += "/";
    }
    calledAddr += args[0].v_string.str;
    calledAddr += "/";
    calledAddr += "GetAll";
*/
    // calledAddress will be like: BusName/ObjPath/InterfaceName/MethodName
    String calledAddr = remoteBusName + this->GetPath();
    calledAddr += "/";
    calledAddr += args[0].v_string.str;
    calledAddr += "/";
    calledAddr += "GetAll";


    status = ownerBusObject->CloudMethodCall(gwConsts::customheader::RPC_MSG_TYPE_PROPERTY_CALL, remoteAccount, calledAddr, numArgs, args, msg->GetSessionId(), this, msg);

    CHECK_STATUS_AND_REPLY("Error making the cloud method call");
}

QStatus CloudServiceAgentBusObject::ParseXml(const char* xml, BusAttachment* proxyBus)
{
    if (!proxyBus) {
        return ER_BUS_NO_TRANSPORTS;
    }
    QStatus status = ER_OK;
    StringSource source(xml);

    XmlParseContext pc(source);
    status = XmlElement::Parse(pc);
    if (status == ER_OK) {
        const XmlElement* root = pc.GetRoot();
        if (root) {
            if (root->GetName() == "service") {
                /* this agent BusObject is top-level parent for all following children */
                vector<XmlElement*>::const_iterator it = root->GetChildren().begin();
                for (;it != root->GetChildren().end();it++) {
                    const XmlElement* elem = *it;
                    const String& elemName = elem->GetName();
                    if (elemName == "node") {
                        const String& relativePath = elem->GetAttribute("name");
                        String childObjPath = this->GetPath();
/*
                        if (childObjPath.size() > 0 && childObjPath[childObjPath.size() - 1] != '/' && relativePath[0] != '/') {
                            childObjPath += '/';
                        }
*/
                        if (childObjPath == "/") {
                            childObjPath = relativePath;
                        } else {
                            if (childObjPath.size() > 1 && relativePath[0] != '/') {
                                childObjPath += '/';
                            }
                            childObjPath += relativePath;
                        }
                        if (!relativePath.empty() && IsLegalObjectPath(childObjPath.c_str())) {
                            CloudServiceAgentBusObject* newChild = new CloudServiceAgentBusObject(childObjPath, remoteAccount, remoteBusName, ownerBusObject);
                            newChild->context = context;

                            ajn::SessionPort sPort = SESSION_PORT_ANY;
                            const String& portStr = elem->GetAttribute("port");
                            if (!portStr.empty()) {
                                sPort = StringToU32(portStr, 10, SESSION_PORT_ANY);
                            }
                            if (sPort != SESSION_PORT_ANY) {
                                sp = sPort;
                                newChild->sp = sPort;
                            }
                            status = newChild->ParseNode(elem, proxyBus);
                            if (ER_OK != status) {
                                QCC_LogError(status, ("Error while parsing Node in the introspection XML"));
                                return status;
                            }
                            status = newChild->PrepareAgent(&context, String(""));
                            if (ER_OK != status) {
                                QCC_LogError(status, ("Error while preparing the Agent of child BusObject"));
                                return status;
                            }
                            children.push_back(newChild);
                        } else {
                            status = ER_FAIL;
                            QCC_LogError(status, ("Object path not correct"));
                            return status;
                        }

                    } else if (elemName == "interface") {
                        status = ParseInterface(elem, proxyBus);
                        if (ER_BUS_IFACE_ALREADY_EXISTS == status) {
                            status = ER_OK;
                        }
                        if (ER_OK != status) {
                            QCC_LogError(status, ("Error while parsing Interface in the introspection XML"));
                            return status;
                        }
                    } else if (elemName == "about") {
                        /* Fill in the about data */
/*
                        vector<XmlElement*>::const_iterator itAboutData = elem->GetChildren().begin();
                        for (;itAboutData != elem->GetChildren().end(); itAboutData++) {
                            const XmlElement* elemAboutData = *itAboutData;
                            const String& aboutKey = elemAboutData->GetAttribute("key");
                            const String& aboutValue = elemAboutData->GetAttribute("value");
                            FillPropertyStore(&propertyStore, aboutKey, aboutValue);
                        }
*/
                        const std::vector<XmlElement*>& aboutDataEles = elem->GetChildren();
                        if (aboutDataEles.size() == 0) {
                            continue;
                        }
                        context.aboutData = new ajn::MsgArg();
                        status = XmlToArg(aboutDataEles[0], *context.aboutData);
                        if (ER_OK != status) {
                            QCC_LogError(status, ("Error while parsing the announced about data"));
                            return status;
                        }
                        // Traverse the aboutData, if some fields are missing, then try to add them
                        /*
                            Field Name          	Required	Announced	Localized	Data type	        Description
                            AppId	                        yes         yes             no                ay	The globally unique id for the application.
                            DefaultLanguage 	    yes         yes             no	             s      The default language supported by the device. IETF langauge tags specified by RFC 5646.
                            DeviceName	            no          yes             yes	             s      If Config service exist on the device, About instance populates the value as DeviceName in Config; If there is not Config, it can be set by the app. Device Name is optional for a third party apps but required for system apps. Versions of AllJoyn older than 14.12 this field was required. If working with older applications this field may be required.
                            DeviceId	                    yes         yes             no                s     A string with value generated using platform specific means.
                            AppName	                    yes         yes             yes               s     The application name assigned by the app manufacturer
                            Manufacturer	            yes         yes             yes	              s     The manufacturer's name.
                            ModelNumber	            yes         yes             no                 s      The app model number
                            SupportedLanguages  yes         no              no                  as      A list of supported languages by the application
                            Description                 yes         no              yes                 s       Detailed description provided by the application.
                            DateOfManufacture   no          no              no                   s      The date of manufacture. using format YYYY-MM-DD. (Known as XML DateTime Format)
                            SoftwareVersion         yes         no              no                   s      The software version of the manufacturer's software
                            AJSoftwareVersion      yes         no              no                  s      The current version of the AllJoyn SDK utilized by the application.
                            HardwareVersion        no          no              no                   s       The device hardware version.
                            SupportUrl                   no          no              no                   s      The support URL.
                        */
                        size_t numAboutEntries = 0;
                        MsgArg* aboutEntries = NULL;
                        status = context.aboutData->Get("a{sv}", &numAboutEntries, &aboutEntries);
                        if (ER_OK != status) {
                            QCC_LogError(status, ("Error while trying to get About Data entries"));
                            return status;
                        }

                        // Add 4 more required fields: SupportedLanguages,Description,SoftwareVersion,AJSoftwareVersion
                        MsgArg* newAboutEntries = new MsgArg[numAboutEntries + 4];
                        for (size_t entryIndx = 0; entryIndx < numAboutEntries; entryIndx++) {
                            newAboutEntries[entryIndx] = aboutEntries[entryIndx];
                        }
                        context.aboutData->Clear();

                        // SupportedLanguages: "as"
                        const char** supportedLangs = new const char*[2];
                        supportedLangs[0] = "en";
                        supportedLangs[1] = "zh";
                        MsgArg* entryVal = new MsgArg("as", 2, supportedLangs);
                        newAboutEntries[numAboutEntries].Set("{sv}", AboutData::SUPPORTED_LANGUAGES, entryVal);

                        // Description
                        entryVal = new MsgArg("s", "Description: not announced");
                        newAboutEntries[numAboutEntries + 1].Set("{sv}", AboutData::DESCRIPTION, entryVal);

                        // SoftwareVersion
                        entryVal = new MsgArg("s", "SoftwareVersion: not announced");
                        newAboutEntries[numAboutEntries + 2].Set("{sv}", AboutData::SOFTWARE_VERSION, entryVal);

                        // AJSoftwareVersion
                        entryVal = new MsgArg("s", "AJSoftwareVersion: not announced");
                        newAboutEntries[numAboutEntries + 3].Set("{sv}", AboutData::AJ_SOFTWARE_VERSION, entryVal);

                        context.aboutData->Set("a{sv}", numAboutEntries + 4, newAboutEntries);

                    }
                }
            } else if (root->GetName() == "interface") {
                status = ParseInterface(root, proxyBus);
                if (ER_BUS_IFACE_ALREADY_EXISTS == status) {
                    status = ER_OK;
                }
            } else if (root->GetName() == "node") {
                return ParseNode(root, proxyBus);
            }
        }
    }
    if (ER_OK != status) {
        return status;
    }

    // Add the interface SIPE2E_CLOUDSERVICEAGENT_ALLJOYNENGINE_INTERFACE
    InterfaceDescription* agentIntf = NULL;
    status = proxyBus->CreateInterface(gwConsts::SIPE2E_CLOUDSERVICEAGENT_ALLJOYNENGINE_INTERFACE.c_str(), agentIntf, false);
    if (ER_BUS_IFACE_ALREADY_EXISTS == status) {
        status = ER_OK;
        const InterfaceDescription* agentIntfExist = proxyBus->GetInterface(gwConsts::SIPE2E_CLOUDSERVICEAGENT_ALLJOYNENGINE_INTERFACE.c_str());
        this->AddInterface(*agentIntfExist, AnnounceFlag::ANNOUNCED);
        interfaces.push_back(agentIntfExist->GetName());
    } else if (ER_OK == status) {
        status = agentIntf->AddProperty("Version", "q", PROP_ACCESS_READ);
        if (ER_OK != status) {
            return status;
        }
        agentIntf->Activate();
        this->AddInterface(*agentIntf, AnnounceFlag::ANNOUNCED);
        interfaces.push_back(agentIntf->GetName());
    } else {
        return status;
    }

    return status;
}

QStatus CloudServiceAgentBusObject::ParseNode(const XmlElement* root, BusAttachment* proxyBus)
{
    if (!proxyBus) {
        return ER_BUS_NO_TRANSPORTS;
    }
    QStatus status = ER_OK;

    assert(root->GetName() == "node");

/*
    if (GetSecureAnnotation(root) == "true") {
        this->isSecure = true;
    }
*/

    /* Iterate over <interface> and <node> elements */
    const vector<XmlElement*>& rootChildren = root->GetChildren();
    vector<XmlElement*>::const_iterator it = rootChildren.begin();
    while ((ER_OK == status) && (it != rootChildren.end())) {
        const XmlElement* elem = *it++;
        const String& elemName = elem->GetName();
        if (elemName == "interface") {
            status = ParseInterface(elem, proxyBus);
            if (ER_BUS_IFACE_ALREADY_EXISTS == status) {
                status = ER_OK;
            }
        } else if (elemName == "node") {
            const String& relativePath = elem->GetAttribute("name");
            String childObjPath = this->GetPath();
            if (childObjPath.size() > 1 && relativePath[0] != '/') {
                childObjPath += '/';
            }
            childObjPath += relativePath;
            if (!relativePath.empty() && IsLegalObjectPath(childObjPath.c_str())) {
                CloudServiceAgentBusObject* newChild = new CloudServiceAgentBusObject(childObjPath, remoteAccount, remoteBusName, ownerBusObject);
                newChild->context = context;

                if (sp != SESSION_PORT_ANY) {
                    newChild->sp = sp;
                }
                status = newChild->ParseNode(elem, proxyBus);
                if (ER_OK != status) {
                    break;
                }
                status = newChild->PrepareAgent(&context, String(""));
                if (ER_OK != status) {
                    QCC_LogError(status, ("Error while preparing the Agent of child BusObject"));
                    return status;
                }
                children.push_back(newChild);
            } else {
                status = ER_FAIL;
                break;
            }
        }
    }
    if (ER_OK != status) {
        return status;
    }

    // Add the interface SIPE2E_CLOUDSERVICEAGENT_ALLJOYNENGINE_INTERFACE
    InterfaceDescription* agentIntf = NULL;
    status = proxyBus->CreateInterface(gwConsts::SIPE2E_CLOUDSERVICEAGENT_ALLJOYNENGINE_INTERFACE.c_str(), agentIntf, false);
    if (ER_BUS_IFACE_ALREADY_EXISTS == status) {
        status = ER_OK;
        const InterfaceDescription* agentIntfExist = proxyBus->GetInterface(gwConsts::SIPE2E_CLOUDSERVICEAGENT_ALLJOYNENGINE_INTERFACE.c_str());
        this->AddInterface(*agentIntfExist, AnnounceFlag::ANNOUNCED);
        interfaces.push_back(agentIntfExist->GetName());
    } else if (ER_OK == status) {
        status = agentIntf->AddProperty("Version", "q", PROP_ACCESS_READ);
        if (ER_OK != status) {
            return status;
        }
        agentIntf->Activate();
        this->AddInterface(*agentIntf, AnnounceFlag::ANNOUNCED);
        interfaces.push_back(agentIntf->GetName());
    } else {
        return status;
    }

    return status;
}

QStatus CloudServiceAgentBusObject::ParseInterface(const XmlElement* root, BusAttachment* proxyBus)
{
    if (!proxyBus) {
        return ER_BUS_NO_TRANSPORTS;
    }
    QStatus status = ER_OK;
    InterfaceSecurityPolicy secPolicy;

    assert(root->GetName() == "interface");

    String ifName = root->GetAttribute("name");
    if (!IsLegalInterfaceName(ifName.c_str())) {
        status = ER_BUS_BAD_INTERFACE_NAME;
        // log error
        return status;
    }

    /*
     * Due to a bug in AllJoyn 14.06 and previous, we need to ignore
     * the introspected versions of the standard D-Bus interfaces.
     * This will allow a maximal level of interoperability between
     * this code and 14.06.  This hack should be removed when
     * interface evolution is better supported.
     */
    if ((ifName == org::freedesktop::DBus::InterfaceName) ||
        (ifName == org::freedesktop::DBus::Peer::InterfaceName) ||
        (ifName == org::freedesktop::DBus::Properties::InterfaceName) ||
        (ifName == org::freedesktop::DBus::Introspectable::InterfaceName) ||
        (ifName == org::allseen::Introspectable::InterfaceName)) {
            return ER_OK;
    }

    /*
     * Security on an interface can be "true", "inherit", or "off"
     * Security is implicitly off on the standard DBus interfaces.
     */
    String sec = sipe2e::gateway::GetSecureAnnotation(root);
    if (sec == "true") {
        secPolicy = AJ_IFC_SECURITY_REQUIRED;
    } else if ((sec == "off") || (ifName.find(org::freedesktop::DBus::InterfaceName) == 0)) {
        secPolicy = AJ_IFC_SECURITY_OFF;
    } else {
        if ((sec != String::Empty) && (sec != "inherit")) {
            QCC_LogError(ER_WARNING, ("Unknown annotation %s is defaulting to 'inherit'. Valid values: 'true', 'inherit', or 'off'.", org::alljoyn::Bus::Secure));
        }
        secPolicy = AJ_IFC_SECURITY_INHERIT;
    }

    /* Create a new interface */
//     InterfaceDescription intf(ifName.c_str(), secPolicy);
    InterfaceDescription* intf = NULL;
    status = proxyBus->CreateInterface(ifName.c_str(), intf, secPolicy);
    if (ER_OK != status) {
        // log error
        return status;
    }


    /* Iterate over <method>, <signal> and <property> elements */
    vector<XmlElement*>::const_iterator ifIt = root->GetChildren().begin();
    while ((ER_OK == status) && (ifIt != root->GetChildren().end())) {
        const XmlElement* ifChildElem = *ifIt++;
        const String& ifChildName = ifChildElem->GetName();
        const String& memberName = ifChildElem->GetAttribute("name");
        if ((ifChildName == "method") || (ifChildName == "signal")) {
            if (IsLegalMemberName(memberName.c_str())) {

                bool isMethod = (ifChildName == "method");
                bool isSignal = (ifChildName == "signal");
                bool isFirstArg = true;
                String inSig;
                String outSig;
                String argNames;
                bool isArgNamesEmpty = true;
                map<String, String> annotations;
                bool isSessionlessSignal = false;

                /* Iterate over member children */
                const vector<XmlElement*>& argChildren = ifChildElem->GetChildren();
                vector<XmlElement*>::const_iterator argIt = argChildren.begin();
                map<String, String> argDescriptions;
                String memberDescription;
                while ((ER_OK == status) && (argIt != argChildren.end())) {
                    const XmlElement* argElem = *argIt++;
                    if (argElem->GetName() == "arg") {
                        if (!isFirstArg) {
                            argNames += ',';
                        }
                        isFirstArg = false;
                        const String& typeAtt = argElem->GetAttribute("type");

                        if (typeAtt.empty()) {
                            status = ER_BUS_BAD_XML;
                            QCC_LogError(status, ("Malformed <arg> tag (bad attributes)"));
                            break;
                        }

                        const String& nameAtt = argElem->GetAttribute("name");
                        if (!nameAtt.empty()) {
                            isArgNamesEmpty = false;
                            argNames += nameAtt;

                            String description;
                            if (sipe2e::gateway::GetDescription(argElem, description)) {
                                argDescriptions.insert(pair<String, String>(nameAtt, description));
                            }
                        }

                        if (isSignal || (argElem->GetAttribute("direction") == "in")) {
                            inSig += typeAtt;
                        } else {
                            outSig += typeAtt;
                        }
                    } else if (argElem->GetName() == "annotation") {
                        const String& nameAtt = argElem->GetAttribute("name");
                        const String& valueAtt = argElem->GetAttribute("value");
                        annotations[nameAtt] = valueAtt;
                    } else if (argElem->GetName() == "description") {
                        memberDescription = argElem->GetContent();
                    }

                }
                if (isSignal) {
                    const String& sessionlessStr = ifChildElem->GetAttribute("sessionless");
                    isSessionlessSignal = (sessionlessStr == "true");
                }

                /* Add the member */
                if ((ER_OK == status) && (isMethod || isSignal)) {
                    status = intf->AddMember(isMethod ? MESSAGE_METHOD_CALL : MESSAGE_SIGNAL,
                        memberName.c_str(),
                        inSig.c_str(),
                        outSig.c_str(),
                        isArgNamesEmpty ? NULL : argNames.c_str());

                    for (map<String, String>::const_iterator it = annotations.begin(); it != annotations.end(); ++it) {
                        intf->AddMemberAnnotation(memberName.c_str(), it->first, it->second);
                    }

                    if (!memberDescription.empty()) {
                        intf->SetMemberDescription(memberName.c_str(), memberDescription.c_str(), isSessionlessSignal);
                    }

                    for (map<String, String>::const_iterator it = argDescriptions.begin(); it != argDescriptions.end(); it++) {
                        intf->SetArgDescription(memberName.c_str(), it->first.c_str(), it->second.c_str());
                    }
                }
            } else {
                status = ER_BUS_BAD_MEMBER_NAME;
                QCC_LogError(status, ("Illegal member name \"%s\" introspection data", memberName.c_str()));
            }
        } else if (ifChildName == "property") {
            const String& sig = ifChildElem->GetAttribute("type");
            const String& accessStr = ifChildElem->GetAttribute("access");
            if (!SignatureUtils::IsCompleteType(sig.c_str())) {
                status = ER_BUS_BAD_SIGNATURE;
                QCC_LogError(status, ("Invalid signature for property %s in introspection data", memberName.c_str()));
            } else if (memberName.empty()) {
                status = ER_BUS_BAD_BUS_NAME;
                QCC_LogError(status, ("Invalid name attribute for property in introspection data from"));
            } else {
                uint8_t access = 0;
                if (accessStr == "read") {
                    access = PROP_ACCESS_READ;
                }
                if (accessStr == "write") {
                    access = PROP_ACCESS_WRITE;
                }
                if (accessStr == "readwrite") {
                    access = PROP_ACCESS_RW;
                }
                status = intf->AddProperty(memberName.c_str(), sig.c_str(), access);

                // add Property annotations
                const vector<XmlElement*>& argChildren = ifChildElem->GetChildren();
                vector<XmlElement*>::const_iterator argIt = argChildren.begin();
                while ((ER_OK == status) && (argIt != argChildren.end())) {
                    const XmlElement* argElem = *argIt++;
                    status = intf->AddPropertyAnnotation(memberName, argElem->GetAttribute("name"), argElem->GetAttribute("value"));
                }

                String description;
                if (sipe2e::gateway::GetDescription(ifChildElem, description)) {
                    intf->SetPropertyDescription(memberName.c_str(), description.c_str());
                }
            }
        } else if (ifChildName == "annotation") {
            status = intf->AddAnnotation(ifChildElem->GetAttribute("name"), ifChildElem->GetAttribute("value"));
        } else if (ifChildName == "description") {
            String const& description = ifChildElem->GetContent();
            String const& language = ifChildElem->GetAttribute("language");
            intf->SetDescriptionLanguage(language.c_str());
            intf->SetDescription(description.c_str());
        } else {
            status = ER_FAIL;
            QCC_LogError(status, ("Unknown element \"%s\" found in introspection data", ifChildName.c_str()));
            break;
        }
    }
    /* Add the interface with all its methods, signals and properties */
    if (ER_OK == status) {
        /* Activate the interface and add it to the interface map for later usage */
        intf->Activate();
        this->AddInterface(*intf, AnnounceFlag::ANNOUNCED);
        interfaces.push_back(intf->GetName());

        /* Add method handler for all methods */
        if (intf) {
            size_t memberNum = intf->GetMembers();
            if (memberNum > 0) {
                const InterfaceDescription::Member** intfMembers = new InterfaceDescription::Member const*[memberNum];
                intf->GetMembers(intfMembers, memberNum);
                for (size_t i = 0; i < memberNum; i++) {
                    const InterfaceDescription::Member* currMember = intfMembers[i];
                    if (currMember) {
                        if (currMember->memberType == MESSAGE_METHOD_CALL) {
                            status = this->AddMethodHandler(currMember, static_cast<MessageReceiver::MethodHandler>(&CloudServiceAgentBusObject::CommonMethodHandler), NULL);
                            if (ER_OK != status) {
                                // log error
                                break;
                            } 
                        } else if (currMember->memberType == MESSAGE_SIGNAL) {
                            // not implemented
                        }
                    }
                }

                delete[] intfMembers;
            }
        }

    }

    return status;
}


QStatus CloudServiceAgentBusObject::PrepareAgent(AllJoynContext* _context, const qcc::String& serviceIntrospectionXml)
{
    QStatus status = ER_OK;

    if (NULL == _context) {
        /* If it's NULL then this Agent BusObject is the top level parent object, and we should prepare AllJoyn context for it */
        /* Prepare the BusAttachment */
        context.bus = NULL;
        context.aboutData = NULL;
        context.busListener = NULL;
        context.aboutListener = NULL;
        context.aboutObj = NULL;
        context.bus = new BusAttachment(this->GetName().c_str(), true);
        if (!context.bus) {
            Cleanup(true);
            status = ER_OUT_OF_MEMORY;
            return status;
        }
        status = context.bus->Start();
        if (ER_OK != status) {
            Cleanup(true);
            return status;
        }
        status = context.bus->Connect();
        if (ER_OK != status) {
            Cleanup(true);
            return status;
        }

        /* Prepare the BusListener */
        context.busListener = new CommonBusListener(context.bus, NULL, LocalSessionJoined, LocalSessionLost, this);
        context.bus->RegisterBusListener(*context.busListener);

        /* Prepare the About Service */
/*
        context.about = new services::AboutService(*context.bus, *context.propertyStore);

        status = context.about->Register(gwConsts::ANNOUNCMENT_PORT_NUMBER);
        if (status != ER_OK) {
            Cleanup(true);
            return status;
        }
*/
        context.aboutObj = new AboutObj(*context.bus);
/*

        status = context.bus->RegisterBusObject(*context.aboutObj);
        if (status != ER_OK) {
            Cleanup(true);
            return status;
        }
*/
    } else {
        /* If bus is not NULL then this Agent BusObject is a child object, and we just copy its context */
        context = *_context;

    }

    if (sp != SESSION_PORT_ANY) {
        TransportMask transportMask = TRANSPORT_ANY;
        SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, false, SessionOpts::PROXIMITY_ANY, transportMask);

        context.busListener->setSessionPort(sp);
        status = context.bus->BindSessionPort(sp, opts, *context.busListener);
        if (status != ER_OK && status != ER_ALLJOYN_BINDSESSIONPORT_REPLY_ALREADY_EXISTS) {
            Cleanup(_context ? false : true);
            return status;
        }
    }

    if (!_context) {
        status = ParseXml(serviceIntrospectionXml.c_str(), context.bus);
        if (ER_OK != status) {
            QCC_LogError(status, ("Error while trying to ParseXml to prepare CloudServiceAgentBusObject"));
            Cleanup(true);
            return status;
        }
    }

    /* Add Object Description */
//     context.about->AddObjectDescription(this->GetPath(), interfaces);

    /* Register itself on the bus */
    status = context.bus->RegisterBusObject(*this, false); // security will be considered in the future
    if (status != ER_OK) {
        Cleanup(_context ? false : true);
        return status;
    }

    /* Add the object description to About */
/*
    status = context.about->AddObjectDescription(String(this->GetPath()), interfaces);
    if (status != ER_OK) {
        Cleanup(_context ? false : true);
        return status;
    }
*/
    /* Prepare the context for all children objects */
/*
    for (size_t i = 0; i < children.size(); i++) {
        CloudServiceAgentBusObject* child = children[i];
        if (child) {
            status = child->PrepareAgent(&context, propertyStore, serviceIntrospectionXml);
            if (ER_OK != status) {
                break;
            }
        }
    }
*/

    return status;
}

QStatus CloudServiceAgentBusObject::Announce()
{
    if (!context.aboutObj) {
        return ER_BUS_NO_TRANSPORTS;
    }

    return context.aboutObj->Announce(sp, aboutDataHandler);
}

QStatus CloudServiceAgentBusObject::Cleanup(bool topLevel)
{
    QStatus status = ER_OK;
    for (size_t i = 0; i < children.size(); i++) {
        CloudServiceAgentBusObject* child = children[i];
        if (child) {
            status = child->Cleanup(false);
            if (ER_OK != status) {
                return status;
            }
        }
    }
    if (!topLevel) {
        /* First clean up all children */
        // Not necessary, because Bus will remove all children while removing top-most BusObject
//         if (context.bus) {
//             context.bus->UnregisterBusObject(*this);
//         }
//         if (context.about) {
//             status = context.about->RemoveObjectDescription(String(this->GetPath()), interfaces);
//         }
    }
    else {
        if (context.bus && context.busListener) {
            context.bus->UnregisterBusListener(*context.busListener);
            context.bus->UnbindSessionPort(context.busListener->getSessionPort());
        }
        if (context.busListener) {
            delete context.busListener;
            context.busListener = NULL;
        }
        if (context.aboutObj) {
//             context.about->RemoveObjectDescription(String(this->GetPath()), interfaces);
            context.aboutObj->Unannounce();
            delete context.aboutObj;
            context.aboutObj = NULL;
        }
        if (context.aboutData) {
            delete context.aboutData;
            context.aboutData = NULL;
        }
        if (context.bus) {
            context.bus->UnregisterBusObject(*this);
            delete context.bus;
            context.bus = NULL;
        }
    }
    return status;
}

void CloudServiceAgentBusObject::LocalSessionJoined(void* arg, ajn::SessionPort sessionPort, ajn::SessionId id, const char* joiner)
{
    if (!arg) {
        return;
    }
    CloudServiceAgentBusObject* parentCSABO = (CloudServiceAgentBusObject*)arg;
    if (!parentCSABO || !parentCSABO->ownerBusObject) {
        QCC_LogError(ER_FAIL, ("The CloudServiceAgentBusObject is not ready"));
        return;
    }
/*
    const char* objPath = parentCSABO->GetPath(); // This is only the path of the root AgentBusObject, like: thierry_luo@nane.cn/BusName (in most cases the root path is /BusName)
    String peerAddr(objPath);
    peerAddr.erase(0, 1); // since the first character is definitely '/'
    size_t slash = peerAddr.find_first_of('/', 0);
    if (slash == String::npos) {
        QCC_LogError(ER_FAIL, ("The format of ObjPath of CloudServiceAgentBusObject is not correct"));
        return;
    }
    String peerBusNameObjPath = peerAddr.substr(slash + 1, peerAddr.length() - slash - 1);
    peerAddr.erase(slash, peerAddr.length() - slash);
*/
   
    QStatus status = parentCSABO->ownerBusObject->UpdateSignalHandlerInfoToCloud(parentCSABO->remoteAccount, parentCSABO->remoteBusName, joiner, id);
    CHECK_STATUS_AND_LOG("Error updating signal handler info to cloud");
}

void CloudServiceAgentBusObject::LocalSessionLost(void* arg, ajn::SessionId sessionId, ajn::SessionListener::SessionLostReason reason)
{
    // TBD: 
}

CloudServiceAgentBusObject::CloudServiceAgentAboutData::CloudServiceAgentAboutData(CloudServiceAgentBusObject* _owner)
    : owner(_owner)
{
    
}

QStatus CloudServiceAgentBusObject::CloudServiceAgentAboutData::GetAboutData(ajn::MsgArg* msgArg, const char* language)
{
    QStatus status = ER_OK;
    if (owner->context.aboutData) {
        *msgArg = *owner->context.aboutData;
    } else {
        status = ER_ABOUT_ABOUTDATA_MISSING_REQUIRED_FIELD;
        QCC_LogError(status, ("No AboutData available for this service/device"));
    }
    return status;
}

QStatus CloudServiceAgentBusObject::CloudServiceAgentAboutData::GetAnnouncedAboutData(ajn::MsgArg* msgArg)
{
    return GetAboutData(msgArg, NULL);
}

}

}