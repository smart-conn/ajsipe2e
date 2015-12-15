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
#include "CloudCommEngine/IMSTransport/pidf.h"
#include "CloudCommEngine/IMSTransport/IMSTransportConstants.h"

#include "alljoyn/Status.h"

#include <qcc/XmlElement.h>
#include <qcc/StringSource.h>

using namespace qcc;

namespace sipe2e {
namespace gateway {

namespace ims {


service::service()
    : introspectionXml("")
{

}

service::~service()
{
    introspectionXml.clear();
}

void service::SetIntrospectionXml(const String& _introspectionXml)
{
    introspectionXml = _introspectionXml;
}

String& service::GetIntrospectionXml()
{
    return introspectionXml;
}

void service::Serialize(String& serviceXml)
{
    StringSource source(introspectionXml);

    XmlParseContext pc(source);
    QStatus status = XmlElement::Parse(pc);

    if (ER_OK != status) {
        serviceXml.clear();
        return;
    }

    XmlElement* rootNode = (XmlElement*)pc.GetRoot();
    if (NULL == rootNode) {
        serviceXml.clear();
        return;
    }

    if (rootNode->GetName() == "service") {
        rootNode->AddAttribute((String)"xmlns", (String)gwConsts::DEFAULT_NAMESPACE_URI);
        serviceXml = rootNode->Generate();
    } else {
        serviceXml.clear();
    }

}


status::status()
    : basicField(basic::closed)
{

}

status::~status()
{

}

void status::SetBasicStatus(basic basicStatus)
{
    basicField = basicStatus;
}

basic status::GetBasicStatus()
{
    return basicField;
}

void status::Serialize(String& statusXml)
{
    statusXml = "<status><basic>";
    statusXml += (basicField == basic::open ? "open" : "closed");
    statusXml += "</basic></status>";
}

void status::Deserialize(const XmlElement* statusNode)
{
    if (statusNode == NULL) {
        return;
    }
    const std::vector<XmlElement*>& basicNode = statusNode->GetChildren();

    if (basicNode.size() > 0) {
        const String& basicStatusText = basicNode[0]->GetContent();
        if (basicStatusText == "open") {
            basicField = basic::open;
        } else {
            basicField = basic::closed;
        }
    }
}


tuple::tuple()
    : idField("")
{

}

tuple::~tuple()
{

}

void tuple::SetId(const String& id)
{
    idField = id;
}

String& tuple::GetId()
{
    return idField;
}

void tuple::SetStatus(const status& _status)
{
    statusField = _status;
}

status& tuple::GetStatus()
{
    return statusField;
}

void tuple::SetService(const service& _service)
{
    serviceField = _service;
}

service& tuple::GetService()
{
    return serviceField;
}

void tuple::Serialize(String& tupleXml)
{
    tupleXml = "<tuple id=\"";
    tupleXml += idField;
    tupleXml += "\">";
    
    String childXml;
    statusField.Serialize(childXml);
    tupleXml += childXml;

    serviceField.Serialize(childXml);
    tupleXml += childXml;

    tupleXml += "</tuple>";
}

void tuple::Deserialize(const XmlElement* tupleNode)
{
    if (tupleNode == NULL) {
        return;
    }
    idField = tupleNode->GetAttribute("id");
    const std::vector<XmlElement*>& tupleChildren = tupleNode->GetChildren();
    for (size_t i = 0; i < tupleChildren.size(); i++) {
        XmlElement* tupleChild = tupleChildren[i];
        const String& tupleChildName = tupleChild->GetName();
        if (tupleChildName == "status") {
            statusField.Deserialize(tupleChild);
        } else if (tupleChildName == "service") {
            serviceField.SetIntrospectionXml(tupleChild->Generate());
        }
    }
}


presence::presence()
    : entityField("")
{

}

presence::~presence()
{
    tupleField.clear();
}

void presence::SetEntity(const String& entity)
{
    entityField = entity;
}

void presence::AddTuple(const tuple& _tuple)
{
    tupleField.push_back(_tuple);
}

void presence::Serialize(String& presenceXml)
{
    presenceXml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    presenceXml += "<presence entity=\"";
    presenceXml += entityField;
    presenceXml += "\">";

    for (size_t i = 0; i < tupleField.size(); i++) {
        tuple& _tuple = tupleField[i];
        String childTupleXml;
        _tuple.Serialize(childTupleXml);
        presenceXml += childTupleXml;
    }

    presenceXml += "</presence>";
}

void presence::Deserialize(const XmlElement* presenceNode) 
{
    if (presenceNode == NULL) {
        return;
    }
    entityField = presenceNode->GetAttribute("entity");
    const std::vector<XmlElement*>& presenceChildren = presenceNode->GetChildren();
    for (size_t i = 0; i < presenceChildren.size(); i++) {
        tuple _tuple;
        _tuple.Deserialize(presenceChildren[i]);
        tupleField.push_back(_tuple);
    }
}

void presence::Deserialize(const String& presenceXml)
{
    StringSource source(presenceXml);

    XmlParseContext pc(source);
    QStatus status = XmlElement::Parse(pc);

    if (ER_OK != status) {
        return;
    }
    const XmlElement* rootNode = pc.GetRoot();
    presence::Deserialize(rootNode);
}

std::vector<tuple>& presence::GetTuples()
{
    return tupleField;
}

}

}
}