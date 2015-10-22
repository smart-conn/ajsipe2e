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
#ifndef PIDF_H_
#define PIDF_H_

#include "Common/GatewayExport.h"
#include <qcc/String.h>
#include <vector>

namespace qcc {
    class XmlElement;
}


namespace sipe2e {
namespace gateway {

namespace ims {

class SIPE2E_GATEWAY_EXPORT service
{
public:
	service();
	~service();

public:
	void SetIntrospectionXml(const qcc::String& _introspectionXml);
	qcc::String& GetIntrospectionXml();

public:
	void Serialize(qcc::String& serviceXml);

protected:
	qcc::String introspectionXml;
};

enum basic
{
	open,
	closed
};

class SIPE2E_GATEWAY_EXPORT status
{
public:
	status();
	~status();

public:
	void SetBasicStatus(basic basicStatus);
	basic GetBasicStatus();

public:
	void Serialize(qcc::String& statusXml);
	void Deserialize(const qcc::XmlElement* statusNode);

protected:
	basic basicField;
};

class SIPE2E_GATEWAY_EXPORT tuple 
{
public:
	tuple();
	~tuple();

public:
	void SetId(const qcc::String& id);
	qcc::String& GetId();
	void SetStatus(const status& _status);
	status& GetStatus();
	void SetService(const service& _service);
	service& GetService();

public:
	void Serialize(qcc::String& tupleXml);
	void Deserialize(const qcc::XmlElement* tupleNode);

protected:
	qcc::String idField;
	status statusField;
// 	std::vector<note> noteField;
	service serviceField;
};

class SIPE2E_GATEWAY_EXPORT presence 
{
public:
	presence();
	~presence();

public:
	void SetEntity(const qcc::String& entity);
	void AddTuple(const tuple& _tuple);

public:
	void Serialize(qcc::String& presenceXml);
	void Deserialize(const qcc::XmlElement* presenceNode);
	void Deserialize(const qcc::String& presenceXml);

	std::vector<tuple>& GetTuples();

protected:
	qcc::String entityField;
	std::vector<tuple> tupleField;
};


} // namespace ims

} // namespace gateway

} // namespace sipe2e

#endif