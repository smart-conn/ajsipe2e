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
#ifndef CONTENTTYPE_H_
#define CONTENTTYPE_H_

namespace sipe2e {
namespace gateway {
namespace gwConsts {

namespace contenttype {

static const char* CONFERENCE_INFO = "application/conference-info+xml";
static const char* DIALOG_INFO = "application/dialog-info+xml";
static const char* EXTERNAL_BODY = "message/external-body";
static const char* MESSAGE_SUMMARY = "application/simple-message-summary";
static const char* MULTIPART_RELATED = "multipart/related";
static const char* OMA_DEFERRED_LIST = "application/vnd.oma.im.deferred-list+xml";
static const char* PIDF = "application/pidf+xml";
static const char* PIDF_DIFF = "application/pidf-diff+xml";
static const char* REG_INFO = "application/reginfo+xml";
static const char* RLMI = "application/rlmi+xml";
static const char* RPID = "application/rpid+xml";

static const char* WATCHER_INFO = "application/watcherinfo+xml";
static const char* XCAP_DIFF = "application/xcap-diff+xml";

static const char* SMS_3GPP = "application/vnd.3gpp.sms";

static const char* ALLJOYN_XML = "application/xml"; // For RPC over SIP Message

static const char* CPIM = "message/CPIM";
static const char* TEXT_PLAIN = "text/plain";
static const char* IS_COMPOSING = "application/im-iscomposing+xml";

static const char* DOUBANGO_DEVICE_INFO = "doubango/device-info";


static const char* UNKNOWN = "unknown/unknown";

}
}
}
}

#endif
