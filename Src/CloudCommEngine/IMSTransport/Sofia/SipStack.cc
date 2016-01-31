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

#include "SipStack.h"
#include "Sofia.h"
#include "SipCommon.h"

SipStack::SipStack() :
        g_bInitialized(false), m_pContext(0)
{
}

SipStack::~SipStack()
{
    this->stop();

    if (m_pContext) {
        delete m_pContext;
        m_pContext = nullptr;
    }
}

SipStack* SipStack::makeInstance(SipCallback* pCallback, const char* realm_uri,
        const char* impi_uri, const char* impu_uri, int local_port)
{
    SipStack* s = getInstance();

//    s->m_pCallback = pCallback;
    s->m_pContext = new Sipe2eContex();
    s->m_pContext->sip_callback = pCallback;
    if (realm_uri) {
        strcpy(s->m_pContext->profile.realm, realm_uri);
    }
    if (impi_uri) {
        strcpy(s->m_pContext->profile.impi, impi_uri);
    }
    if (impu_uri) {
        strcpy(s->m_pContext->profile.impu, impu_uri);
    }
    s->m_pContext->profile.local_port = local_port;
    return s;
}

SipStack* SipStack::getInstance()
{
    static SipStack stack;
    return &stack;
}

static void sipstack_callback(nua_event_t event, int status, const char* phrase,
        nua_t* nua, Sipe2eContex* ssc, nua_handle_t* nh, Sipe2eOperation* op,
        const sip_t* sip, tagi_t tags[])
{
    Sipe2eSofiaHelper sh;
    sh.dispatchEvent(event, status, phrase, nua, ssc, nh, op, sip, tags);
}

bool SipStack::initialize()
{
    Sipe2eContex* ctx = m_pContext;
    ctx[0].sip_home[0].suh_size = (sizeof ctx);
    su_init();
    su_home_init(ctx->sip_home);
    ctx->sip_root = su_glib_root_create(ctx);
    if (ctx->sip_root != nullptr) {
        ctx->loop = g_main_loop_new(nullptr, false);
        GSource *gsource = su_root_gsource(ctx->sip_root);
        g_source_attach(gsource, g_main_loop_get_context(ctx->loop));
        Sipe2eSofiaHelper sh;
        char address_url[BUFFER_SIZE_B];
        sh.guessVia(address_url, ctx->profile.local_port, ctx->profile.is_tcp);
        sipe2e_log("%s\n", address_url);
        ctx->sip_nua = nua_create(ctx->sip_root,
                (nua_callback_f) sipstack_callback, (nua_magic_t *) ctx,
                NUTAG_URL(address_url),
                TAG_END());
        char route[BUFFER_SIZE_B];
        sprintf(route, "<sip:%s;lr>", ctx->profile.pcscf);
        nua_set_params(ctx->sip_nua, NUTAG_INITIAL_ROUTE_STR(route),
                NUTAG_SERVICE_ROUTE_ENABLE(1),
                TAG_END());
        g_bInitialized = true;
    }
    return g_bInitialized;
}

bool SipStack::deInitialize()
{
    Sipe2eContex* ctx = m_pContext;
    if (ctx->sip_root != nullptr) {
        GSource *source = su_glib_root_gsource(ctx->sip_root);
        g_source_unref(source);
        if (ctx->sip_nua != nullptr) {
            nua_destroy(ctx->sip_nua);
        }
        su_root_destroy(ctx->sip_root);
        ctx->sip_root = nullptr;
        g_main_loop_unref(ctx->loop);
    }
    su_home_deinit(ctx->sip_home);
    su_deinit();
    g_bInitialized = false;
    return true;
}

bool SipStack::start()
{
    if (isValid()) {
		g_main_loop_run(m_pContext->loop);
    } else {
		return false;
	}
    return true;
}

bool SipStack::stop()
{
    if (isValid()) {
        Sipe2eContex* ctx = this->m_pContext;
        nua_shutdown(ctx->sip_nua);
        g_main_loop_quit((GMainLoop*) ctx->loop);
    }
    return true;
}

bool SipStack::isValid()
{
    return (m_pContext != nullptr) && g_bInitialized;
}

bool SipStack::setRealm(const char* realm_uri)
{
    if (realm_uri) {
        strcpy(m_pContext->profile.realm, realm_uri);
    }
    return true;
}
bool SipStack::setIMPI(const char* impi_uri)
{
    if (impi_uri) {
        strcpy(m_pContext->profile.impi, impi_uri);
    }
    return true;
}
bool SipStack::setIMPU(const char* impu_uri)
{
    if (impu_uri) {
        strcpy(m_pContext->profile.impu, impu_uri);
    }
    return true;
}
bool SipStack::setPassword(const char* password)
{
    if (password) {
        strcpy(m_pContext->profile.password, password);
    }
    return true;
}
bool SipStack::setProxyCSCF(const char* fqdn, unsigned short port,
        const char* transport, const char* ipversion)
{
    Sipe2eContex* ctx = m_pContext;
    ctx->profile.is_tcp = (strcmp("tcp", transport) == 0);
    sprintf(ctx->profile.pcscf, "%s:%u", fqdn, port);
    sipe2e_log("%s\n", ctx->profile.pcscf);
    return true;
}

Sipe2eContex* SipStack::getContext() const
{
    return m_pContext;
}

bool SipStack::addHeader(const char* name, const char* value)
{
	// TODO
    return true;
}
