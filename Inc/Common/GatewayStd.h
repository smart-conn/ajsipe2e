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

#ifndef GATEWAYSTD_H_
#define GATEWAYSTD_H_

#include "Common/CommonBusListener.h"
#include <alljoyn/AboutListener.h>
// #include <alljoyn/about/AboutPropertyStoreImpl.h>
#include <alljoyn/AboutObj.h>


#define CHECK_RETURN(x) if ((status = x) != ER_OK) { return status; }

#define CHECK_STATUS_AND_REPLY(errorInfo) \
    if (ER_OK != status) { \
        QCC_LogError(status, (errorInfo)); \
        status = MethodReply(msg, status); \
        if (ER_OK != status) { \
            QCC_LogError(status, ("Method Reply did not complete successfully")); \
        } \
        return; \
    }

#define CHECK_STATUS_AND_LOG(errorInfo) \
    if (ER_OK != status) { \
        QCC_LogError(status, (errorInfo)); \
        return; \
    }

namespace sipe2e {

namespace gateway {

typedef struct _AllJoynContext {
    ajn::BusAttachment* bus;
    ajn::MsgArg* aboutData;
    CommonBusListener* busListener;
    ajn::AboutListener* aboutListener;
    ajn::AboutObj* aboutObj;

    _AllJoynContext() :
        bus(NULL),
        aboutData(NULL),
        busListener(NULL),
        aboutListener(NULL),
        aboutObj(NULL)
    {

    }
} AllJoynContext;

typedef struct _AnnounceContent {
    uint16_t version; // version of AboutService
    uint16_t port; // port used by the AboutService
    qcc::String busName; // unique bus name of the service
    ajn::MsgArg objectDescsArg; // map of ObjectDescriptions describing objects/interfaces
    ajn::MsgArg aboutDataArg; // map of AboutData describing properties of app/device/services
} AnnounceContent;

/*
   typedef struct _AsyncReplyContext {
    ajn::Message msg;
    ajn::InterfaceDescription::Member* member;
    _AsyncReplyContext(ajn::Message& _msg, const ajn::InterfaceDescription::Member* _member)
        : msg(_msg), member(new ajn::InterfaceDescription::Member(*_member))
    {
    }
    ~_AsyncReplyContext()
    {
        if (member) {
            delete member;
            member = NULL;
        }
    }
   } AsyncReplyContext;
 */

}

}

#endif