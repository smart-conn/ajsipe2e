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

#ifndef COMMONUTILS_H_
#define COMMONUTILS_H_

#include <qcc/platform.h>

#include <qcc/String.h>
#include <qcc/XmlElement.h>

#include <alljoyn/ProxyBusObject.h>

#include "Common/GatewayConstants.h"
#include "Common/GatewayStd.h"

namespace sipe2e {

namespace gateway {

/**
 * Introspect Remote Object and its all children and grand children and so on
 * @param proxy - the top-level ProxyBusObject
 */
QStatus IntrospectChildren(ajn::ProxyBusObject* proxy);

/**
 * Generate introspection XML string according to ProxyBusObject (and its children)
 * @param proxy - the ProxyBusObject
 * @param objName - the object path name of this ProxyBusObject. If it's top-level,
 *                                   the objName should be the absolute object path, and if not,
 *                                   the objName should be the last part of the object path
 */
qcc::String GenerateIntrospectionXml(ajn::ProxyBusObject* proxy, qcc::String& objName);

/**
 * Filling in the property store using the key/value pair
 * @param propertyStore -    the propertyStore that will be filled in
 * @param aboutKey - the key of one about data
 * @param aboutValue - the value of one about data
 */
QStatus FillPropertyStore(ajn::services::AboutPropertyStoreImpl* propertyStore, const qcc::String& aboutKey, const qcc::String& aboutValue);

/**
 * 
 * @param strPath - the relative path
 */
QStatus NormalizePath(qcc::String& strPath);

/**
 * Returns an XML string representation of this type, just exactly as ToString() does
 * @param args - input arguments
 * @param indent - number of spaces to indent the generated xml
 */
qcc::String ArgToXml(const ajn::MsgArg* args, size_t indent);
/**
 * Parse the xml string and return the result arguments array
 * @param argsStr - the result of ArgsToXml() of response argument array
 * @param args - parsed result arguments with signature 'av'
 */
QStatus XmlToArg(const qcc::XmlElement* argEle, ajn::MsgArg& arg);

} // namespace gateway

} // namespace sipe2e

#endif
