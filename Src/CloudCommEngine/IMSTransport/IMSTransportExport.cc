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

#include "CloudCommEngine/IMSTransport/IMSTransportExport.h"
#include "CloudCommEngine/IMSTransport/IMSTransport.h"

#include "CloudCommEngine/IMSTransport/pidf.h"

using namespace sipe2e;
using namespace gateway;

int SIPE2E_GATEWAY_CALL ITInitialize()
{
    return IMSTransport::GetInstance()->Init();
}

int SIPE2E_GATEWAY_CALL ITShutdown()
{
    return IMSTransport::DeleteInstance();
}

int SIPE2E_GATEWAY_CALL ITGetStatus()
{
    return IMSTransport::GetInstance()->GetStatus();
}

int SIPE2E_GATEWAY_CALL ITSubscribe(const char* remoteAccount)
{
    return IMSTransport::GetInstance()->Subscribe(remoteAccount);
}

int SIPE2E_GATEWAY_CALL ITUnsubscribe(const char* remoteAccount)
{
    return IMSTransport::GetInstance()->Unsubscribe(remoteAccount);
}

int SIPE2E_GATEWAY_CALL ITPublishService(const char* introspectionXml)
{
    return IMSTransport::GetInstance()->PublishService(introspectionXml);
}

int SIPE2E_GATEWAY_CALL ITDeleteService(const char* introspectionXml)
{
    return IMSTransport::GetInstance()->DeleteService(introspectionXml);
}

int SIPE2E_GATEWAY_CALL ITReadCloudMessage(char** msgBuf)
{
    return IMSTransport::GetInstance()->ReadCloudMessage(msgBuf);
}

int SIPE2E_GATEWAY_CALL ITSendCloudMessage(int isRequest, 
                                           const char* peer,
                                           const char* callId,
                                           const char* addr,
                                           const char* msgBuf, 
                                           char** resMsgBuf)
{
    return IMSTransport::GetInstance()->SendCloudMessage(1==isRequest, peer, callId, addr, msgBuf, resMsgBuf);
}

void SIPE2E_GATEWAY_CALL ITReleaseBuf(char* buf)
{
    if (buf) {
        delete[] buf;
    }
}

#ifdef _DEBUG
const char* SIPE2E_GATEWAY_CALL ITTestServiceSerialize(char* buf)
{
    ims::service serviceIns;
    serviceIns.SetIntrospectionXml((qcc::String)buf);
/*
    static std::string introspectionXml;
    introspectionXml.clear();
    serviceIns.Serialize(introspectionXml);
    return introspectionXml.c_str();
    */

    ims::status _status;
    _status.SetBasicStatus(ims::basic::open);
    ims::tuple _tuple;
    _tuple.SetId((qcc::String)"/BusName/RootObjPath");
    _tuple.SetStatus(_status);
    _tuple.SetService(serviceIns);

    ims::presence _presence;
    _presence.SetEntity((qcc::String)"pres:lyh@nane.cn");
    _presence.AddTuple(_tuple);

    static qcc::String presenceXml;
    _presence.Serialize(presenceXml);
    return presenceXml.c_str();
}
#endif
