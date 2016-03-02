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

#include "Sofia.h"
#include "SipCommon.h"
#include "SipStack.h"
#include "SipSession.h"
#include "SipCallback.h"
#include "SipMessage.h"

#ifdef WIN32
#   include <windows.h>
#else
#   include <net/if.h>
#   include <ifaddrs.h>
#endif

#ifdef WIN32
int SipE2eSofiaHelper::guessInterfaceAddress(int type, char *address,
        int size) {
    return 0;
}
#else
int SipE2eSofiaHelper::guessInterfaceAddress(int type, char *address, int size)
{
    struct ifaddrs *ifp;
    struct ifaddrs *ifpstart;
    int ret = -1;
    if (getifaddrs(&ifpstart) < 0) {
        return -1;
    }
    for (ifp = ifpstart; ifp != NULL; ifp = ifp->ifa_next) {
        if (ifp->ifa_addr && ifp->ifa_addr->sa_family == type
                && (ifp->ifa_flags & IFF_RUNNING)
                && !(ifp->ifa_flags & IFF_LOOPBACK)) {
            getnameinfo(ifp->ifa_addr,
                    (type == AF_INET6) ?
                            sizeof(struct sockaddr_in6) :
                            sizeof(struct sockaddr_in), address, size, NULL, 0,
                    NI_NUMERICHOST);
            if (strchr(address, '%') == NULL) { /*avoid ipv6 link-local addresses */
                ret = 0;
                break;
            }
        }
    }
    freeifaddrs(ifpstart);
    return ret;
}
#endif

void SipE2eSofiaHelper::guessVia(char *address_url, int port, bool is_tcp)
{
    char address[BUFFER_SIZE_B];
    int ec = guessInterfaceAddress(AF_INET, address, BUFFER_SIZE_B);
    sprintf(address_url, "sip:%s:%d;transport=%s", address, port,
            is_tcp ? "tcp" : "udp");
}
SipE2eOperation* SipE2eSofiaHelper::createOperation(SipE2eContext* ssc,
        sip_method_t method, const char* name, tag_type_t tag, tag_value_t value, ...)
{
    SipE2eOperation *op;
    ta_list ta;
/*
    sip_to_t *to;
    to = sip_to_make(ssc->sip_home, address);
    if (url_sanitize(to->a_url) < 0) {
        return NULL;
    }
*/
    if (!(op = (SipE2eOperation *) su_zalloc(ssc->sip_home, sizeof(*op)))) {
        return NULL;
    }
    op->op_ssc = ssc;
/*
    if (method == sip_method_register)
        have_url = 0;
*/
    ta_start(ta, tag, value);
    op->op_handle = nua_handle(ssc->sip_nua, op,
            ta_tags(ta));
    ta_end(ta);
/*
    su_free(ssc->sip_home, to);
*/
    return op;
}

SipE2eOperation* SipE2eSofiaHelper::createOperationWithHandle(SipE2eContext* ssc,
        sip_method_t method, const char* name, nua_handle_t* nh)
{
    SipE2eOperation* op;
    if ((op = (SipE2eOperation*) ((su_zalloc(ssc->sip_home, sizeof(*op)))))) {
        op->op_handle = nh;
        nua_handle_bind(op->op_handle, op);
        op->op_ssc = ssc;
    }
    return op;
}

void SipE2eSofiaHelper::deleteOperation(SipE2eContext* ssc, SipE2eOperation* op) {
	if (op) {
		su_free(ssc->sip_home, op);
	}
}

void SipE2eSofiaHelper::_authenticate(SipE2eContext* ssc, SipE2eOperation* op,
        const char* scheme, const char* realm)
{
    auto& profile = ssc->profile;
    su_home_t* home = ssc->sip_home;
    char authstring[BUFFER_SIZE_B];
    sprintf(authstring, "%s:%s:%s:%s", scheme, realm, profile.impi,
            profile.password);
//    sipe2e_log("%s\n", authstring);
    nua_authenticate(op->op_handle, NUTAG_AUTH(authstring), TAG_END());
}

void SipE2eSofiaHelper::authenticate(SipE2eContext* ssc, SipE2eOperation* op,
        const sip_t* sip, tagi_t* tags)
{
    su_home_t* home = ssc->sip_home;
    const sip_from_t* sipfrom = sip->sip_from;
    const sip_www_authenticate_t* wa = sip->sip_www_authenticate;
    const sip_proxy_authenticate_t* pa = sip->sip_proxy_authenticate;
    tl_gets(tags, SIPTAG_WWW_AUTHENTICATE_REF(wa),
            SIPTAG_PROXY_AUTHENTICATE_REF(pa), TAG_END());
    /*sipe2e_log("%s: %s was unauthorized\n", self->sip_name, op->op_method_name);*/
    if (wa) {
        sl_header_print(stdout, "Server auth: %s\n", (sip_header_t *) wa);
        const char *realm = msg_params_find(wa->au_params, "realm=");
        _authenticate(ssc, op, wa->au_scheme, realm);
    }
    if (pa) {
        sl_header_print(stdout, "Proxy auth: %s\n", (sip_header_t *) pa);
        const char *realm = msg_params_find(pa->au_params, "realm=");
        _authenticate(ssc, op, pa->au_scheme, realm);
    }
}

/***
 *
 * code below is just for test purpose.
 *
 *
 *
 *
 * */
class MyTest
{
    struct MyCallback: public SipCallback
    {
        virtual int OnRegistrationEvent(const RegistrationEvent* e) override
        {
            SipMessage* m = e->GetSipMessage();
            char* v = m->getSipHeaderValue("From", 0);
            sipe2e_log("rrrrrrrrr%sttttttttt\n", v);
            return 0;
        }
    } callback;
    struct TestContext
    {
        SipStack* stack;
        SipE2eContext* context;
        RegistrationSession* pRegistrationSession;
        OptionsSession* pOptionsSession;
        MessagingSession* pMessagingSession;
        PublicationSession* pPublicationSession;
        SubscriptionSession* pSubscriptionSession;
        TestContext() :
                stack(0), context(0), pRegistrationSession(0), //
                pOptionsSession(0), pMessagingSession(0), //
                pPublicationSession(0), pSubscriptionSession(0)
        {
        }
    } test;
public:
    MyTest() :
            use_nane(true), is_alice(true)
    {
    }
private:
    bool use_nane;
    bool is_alice;
    SipE2eUserProfile profile;
    char peer_impu[BUFFER_SIZE_B];

private:
    static gboolean one_run(gpointer data)
    {
        static int step = 0;
        sipe2e_log("loop called %d times\n", step);
        MyTest* _this = (MyTest*) data;
        gboolean r = _this->run_step(step);
        step++;
        return r;
    }
    gboolean run_step(int step)
    {
        if (step == 1) {            // 0.1 second
            test.stack = SipStack::getInstance();
            test.context = test.stack->getContext();
        } else if (step == 10) {       // 1 second
            test.pRegistrationSession = new RegistrationSession(test.stack);
            SipE2eContext* ctx = test.stack->getContext();
            char registrar[BUFFER_SIZE_B];
            sprintf(registrar, "%s:%s", "sip", ctx->profile.realm);
            test.pRegistrationSession->setToUri(registrar);
            test.pRegistrationSession->register_();
        } else if (step == 30) {
            test.pOptionsSession = new OptionsSession(test.stack);
            SipE2eContext* ctx = test.stack->getContext();
            char registrar[BUFFER_SIZE_B];
            sprintf(registrar, "%s:%s", "sip", ctx->profile.realm);
            test.pOptionsSession->setToUri(registrar);
            test.pOptionsSession->send();
        } else if (step == 50) {
            if (is_alice) {

                test.pSubscriptionSession = //
                        new SubscriptionSession(test.stack);
				test.pSubscriptionSession->setToUri(peer_impu);
				test.pSubscriptionSession->addHeader("Event", "presence");
                test.pSubscriptionSession->subscribe();
            }
        } else if (step == 100) {
            if (is_alice) {
                string s = "hello, world!";
                test.pMessagingSession = //
                        new MessagingSession(test.stack);
				test.pMessagingSession->setToUri(peer_impu);
                test.pMessagingSession->send(s.c_str());
            }
        } else if (step == 120) {
            if (!is_alice) {
                string s = "hello, service!";
                test.pPublicationSession = //
                        new PublicationSession(test.stack);
                test.pPublicationSession->publish(s.c_str());
            }
        } else if (step == 150) {
            if (!is_alice) {
                test.pPublicationSession->setExpires(0);
                test.pPublicationSession->unPublish();
            }
        } else if (step == 230) {
            if (is_alice) {
                test.pSubscriptionSession->setExpires(0);
                test.pSubscriptionSession->unSubscribe();
            }
        } else if (step == 260) {
            test.pRegistrationSession->unRegister();
        } else if (step == 370) {
//            test.stack->stop();
        }
        return true;
    }

private:
    void init_alice_local_profile()
    {
        strcpy(profile.name, "Alice");
        strcpy(profile.impu, "sip:alice@open-ims.test");
        strcpy(profile.impi, "alice@open-ims.test");
        strcpy(profile.pcscf, "pcscf.open-ims.test:4060");
        strcpy(profile.realm, "open-ims.test");
        strcpy(profile.password, "alice");
        profile.local_port = 2060;
        strcpy(peer_impu, "sip:bob@open-ims.test");
    }
    void init_alice_nane_profile()
    {
        strcpy(profile.name, "Alice");
        strcpy(profile.impu, "sip:luoyongheng@nane.cn");
        strcpy(profile.impi, "luoyongheng@nane.cn");
        strcpy(profile.pcscf, "pcscf.nane.cn:4060");
        strcpy(profile.realm, "nane.cn");
        strcpy(profile.password, "luoyongheng");
        profile.local_port = 2060;
        strcpy(peer_impu, "sip:wuaihua@nane.cn");
    }
    void init_bob_local_profile()
    {
        strcpy(profile.name, "Bob");
        strcpy(profile.impu, "sip:bob@open-ims.test");
        strcpy(profile.impi, "bob@open-ims.test");
        strcpy(profile.pcscf, "pcscf.open-ims.test:4060");
        strcpy(profile.realm, "open-ims.test");
        strcpy(profile.password, "bob");
        profile.local_port = 3060;
        strcpy(peer_impu, "sip:alice@open-ims.test");
    }
    void init_bob_nane_profile()
    {
        strcpy(profile.name, "Bob");
        strcpy(profile.impu, "sip:wuaihua@nane.cn");
        strcpy(profile.impi, "wuaihua@nane.cn");
        strcpy(profile.pcscf, "pcscf.nane.cn:4060");
        strcpy(profile.realm, "nane.cn");
        strcpy(profile.password, "wuaihua");
        profile.local_port = 3060;
        strcpy(peer_impu, "sip:luoyongheng@nane.cn");
    }

private:
    void init_profile()
    {
        if (is_alice && !use_nane) {
            init_alice_local_profile();
        } else if (is_alice && use_nane) {
            init_alice_nane_profile();
        } else if (!is_alice && !use_nane) {
            init_bob_local_profile();
        } else if (!is_alice && use_nane) {
            init_bob_nane_profile();
        }
    }
    bool init_stack()
    {
        char fqdn[BUFFER_SIZE_B];
        char* pch = strtok(profile.pcscf, ":");
        strcpy(fqdn, pch);
        pch = strtok(nullptr, ":");
        int port = (int) strtol(pch, nullptr, 10);
        //
        SipStack* s = SipStack::makeInstance(&callback, profile.realm,
                profile.impi, profile.impu, profile.local_port);
        s->setProxyCSCF(fqdn, port, "udp", "ipv4");
        s->setPassword(profile.password);
        s->initialize();
        return true;
    }

    bool init_loop()
    {
        SipStack* s = SipStack::getInstance();
        SipE2eContext* ctx = s->getContext();
        g_timeout_add(100, one_run, this);
        return true;
    }

private:
    void init_from_args(int argc, char *argv[])
    {
        if (argc == 2 && strcmp("bob", argv[1]) == 0)
            is_alice = false;
        else
            is_alice = true;
        init_profile();
        init_stack();
        init_loop();
    }
    void run()
    {
        SipStack* s = SipStack::getInstance();
        SipE2eContext* ctx = s->getContext();
        g_main_loop_run(ctx->loop);
    }
    void deinit()
    {
        SipStack* s = SipStack::getInstance();
        SipE2eContext* ctx = s->getContext();
        g_main_loop_unref(ctx->loop);
        s->deInitialize();
    }
public:
    int main(int argc, char *argv[])
    {
        init_from_args(argc, argv);
        run();
        deinit();
        return 0;
    }
};

int __sofia_main__(int argc, char *argv[])
{
    MyTest t;
    t.main(argc, argv);
    return 0;
}
