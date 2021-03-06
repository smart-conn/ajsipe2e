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
#ifndef IMSTRANSPORTCONSTANTS_H_
#define IMSTRANSPORTCONSTANTS_H_

#include "CloudCommEngine/IMSTransport/ContentType.h"

namespace sipe2e {
namespace gateway {
namespace gwConsts {

namespace customheader {

static const char* RPC_MSG_TYPE = "RMT"; // "RPC-Msg-Type";
static const char* RPC_CALL_ID = "RCI"; // "RPC-Call-ID";
static const char* RPC_ADDR = "RA"; // "RPC-Addr";

enum RPC_MSG_TYPE_ENUM {
    RPC_MSG_TYPE_METHOD_CALL = 0,
    RPC_MSG_TYPE_METHOD_RET = 1,
    RPC_MSG_TYPE_SIGNAL_CALL = 2,
    RPC_MSG_TYPE_SIGNAL_RET = 3,
    RPC_MSG_TYPE_PROPERTY_CALL = 4,
    RPC_MSG_TYPE_PROPERTY_RET = 5,
    RPC_MSG_TYPE_UPDATE_SIGNAL_HANDLER = 6
};

}

/**
 * The status of IMS Transport Layer, which will be sometimes tested to ensure the gateway network availability
 */
enum IMS_TRANSPORT_STATUS_ENUM {
    IMS_TRANSPORT_STATUS_REGISTERED,
    IMS_TRANSPORT_STATUS_UNREGISTERED
};

static const char* DEFAULT_REALM = "nane.cn";
static const unsigned short DEFAULT_PCSCF_PORT = 4060;

static const uint32_t MAX_SIP_CONTENT_LEN = 32000; // maximum of SIP message content length
static const uint32_t MAX_SIP_ADDR_LEN = 128; // maximum of SIP address length
static const uint32_t MAX_RPC_MSG_CALLID_LEN = 128; // maximum of CallID length of RPC message

static const uint32_t RPC_REQ_TIMEOUT = 30000; // timeout in ms of request-response over non-proximal network

static const uint32_t REGISTRATION_DEFAULT_EXPIRES = 300; // expires of SIP registration binding in seconds
static const uint32_t REGISTRATION_DEFAULT_TIMEOUT = 5000; // timeout of SIP (un)registration in ms

static const uint32_t PUBLICATION_DEFAULT_TIMEOUT = 5000; // timeout of SIP (un)publish in ms
static const uint32_t SUBSCRIPTION_DEFAULT_TIMEOUT = 5000; // timeout of SIP (un)subscribe in ms

static const uint32_t SIPSTACK_HEARTBEAT_INTERVAL = 30000; // heartbeat interval for SIP stack in ms, sending OPTIONS periodically
static const uint32_t SIPSTACK_HEARTBEAT_EXPIRES = 2000; // heartbeat expires for SIP stack in ms, waiting for OPTIONS response

static const uint32_t SIPSTACK_SUBSCRIPTION_INTERVAL = 5000; // heartbeat interval for SIP stack in ms, sending OPTIONS periodically

static const char* DEFAULT_NAMESPACE_URI = "http://nane.cn/sipe2e";
static const char* DEFAULT_NAMESPACE_PREFIX = "sipe2e";

}
}
}

#endif // IMSTRANSPORTCONSTANTS_H_