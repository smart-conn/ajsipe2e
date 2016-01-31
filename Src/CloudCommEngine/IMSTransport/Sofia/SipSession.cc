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

#include "SipSession.h"
#include "SipStack.h"
#include "Sofia.h"
#include "SipCommon.h"

SipSession::SipSession(SipStack* stack) :
        m_pStack(stack), m_pOperation(NULL)
{

}

SipSession::~SipSession()
{
    if (m_pOperation && m_pStack) {
		Sipe2eSofiaHelper sh;
		sh.deleteOperation(m_pStack->getContext(), m_pOperation);
	}
}

bool SipSession::addHeader(const char* name, const char* value)
{
	if (!name || !value || !m_pOperation || !m_pOperation->op_handle) {
		return false;
	}
    nua_handle_t* hdl = m_pOperation->op_handle;
	if (0 == strcasecmp(name, "Content-Type")) {
		nua_set_hparams(hdl, SIPTAG_CONTENT_TYPE_STR(value),
			TAG_END());
	} else if (0 == strcasecmp(name, "Event")) {
		nua_set_hparams(hdl, SIPTAG_EVENT_STR(value),
			TAG_END());
	} else {
		char* headerStr = new char[strlen(name) + strlen(value) + 8];
		sprintf(headerStr, "%s: %s", name, value);
		nua_set_hparams(hdl, SIPTAG_HEADER_STR(headerStr),
			TAG_END());
		delete[] headerStr;
	}
	return true;
}

bool SipSession::removeHeader(const char* name)
{
    // TODO, NOT IMPLEMENTED
    return true;
}

bool SipSession::addCaps(const char* name, const char* value)
{
    // TODO, NOT IMPLEMENTED
    return true;
}

bool SipSession::addCaps(const char* name)
{
    // TODO, NOT IMPLEMENTED
    return true;
}

bool SipSession::removeCaps(const char* name)
{
    // TODO, NOT IMPLEMENTED
    return true;
}

bool SipSession::setExpires(unsigned expires)
{
	if (!m_pOperation || !m_pOperation->op_handle) {
		return false;
	}
	nua_handle_t* hdl = m_pOperation->op_handle;
	char expiresStr[32];
	sprintf(expiresStr, "%u", expires);
    nua_set_hparams(hdl, SIPTAG_EXPIRES_STR(expiresStr),
    TAG_END());
    return true;
}

bool SipSession::setFromUri(const char* fromUriString)
{
	if (!fromUriString || !m_pOperation || !m_pOperation->op_handle) {
		return false;
	}
	nua_handle_t* hdl = m_pOperation->op_handle;
    nua_set_hparams(hdl, SIPTAG_FROM_STR(fromUriString),
    TAG_END());
    return true;
}

bool SipSession::setToUri(const char* toUriString)
{
	if (!toUriString || !m_pOperation || !m_pOperation->op_handle) {
		return false;
	}
	nua_handle_t* hdl = m_pOperation->op_handle;
    nua_set_hparams(hdl, SIPTAG_TO_STR(toUriString),
    TAG_END());
    return true;
}

MessagingSession::MessagingSession(SipStack* stack, nua_handle_t* nh) :
	SipSession(stack)
{
	Sipe2eSofiaHelper sh;
	if (!nh) {
		m_pOperation = sh.createOperation(m_pStack->getContext(), SIP_METHOD_MESSAGE, TAG_END());
	} else {
		m_pOperation = sh.createOperationWithHandle(m_pStack->getContext(), SIP_METHOD_MESSAGE, nh);
	}
}

MessagingSession::~MessagingSession()
{

}

bool MessagingSession::send(const char* payload)
{
    if (!payload) {
        return false;
    }
    Sipe2eContex* ctx = m_pStack->getContext();
    sip_payload_t *pl = sip_payload_format(ctx->sip_home, "%s", payload);
    if (!m_pOperation) {
        Sipe2eSofiaHelper sh;
        m_pOperation = sh.createOperation(ctx, SIP_METHOD_MESSAGE, TAG_END());
        nua_message(m_pOperation->op_handle, SIPTAG_PAYLOAD(pl),
                TAG_END());
    } else {
        nua_message(m_pOperation->op_handle, SIPTAG_PAYLOAD(pl),
                TAG_END());
    }
    su_free(ctx->sip_home, pl);
    return true;
}

bool MessagingSession::accept()
{

    return true;
}

bool MessagingSession::reject()
{

    return true;
}

OptionsSession::OptionsSession(SipStack* stack, nua_handle_t* nh) :
        SipSession(stack)
{
	Sipe2eSofiaHelper sh;
	if (!nh) {
		m_pOperation = sh.createOperation(stack->getContext(), SIP_METHOD_OPTIONS,
			TAG_END());
	} else {
		m_pOperation = sh.createOperationWithHandle(stack->getContext(), SIP_METHOD_OPTIONS, nh);
	}
}

OptionsSession::~OptionsSession()
{

}

bool OptionsSession::send()
{
    if (!m_pOperation) {
        Sipe2eContex* ctx = m_pStack->getContext();
        Sipe2eSofiaHelper sh;
        m_pOperation = sh.createOperation(ctx, SIP_METHOD_OPTIONS,
        TAG_END());
        nua_options(m_pOperation->op_handle, TAG_END());
    } else {
        nua_options(m_pOperation->op_handle, TAG_END());
    }
    return true;
}

bool OptionsSession::accept()
{
	return true;
}

bool OptionsSession::reject()
{
	return true;
}

PublicationSession::PublicationSession(SipStack* stack, nua_handle_t* nh) :
        SipSession(stack)
{
	Sipe2eSofiaHelper sh;
	if (!nh) {
		m_pOperation = sh.createOperation(stack->getContext(), SIP_METHOD_PUBLISH,
			TAG_END());
	} else {
		m_pOperation = sh.createOperationWithHandle(stack->getContext(), SIP_METHOD_PUBLISH, nh);
	}
}

PublicationSession::~PublicationSession()
{

}

bool PublicationSession::publish(const char* payload)
{
    if (!payload) {
        return false;
    }
    const char* fmt = "" //
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
                    "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\"\r\n"
                    "    xmlns:im=\"urn:ietf:params:xml:ns:pidf:im\"\r\n"
                    "    entity=\"%s\">\r\n"
                    "  <tuple id=\"Sipe2eClient\">\r\n"
                    "    <status>\r\n"
                    "      <basic>%s</basic>\r\n"
                    "    </status>\r\n"
                    "    <note>%s</note>\r\n"
                    "  </tuple>\r\n"
                    "</presence>\r\n";
    bool open = true;
    Sipe2eContex* ctx = m_pStack->getContext();
    sip_payload_t *pl = sip_payload_format(ctx->sip_home, fmt,
            ctx->profile.impu, open ? "open" : "closed", payload);
    if (!m_pOperation) {
        char address[BUFFER_SIZE_B];
        strcpy(address, ctx->profile.impu);
        Sipe2eSofiaHelper sh;
        m_pOperation = sh.createOperation(ctx, SIP_METHOD_PUBLISH,
                TAG_END());
        nua_publish(m_pOperation->op_handle,
                SIPTAG_CONTENT_TYPE_STR("application/pidf+xml"),
                SIPTAG_PAYLOAD(pl),
                TAG_END());
    } else {
        nua_publish(m_pOperation->op_handle, SIPTAG_PAYLOAD(pl),
                TAG_IF(pl, SIPTAG_CONTENT_TYPE_STR("application/pidf+xml")),
                TAG_END());
    }
    su_free(ctx->sip_home, pl);
    return true;
}
bool PublicationSession::unPublish()
{
    if (!m_pOperation) {
        Sipe2eContex* ctx = m_pStack->getContext();
        Sipe2eSofiaHelper sh;
        m_pOperation = sh.createOperation(ctx, SIP_METHOD_PUBLISH,
                TAG_END());
        nua_publish(m_pOperation->op_handle, SIPTAG_EXPIRES_STR("0"),
        TAG_END());
    } else {
        nua_publish(m_pOperation->op_handle, SIPTAG_EXPIRES_STR("0"),
        TAG_END());
    }
    return true;
}

RegistrationSession::RegistrationSession(SipStack* stack, nua_handle_t* nh) :
        SipSession(stack)
{
	Sipe2eSofiaHelper sh;
	if (!nh) {
		m_pOperation = sh.createOperation(stack->getContext(), SIP_METHOD_REGISTER,
			TAG_END());
	} else {
		m_pOperation = sh.createOperationWithHandle(stack->getContext(), SIP_METHOD_REGISTER, nh);
	}
}

RegistrationSession::~RegistrationSession()
{

}


bool RegistrationSession::setToUri(const char* toUriString)
{
	if (!toUriString || !m_pOperation || !m_pOperation->op_handle) {
		return false;
	}
	nua_handle_t* hdl = m_pOperation->op_handle;
    nua_set_hparams(hdl, TAG_IF(toUriString, NUTAG_REGISTRAR(toUriString)),
    TAG_END());
    return true;
}

bool RegistrationSession::register_()
{
    if (!m_pOperation) {
        nua_register(m_pOperation->op_handle, NUTAG_KEEPALIVE(0),
                NUTAG_KEEPALIVE_STREAM(0), NUTAG_OUTBOUND("no-validate"),
                NUTAG_OUTBOUND("no-options-keepalive"),
                /*TAG_IF(registrar, NUTAG_REGISTRAR(registrar)),*/
                /*NUTAG_M_FEATURES("expires=180"), SIPTAG_EXPIRES_STR("180"),*/
                TAG_END());
    } else {
		nua_register(m_pOperation->op_handle, NUTAG_KEEPALIVE(0),
			NUTAG_KEEPALIVE_STREAM(0), NUTAG_OUTBOUND("no-validate"),
			NUTAG_OUTBOUND("no-options-keepalive"),
			TAG_END());
    }
    return true;
}
bool RegistrationSession::unRegister()
{
    if (!m_pOperation) {
        nua_unregister(m_pOperation->op_handle,
                /*TAG_IF(registrar, NUTAG_REGISTRAR(registrar)),*/
                SIPTAG_CONTACT_STR("*"), SIPTAG_EXPIRES_STR("0"),
                TAG_END());
    } else {
        nua_unregister(m_pOperation->op_handle, 
			SIPTAG_CONTACT_STR("*"), SIPTAG_EXPIRES_STR("0"),
			TAG_END());
    }
    return true;
}

SubscriptionSession::SubscriptionSession(SipStack* stack, nua_handle_t* nh) :
        SipSession(stack)
{
	Sipe2eSofiaHelper sh;
	if (!nh) {
		m_pOperation = sh.createOperation(stack->getContext(), SIP_METHOD_SUBSCRIBE,
			TAG_END());
	} else {
		m_pOperation = sh.createOperationWithHandle(stack->getContext(), SIP_METHOD_SUBSCRIBE, nh);
	}
}

SubscriptionSession::~SubscriptionSession()
{

}

bool SubscriptionSession::subscribe()
{
    char const *event = "presence";
    char const *supported = "eventlist";

    if (!m_pOperation) {
        nua_subscribe(m_pOperation->op_handle, SIPTAG_EXPIRES_STR("3600"),
                SIPTAG_ACCEPT_STR("application/cpim-pidf+xml;q=0.5, "
                        "application/pidf-partial+xml"),
                TAG_IF(supported,
                        SIPTAG_ACCEPT_STR("multipart/related, " "application/rlmi+xml")),
                SIPTAG_SUPPORTED_STR(supported), SIPTAG_EVENT_STR(event),
                TAG_END());
    } else {
		nua_subscribe(m_pOperation->op_handle, SIPTAG_EXPIRES_STR("3600"),
			SIPTAG_ACCEPT_STR("application/cpim-pidf+xml;q=0.5, "
			"application/pidf-partial+xml"),
			TAG_IF(supported,
			SIPTAG_ACCEPT_STR("multipart/related, " "application/rlmi+xml")),
			SIPTAG_SUPPORTED_STR(supported), SIPTAG_EVENT_STR(event),
			TAG_END());
    }
    return true;
}

bool SubscriptionSession::unSubscribe()
{
    char const *event = "presence";
    char const *supported = "eventlist";

    if (!m_pOperation) {
        nua_unsubscribe(m_pOperation->op_handle, TAG_END());
    } else {
        nua_unsubscribe(m_pOperation->op_handle, TAG_END());
    }
    return true;
}
