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

#include "Common/CommonUtils.h"
#include <qcc/Debug.h>

#if defined( _MSC_VER )
#include <direct.h>		// _getcwd
// #include <crtdbg.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(MINGW32) || defined(__MINGW32__)
#include <io.h>  // mkdir
#else
#include <sys/stat.h>	// mkdir
#endif

#define QCC_MODULE "SIPE2E"

using namespace ajn;
using namespace qcc;
using namespace std;

namespace sipe2e {

namespace gateway {

QStatus IntrospectChildren(ProxyBusObject* proxy)
{
	if (!proxy) {
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

	ProxyBusObject** proxyBusObjectChildren = new ProxyBusObject *[numChildren];
	numChildren = proxy->GetChildren(proxyBusObjectChildren, numChildren);

	for (size_t i = 0; i < numChildren; i++) {

		String const& objectPath = proxyBusObjectChildren[i]->GetPath();
		QCC_DbgPrintf(("ObjectPath is: %s", objectPath.c_str()));

		status = IntrospectChildren(proxyBusObjectChildren[i]);
		if (status != ER_OK) {
			QCC_LogError(status, ("Could not introspect RemoteObjectChild"));
			delete[] proxyBusObjectChildren;
			return status;
		}
	}
	delete[] proxyBusObjectChildren;
	return ER_OK;
}

String GenerateIntrospectionXml(ProxyBusObject* proxy, String& objName)
{
	String introXml("");
	if (!proxy) {
		return introXml;
	}
	const qcc::String close = "\">\n";

	introXml += "<node name=\"";
	introXml += objName + close;

	/* First we add all interface description */
	size_t numIntf = proxy->GetInterfaces();
	if (0 < numIntf) {
		const InterfaceDescription** intfs = new const InterfaceDescription*[numIntf];
		if (numIntf == proxy->GetInterfaces(intfs, numIntf)) {
			for (size_t i = 0; i < numIntf; i++) {
				const InterfaceDescription* intf = intfs[i];
				if (intf) {
					introXml +=intf->Introspect();
					introXml += "\n";
				}
			}
		}
		delete[] intfs;
	}

	/* Secondly we add all children */
	size_t numChildren = proxy->GetChildren();
	if (0 < numChildren) {
		ProxyBusObject** children = new ProxyBusObject*[numChildren];
		if (numChildren == proxy->GetChildren(children, numChildren)) {
			for (size_t i = 0; i < numChildren; i++) {
				ProxyBusObject* child = children[i];
				String childObjName = child->GetPath();
				size_t pos = childObjName.find_last_of('/');
				if (String::npos != pos) {
					childObjName = childObjName.substr(pos + 1, childObjName.length() - pos - 1);
					introXml += GenerateIntrospectionXml(child, childObjName);
				}
			}
		}
		delete[] children;
	}

	introXml += "</node>\n";
	return introXml;
}

QStatus FillPropertyStore(services::AboutPropertyStoreImpl* propertyStore, const String& aboutKey, const String& aboutValue)
{
#define MANIPULATE_ARRAY_DICT(setMethod) \
	do { \
		size_t pos = 0; \
		do { \
			size_t searchPos = aboutValue.find_first_of('`', pos); \
			String manu = aboutValue.substr(pos, (String::npos==searchPos ? (aboutValue.length() - pos) : (searchPos - pos))); \
			pos = (String::npos == searchPos ? searchPos : searchPos + 1); \
			\
			/* Manipulate the manufacturer name string consisting of language and name in specific language */ \
			String lan, deviceName; \
			size_t posLan = manu.find_first_of(','); \
			if (String::npos != posLan) { \
				lan = manu.substr(0, posLan); \
				deviceName = manu.substr(posLan + 1, manu.length() - posLan - 1); \
			} else { \
				lan = "en"; \
				deviceName = manu; \
			} \
			status = propertyStore->setMethod(deviceName, lan); \
		} while (String::npos != pos); \
	} while (0);

	QStatus status = ER_OK;
	if (aboutKey == "DeviceId") {
		status = propertyStore->setDeviceId(aboutValue);
	} else if (aboutKey == "DeviceName") {
		/* The manufacturer name is like "en,device name`zh,设备名字" */
		MANIPULATE_ARRAY_DICT(setDeviceName);
	} else if (aboutKey == "AppId") {
		status = propertyStore->setAppId(aboutValue);
	} else if (aboutKey == "AppName") {
		/* The manufacturer name is like "en,app name`zh,app名字" */
		MANIPULATE_ARRAY_DICT(setAppName);
	} else if (aboutKey == "DefaultLanguage") {
		status = propertyStore->setDefaultLang(aboutValue);
	} else if (aboutKey == "SupportedLanguages") {
		/* The supported languages are marshaled like "en,zh,sp" */
		vector<String> supportedLanguages;
		size_t pos = 0;
		do {
			size_t searchPos = aboutValue.find_first_of(',', pos);
			String lan = aboutValue.substr(pos, (String::npos==searchPos ? (aboutValue.length() - pos) : (searchPos - pos)));
			supportedLanguages.push_back(lan);
			pos = (String::npos == searchPos ? searchPos : searchPos + 1);
		} while (String::npos != pos);
		status = propertyStore->setSupportedLangs(supportedLanguages);
	} else if (aboutKey == "Description") {
		/* The manufacturer name is like "en,description words`zh,介绍" */
		MANIPULATE_ARRAY_DICT(setDescription);
	} else if (aboutKey == "Manufacturer") {
		/* The manufacturer name is like "en,manufacturer name`zh,设备商名字" */
		MANIPULATE_ARRAY_DICT(setManufacturer);
	} else if (aboutKey == "DateOfManufacture") {
		status = propertyStore->setDateOfManufacture(aboutValue);
	} else if (aboutKey == "ModelNumber") {
		status = propertyStore->setModelNumber(aboutValue);
	} else if (aboutKey == "SoftwareVersion") {
		status = propertyStore->setSoftwareVersion(aboutValue);
	} else if (aboutKey == "AJSoftwareVersion") {
		status = propertyStore->setAjSoftwareVersion(aboutValue);
	} else if (aboutKey == "HardwareVersion") {
		status = propertyStore->setHardwareVersion(aboutValue);
	} else if (aboutKey == "SupportUrl") {
		status = propertyStore->setSupportUrl(aboutValue);
	}
	return status;
}

QStatus NormalizePath(qcc::String& strPath)
{
	char currPath[MAX_PATH];
	currPath[0] = '\0';
	char* tmpRet = NULL;
#ifdef _WIN32
	tmpRet = _getcwd(currPath, MAX_PATH);
#else // linux
	tmpRet = getcwd(currPath, MAX_PATH);
#endif
	if (NULL == tmpRet) {// indicating an error, like the max length exceeded
		return ER_FAIL;
	}
	if (strPath[0] == '.') {
		if (strPath[1] == '.') {
			// some relative path like '../test/test', just replace the '..' with upper directory
#ifdef _WIN32
			tmpRet = strrchr(currPath, '\\');
#else // Linux
			tmpRet = strrchr(currPath, '/');
#endif
			if (NULL == tmpRet) {
				return ER_FAIL;
			}
			*tmpRet = '\0';
			strPath.erase(0, 2);
			strPath.insert(0, (const char*)currPath);
		} else {
			// some relative path like './test/test', just replace the '.' with the current directory
			strPath.erase(0, 1);
			strPath.insert(0, (const char*)currPath);
		}
	} else {
		if (strPath[0] == '\\' || strPath[0] == '/') {
			strPath.insert(0, (const char*)currPath);
		} else {
#ifdef _WIN32
			strcat(currPath, "\\");
#else
			strcat(currPath, "/");
#endif
			strPath.insert(0, (const char*)currPath);
		}
	}
	// Now we iterate the path, if find any '..' the delete the upper directory
	size_t dotPos = strPath.find_first_of("..");
	while (dotPos != qcc::String::npos) {
		if (dotPos <= 2) {
			// no upper directory available
			break;
		}
#ifdef _WIN32
		size_t slashPos = strPath.find_last_of('\\', dotPos - 2);
#else
		size_t slashPos = strPath.find_last_of('/', dotPos - 2);
#endif
		strPath.erase(slashPos, dotPos - slashPos + 2);
		dotPos = strPath.find_first_of("..", slashPos);
	}
	dotPos = strPath.find_first_of('.');
	while (dotPos != qcc::String::npos) {
		strPath.erase(dotPos, 2);
		dotPos = strPath.find_first_of('.', dotPos);
	}
	// Normalizing the path by converting '\' to '/'
	size_t slashPos = strPath.find_first_of('\\');
	while (slashPos != qcc::String::npos) {
		strPath[slashPos] = '/';
		slashPos = strPath.find_first_of('\\', slashPos);
	}
	return ER_OK;
}


} // namespace gateway

} // namespace sipe2e