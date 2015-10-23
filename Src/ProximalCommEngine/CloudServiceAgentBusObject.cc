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

#include <qcc/StringSource.h>

#include "Common/CommonUtils.h"
#include "ProximalCommEngine/CloudServiceAgentBusObject.h"
#include "ProximalCommEngine/ProximalCommEngineBusObject.h"

#include <alljoyn/AllJoynStd.h>
#include <alljoyn/DBusStd.h>
#include <SignatureUtils.h>
#include <BusUtil.h>


#define QCC_MODULE "SIPE2E"


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


CloudServiceAgentBusObject::CloudServiceAgentBusObject(::String const& objectPath, ManagedObj<ProxyBusObject> cloudEnginePBO, ProximalCommEngineBusObject* owner)
	: BusObject(objectPath.c_str()), cloudEngineProxyBusObject(cloudEnginePBO), ownerBusObject(owner)
{

}

CloudServiceAgentBusObject::~CloudServiceAgentBusObject()
{

}

void CloudServiceAgentBusObject::CommonMethodHandler(const InterfaceDescription::Member* member, Message& msg)
{
	QStatus status = ER_OK;

	/* Make sure the cloud communication engine is present */
	if (!cloudEngineProxyBusObject->IsValid()) {
		/* The CloudCommEngine is not present, so the calls cannot be forwarded */
		status = MethodReply(msg, ER_FAIL);
		QCC_DbgPrintf(("No CloudCommEngine present"));
		if (ER_OK != status) {
			QCC_LogError(status, ("Method call did not complete successfully"));
		}
		return;
	}

	/* Retrieve all arguments of the method call */
	size_t  numArgs = 0;
	const MsgArg* args = 0;
	msg->GetArgs(numArgs, args);


	/* Parsing all arguments and re-marshaling them to send out to CloudCommEngine */
	MsgArg cloudCallArgs[3];

	/**
	 *	The first arg is the address of the cloud service plus the interface name and method name
	 * 
	 */
	String objPath(this->GetPath());
	if (objPath[objPath.length() - 1] != '/') {
		objPath += "/";
	}
	objPath += member->iface->GetName();
	objPath += "/";
	objPath += member->name;
	status = cloudCallArgs[0].Set("s", objPath.c_str());
	CHECK_STATUS_AND_REPLY("Error setting the arg value");

	/**
	 * The second arg is the parameters vector, every element of which a MsgArg (variant argument)
	 */
	status = cloudCallArgs[1].Set("av", numArgs, args);
// 	status = MarshalUtils::MarshalAllJoynArrayArgs(args, numArgs, cloudCallArgs[1]);
	CHECK_STATUS_AND_REPLY("Error marshaling array args");
	/* The third arg is the local session ID */
	status = cloudCallArgs[2].Set("x", msg->GetSessionId());
	CHECK_STATUS_AND_REPLY("Error setting the arg value");

	/**
	  * Now we're ready to send out the cloud call by calling CloudCommEngine::AJCloudMethodCall
	  * Be noted that, since all calls to this BusObject are handled in this common handler, in case of blocking,
	  * we should make the call CloudCommEngine::AJCloudMethodCall asynchronously, and reply the call
	  * in the asynchronous MessageReceiver::ReplyHandler.
	  */
	AsyncReplyContext* replyContext = new AsyncReplyContext(msg, member);
	status = cloudEngineProxyBusObject->MethodCallAsync(gwConsts::SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_INTERFACE.c_str(),
		gwConsts::SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_CLOUDMETHODCALL.c_str(), 
		const_cast<MessageReceiver*>(static_cast<const MessageReceiver* const>(this)), 
		static_cast<MessageReceiver::ReplyHandler>(&CloudServiceAgentBusObject::CloudMethodCallReplyHandler),
		cloudCallArgs, 3, (void*)replyContext);
	CHECK_STATUS_AND_REPLY("Error making the cloud method call");
}


QStatus CloudServiceAgentBusObject::ParseXml(const char* xml, services::AboutPropertyStoreImpl& propertyStore)
{
	if (!bus) {
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
						if (0 || childObjPath.size() > 1) {
							childObjPath += '/';
						}
						childObjPath += relativePath;
						if (!relativePath.empty() && IsLegalObjectPath(childObjPath.c_str())) {
							CloudServiceAgentBusObject* newChild = new CloudServiceAgentBusObject(childObjPath, cloudEngineProxyBusObject, ownerBusObject);
							status = newChild->ParseNode(elem);
							if (ER_OK != status) {
							    QCC_LogError(status, ("Error while parsing Node in the introspection XML"));
								return status;
							}
							children.push_back(newChild);
						} else {
							status = ER_FAIL;
							QCC_LogError(status, ("Object path not correct"));
							return status;
						}

					} else if (elemName == "interface") {
						status = ParseInterface(elem);
						if (ER_OK != status) {
							QCC_LogError(status, ("Error while parsing Interface in the introspection XML"));
							return status;
						}
					} else if (elemName == "about") {
						/* Fill in the about data */
						vector<XmlElement*>::const_iterator itAboutData = elem->GetChildren().begin();
						for (;itAboutData != elem->GetChildren().end(); itAboutData++) {
							const XmlElement* elemAboutData = *itAboutData;
							const String& aboutKey = elemAboutData->GetAttribute("key");
							const String& aboutValue = elemAboutData->GetAttribute("value");
							FillPropertyStore(&propertyStore, aboutKey, aboutValue);
						}
					}
				}
			} else if (root->GetName() == "interface") {
				return ParseInterface(root);
			} else if (root->GetName() == "node") {
				return ParseNode(root);
			}
		}
	}
	return status;
}

QStatus CloudServiceAgentBusObject::ParseNode(const XmlElement* root)
{
	if (!bus) {
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
			status = ParseInterface(elem);
		} else if (elemName == "node") {
			const String& relativePath = elem->GetAttribute("name");
			String childObjPath = this->GetPath();
			if (0 || childObjPath.size() > 1) {
				childObjPath += '/';
			}
			childObjPath += relativePath;
			if (!relativePath.empty() && IsLegalObjectPath(childObjPath.c_str())) {
				CloudServiceAgentBusObject* newChild = new CloudServiceAgentBusObject(childObjPath, cloudEngineProxyBusObject, ownerBusObject);
				status = newChild->ParseNode(elem);
				if (ER_OK != status) {
					//this->AddChild(newChild); // AddChild will happen in the RegisterObject() process
					break;
				}
				children.push_back(newChild);
			} else {
				status = ER_FAIL;
				break;
			}
		}
	}
	return status;
}

QStatus CloudServiceAgentBusObject::ParseInterface(const XmlElement* root)
{
	if (!bus) {
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
		(ifName == org::freedesktop::DBus::Properties::InterfaceName)) {
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
			QCC_DbgHLPrintf(("Unknown annotation %s is defaulting to 'inherit'. Valid values: 'true', 'inherit', or 'off'.", org::alljoyn::Bus::Secure));
		}
		secPolicy = AJ_IFC_SECURITY_INHERIT;
	}

	/* Create a new interface */
// 	InterfaceDescription intf(ifName.c_str(), secPolicy);
	InterfaceDescription* intf = NULL;
	status = bus->CreateInterface(ifName.c_str(), intf, secPolicy);
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
			const InterfaceDescription::Member* intfMembers[256];
			size_t memberNum = intf->GetMembers(intfMembers, 256);
			for (size_t i = 0; i < memberNum; i++) {
				const InterfaceDescription::Member* currMember = intfMembers[i];
				if (currMember) {
					if (currMember->memberType == MESSAGE_METHOD_CALL) {
						status = this->AddMethodHandler(currMember, static_cast<MessageReceiver::MethodHandler>(&CloudServiceAgentBusObject::CommonMethodHandler), NULL);
						if (ER_OK != status) {
							// log error
							break;
						} else if (currMember->memberType == MESSAGE_SIGNAL) {
							// not implemented
						}
					}
				}
			}
		}

	}

	return status;
}

void CloudServiceAgentBusObject::CloudMethodCallReplyHandler(Message& msg, void* context)
{
	if (!context) {
		QCC_LogError(ER_BAD_ARG_2, ("No context available in the CloudMethodCall ReplyHandler"));
		return;
	}
	QStatus status = ER_OK;

	AsyncReplyContext* replyContext = reinterpret_cast<AsyncReplyContext*>(context);

	size_t numArgs = 0;
	const MsgArg* args = NULL;
	msg->GetArgs(numArgs, args);
	if (2 <= numArgs) {
		MsgArg* outArgsVariant = NULL;
		size_t numOutArgs = 0;
		// 		status = MarshalUtils::UnmarshalAllJoynArrayArgs(args[0], outArgs, numOutArgs);
		status = args[0].Get("av", &numOutArgs, &outArgsVariant);
		if (ER_OK != status) {
			QCC_LogError(status, ("Error unmarshaling the array args"));
			status = MethodReply(replyContext->msg, status);
			if (ER_OK != status) {
				QCC_LogError(status, ("Method Reply did not complete successfully"));
				return;
			}
		}
		MsgArg* outArgs = new MsgArg[numOutArgs];
		for (size_t i = 0; i < numOutArgs; i++) {
			outArgs[i] = *outArgsVariant[i].v_variant.val;
		}
		/* Reply to the local caller */
		status = MethodReply(replyContext->msg, outArgs, numOutArgs);
		if (ER_OK != status) {
			QCC_LogError(status, ("Method call did not complete successfully"));
			return;
		}

		const char* cloudSessionID = NULL;
		status = args[1].Get("s", &cloudSessionID);
		if (ER_OK == status && cloudSessionID) {
			/* Store the cloud session ID, and map it to local session ID */
			// to be continued
		}

	} else {
		/* something wrong while retrieving the args from the reply message */
		QCC_LogError(ER_BAD_ARG_COUNT, ("Error reply argument count of cloud method call"));
	}
}


QStatus CloudServiceAgentBusObject::PrepareAgent(AllJoynContext* _context, services::AboutPropertyStoreImpl* propertyStore)
{
	QStatus status = ER_OK;

	if (NULL == _context) {
		/* If it's NULL then this Agent BusObject is the top level parent object, and we should prepare AllJoyn context for it */
		/* Prepare the BusAttachment */
		context.bus = NULL;
		context.propertyStore = propertyStore;
		context.busListener = NULL;
		context.aboutHandler = NULL;
		context.about = NULL;
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
		context.busListener = new CommonBusListener(context.bus);

		TransportMask transportMask = TRANSPORT_ANY;
		SessionPort sp = SESSION_PORT_ANY;
		SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, false, SessionOpts::PROXIMITY_ANY, transportMask);

		QStatus status = context.bus->BindSessionPort(sp, opts, *context.busListener); // If successful, selecdted seession port will be returned
		if (status != ER_OK) {
			Cleanup(true);
			return status;
		}

		/* Prepare the About Service */
		context.about = new services::AboutService(*context.bus, *context.propertyStore);
		context.busListener->setSessionPort(sp);
		context.bus->RegisterBusListener(*context.busListener);

		status = context.about->Register(sp);
		if (status != ER_OK) {
			Cleanup(true);
			return status;
		}

		status = context.bus->RegisterBusObject(*context.about);
		if (status != ER_OK) {
			Cleanup(true);
			return status;
		}
	} else {
		/* If bus is not NULL then this Agent BusObject is a child object, and we just copy its context */
		context = *_context;

	}

	/* Register itself on the bus */
	status = context.bus->RegisterBusObject(*this, false); // security will be considered in the future
	if (status != ER_OK) {
		Cleanup(_context ? false : true);
		return status;
	}

	/* Add the object description to About */
	status = context.about->AddObjectDescription(String(this->GetPath()), interfaces);
	if (status != ER_OK) {
		Cleanup(_context ? false : true);
		return status;
	}
	/* Prepare the context for all children objects */
	for (size_t i = 0; i < children.size(); i++) {
		CloudServiceAgentBusObject* child = children[i];
		if (child) {
			status = child->PrepareAgent(&context, propertyStore);
			if (ER_OK != status) {
				break;
			}
		}
	}

	return status;
}

QStatus CloudServiceAgentBusObject::Announce()
{
	if (!context.about) {
		return ER_BUS_NO_TRANSPORTS;
	}

	return context.about->Announce();
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
		if (context.bus) {
			context.bus->UnregisterBusObject(*this);
		}
		if (context.about) {
			status = context.about->RemoveObjectDescription(String(this->GetPath()), interfaces);
		}
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
		if (context.about) {
			context.about->RemoveObjectDescription(String(this->GetPath()), interfaces);
			delete context.about;
			context.about = NULL;
		}
		if (context.propertyStore) {
			delete context.propertyStore;
			context.propertyStore = NULL;
		}
		if (context.bus) {
			context.bus->UnregisterBusObject(*this);
			delete context.bus;
			context.bus = NULL;
		}
	}
	return status;
}


}

}