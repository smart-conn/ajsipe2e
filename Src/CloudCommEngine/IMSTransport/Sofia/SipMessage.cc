/******************************************************************************
 * Copyright (c) 2014-2015, AllSeen Alliance. All rights reserved.
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

#include "SipMessage.h"

class SipE2eMessageHelper {
  public:
    bool foundValue(const char* const s, const char* const name, char* val)
    { // strtok is not allowed in this routine
        const char* p = s;
        while (*p != '\0' && isspace(*p))
            p++;
        const char* p2 = p;
        while (*p != '\0' && !isspace(*p) && *p != '=')
            p++;
        const char* p3 = p;
        int len = strlen(name);
        if ((p3 - p2) == len && strncmp(p2, name, len) == 0) { // found
            while (*p != '\0' && (isspace(*p) || *p == '='))
                p++;
            strcpy(val, p);
            return true;
        }
        return false;
    }
    // a=v1;b=v2;c=v3
    char* getParamValue(const char* const s, const char* const name)
    {
        static char val[BUFFER_SIZE_B];
        val[0] = '\0';

        char ss[BUFFER_SIZE_B];
        strcpy(ss, s);
        char* pch = strtok(ss, ";");
        while (pch) {
            if (foundValue(pch, name, val)) {
                return val;
            }
            pch = strtok(nullptr, ";");
        }
        return nullptr;
    }
};

SipMessage::SipMessage(SipE2eContext* _ssc, const sip_t* _msg) :
    ssc(_ssc), msg(_msg)
{
}

SipMessage::~SipMessage()
{
}

bool SipMessage::isValid()
{
    return (NULL != msg);
}

sip_method_t SipMessage::getRequestType()
{
    if (msg->sip_request) {
        return msg->sip_request->rq_method;
    }
    return sip_method_invalid;
}

int SipMessage::getResponseCode()
{
    if (msg->sip_status) {
        return msg->sip_status->st_status;
    }
    return 0;
}

char* SipMessage::getSipHeaderValue(const char* name, unsigned index /*= 0*/)
{
    /**
     * this routine will not work since the h_data is always nullptr
     *
     * */
    if (strcmp(name, "From") == 0 || strcmp(name, "f") == 0) {
        if (msg->sip_from) {
            return (char*) msg->sip_from->a_common->h_data; // always nullptr
        }
    } else if (strcmp(name, "To") == 0 || strcmp(name, "t") == 0) {
        if (msg->sip_to) {
            return (char*) msg->sip_to->a_common->h_data; // always nullptr
        }
    } else if (strcmp(name, "Service-Route") == 0) {
        if (msg->sip_service_route) {
            return url_as_string(ssc->sip_home, msg->sip_service_route->r_url);
        }
    }
    return nullptr;
}

char* SipMessage::getSipHeaderParamValue(
    const char* name,
    const char* param,
    unsigned index /*= 0*/)
{
    SipE2eMessageHelper mh;
    if (strcmp(name, "From") == 0 || strcmp(name, "f") == 0) {
        if (msg->sip_from) {
            msg_param_t const* p = msg->sip_from->a_params;
            if (p) {
                return mh.getParamValue(*p, param);
            }
        }
    } else if (strcasecmp(name, "To") == 0 || strcmp(name, "t") == 0) {
        if (msg->sip_to) {
            msg_param_t const* p = msg->sip_to->a_params;
            if (p) {
                return mh.getParamValue(*p, param);
            }
        }
    }
    return 0;
}

unsigned SipMessage::getSipContentLength()
{
    if (msg->sip_content_length) {
        return msg->sip_content_length->l_length;
    } else if (msg->sip_payload) {
        return msg->sip_payload->pl_len;
    }
    return 0;
}

unsigned SipMessage::getSipContent(void* output, unsigned maxsize)
{
    if (msg->sip_payload) {
        int len = (maxsize < msg->sip_payload->pl_len ? maxsize : msg->sip_payload->pl_len);
        memcpy(output, msg->sip_payload->pl_data, len);
        return len;
    }
    return 0;
}
