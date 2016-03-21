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

#include "SipCallback.h"

class MessageCallbackHelper {
  public:
    void handle(
        nua_event_t event,
        int status,
        const char* phrase,
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[])
    {
        if (event == nua_i_message) {
            sip_i_message(nua, ssc, nh, op, sip, tags);
        } else if (event == nua_r_message) {
            sip_r_message(status, phrase, nua, ssc, nh, op, sip, tags);
        }
    }

    void sip_i_message(
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[])
    {
        /* Incoming message */
        const sip_from_t* from;
        const sip_to_t* to;
        const sip_subject_t* subject;
        assert(sip);
        from = sip->sip_from;
        to = sip->sip_to;
        subject = sip->sip_subject;
        assert(from && to);
        printf("\tFrom: %s%s" URL_PRINT_FORMAT "\n",
               from->a_display ? from->a_display : "",
               from->a_display ? " " : "", URL_PRINT_ARGS(from->a_url));
        if (subject) {
            printf("\tSubject: %s\n", subject->g_value);
        }

        SipE2eSofiaHelper sh;
        if (op == NULL) {
            op = sh.createOperationWithHandle(
                ssc,
                SIP_METHOD_MESSAGE,
                nh);
        }

        if (op == NULL) {
            nua_handle_destroy(nh);
        }
    }

    void sip_r_message(
        int status,
        const char* phrase,
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[])
    {
        if (status < 200) {
            return;
        }

        SipE2eSofiaHelper sh;
        if (status == 401 || status == 407) {
            sh.authenticate(ssc, op, sip, tags);
        }
    }

};

class NotifyCallbackHelper {
  public:
    void handle(
        nua_event_t event,
        int status,
        const char* phrase,
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[])
    {
        if (event == nua_i_notify) {
            sip_i_notify(nua, ssc, nh, op, sip, tags);
        } else if (event == nua_r_notify) {
            sip_r_notify(status, phrase, nua, ssc, nh, op, sip, tags);
        }
    }

    void sip_i_notify(
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[])
    {
        if (sip) {
            sip_event_t const*event = sip->sip_event;
            sip_content_type_t const*content_type = sip->sip_content_type;

            if (!op) {
                //      printf("%s: rogue NOTIFY from " URL_PRINT_FORMAT "\n",
                //              ssc->sip_name, URL_PRINT_ARGS(from->a_url));
            }
            if (event) {
                printf("\tEvent: %s\n", event->o_type);
            }
            if (content_type) {
                printf("\tContent type: %s\n", content_type->c_type);
            }
            fputs("\n", stdout);
        }
    }

    void sip_r_notify(
        int status,
        const char* phrase,
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[])
    {
        /* Respond to notify */
        if (status < 200) {
            return;
        }

        SipE2eSofiaHelper sh;
        if (status == 401 || status == 407) {
            sh.authenticate(ssc, op, sip, tags);
        }
    }

};

class OptionsCallbackHelper {
  public:
    void handle(
        nua_event_t event,
        int status,
        const char* phrase,
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[])
    {
        if (event == nua_i_options) {
            sip_i_options(nua, ssc, nh, op, sip, tags);
        } else if (event == nua_r_options) {
            sip_r_options(status, phrase, nua, ssc, nh, op, sip, tags);
        }
    }

    /**
     * Callback to an incoming OPTIONS request.
     */
    void sip_i_options(
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[])
    {
    }

    /**
     * Callback to an outgoing OPTIONS request.
     */
    void sip_r_options(
        int status,
        const char* phrase,
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[])
    {
        SipE2eSofiaHelper sh;
        if (status == 401 || status == 407) {
            sh.authenticate(ssc, op, sip, tags);
        }
    }

};

class PublishCallbackHelper {
  public:
    void handle(
        nua_event_t event,
        int status,
        const char* phrase,
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[])
    {
        if (event == nua_r_publish) {
            sip_r_publish(status, phrase, nua, ssc, nh, op, sip, tags);
        } else if (event == nua_r_unpublish) {
        }
    }

    /**
     * Callback for an outgoing PUBLISH request.
     */
    void sip_r_publish(
        int status,
        const char* phrase,
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[])
    {
        if (status < 200) {
            return;
        }

        SipE2eSofiaHelper sh;
        if (status == 401 || status == 407) {
            sh.authenticate(ssc, op, sip, tags);
        }
    }

};
class RegisterCallbackHelper {
  public:
    void handle(
        nua_event_t event,
        int status,
        const char* phrase,
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[])
    {
        if (event == nua_r_register) {
            sip_r_register(status, phrase, nua, ssc, nh, op, sip, tags);
        } else if (event == nua_r_unregister) {
            sip_r_unregister(status, phrase, nua, ssc, nh, op, sip, tags);
        }
    }
    void sip_r_register(
        int status,
        const char* phrase,
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[])
    {
        // if the status is 200OK, then just ignore it and pass it to upper layer
        if (status <= 200) {
            return;
        }

        SipE2eSofiaHelper sh;
        if (status == 401 || status == 407) {
            sh.authenticate(ssc, op, sip, tags);
        }
        sipe2e_log("sip_r_register %u\n", status);
    }

    void sip_r_unregister(
        int status,
        const char* phrase,
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[])
    {
        // if the status is 200OK, then just ignore it and pass it to upper layer
        if (status <= 200) {
            return;
        }

        SipE2eSofiaHelper sh;
        if (status == 401 || status == 407) {
            sh.authenticate(ssc, op, sip, tags);
        }
    }

};

class SubscribeCallbackHelper {
  public:
    void handle(
        nua_event_t event,
        int status,
        const char* phrase,
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[])
    {
        if (event == nua_r_subscribe) {
            sip_r_subscribe(status, phrase, nua, ssc, nh, op, sip, tags);
        } else if (event == nua_r_unsubscribe) {
            sip_r_unsubscribe(status, phrase, nua, ssc, nh, op, sip, tags);
        }
    }

    void sip_r_subscribe(
        int status,
        const char* phrase,
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[])
    {
        if (status < 200) {
            return;
        }

        SipE2eSofiaHelper sh;
        if (status == 401 || status == 407) {
            sh.authenticate(ssc, op, sip, tags);
        }
    }

    void sip_r_unsubscribe(
        int status,
        const char* phrase,
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[])
    {
        if (status < 200) {
            return;
        }

        SipE2eSofiaHelper sh;
        if (status == 401 || status == 407) {
            sh.authenticate(ssc, op, sip, tags);
        }
    }

};

class SipCallbackHelper {
  private:
    nua_event_t event;
    int status;
    const char* phrase;
    nua_t* nua;
    SipE2eContext* ssc;
    nua_handle_t* nh;
    SipE2eOperation* op;
    const sip_t* sip;
    tagi_t* tags;

    SipE2eSofiaEvent ev;

    int OnDialogEvent()
    {
        return 0;
    }
    int OnStackEvent()
    {
        return 0;
    }

    int OnInviteEvent()
    {
        return 0;
    }

    int OnMessagingEvent()
    {
        MessageCallbackHelper ch;
        ch.handle(event, status, phrase, nua, ssc, nh, op, sip, tags);
        if (ssc->sip_callback) {
            MessagingEvent evt(&ev);
            ssc->sip_callback->OnMessagingEvent(&evt);
        }
        return 0;
    }

    int OnInfoEvent()
    {
        return 0;
    }

    int OnOptionsEvent()
    {
        OptionsCallbackHelper ch;
        ch.handle(event, status, phrase, nua, ssc, nh, op, sip, tags);
        if (ssc->sip_callback) {
            OptionsEvent evt(&ev);
            ssc->sip_callback->OnOptionsEvent(&evt);
        }
        return 0;
    }

    int OnPublicationEvent()
    {
        PublishCallbackHelper ch;
        ch.handle(event, status, phrase, nua, ssc, nh, op, sip, tags);
        if (ssc->sip_callback) {
            PublicationEvent evt(&ev);
            ssc->sip_callback->OnPublicationEvent(&evt);
        }
        return 0;
    }

    int OnRegistrationEvent()
    {
        RegisterCallbackHelper ch;
        ch.handle(event, status, phrase, nua, ssc, nh, op, sip, tags);
        if (ssc->sip_callback) {
            RegistrationEvent evt(&ev);
            ssc->sip_callback->OnRegistrationEvent(&evt);
        }
        return 0;
    }

    int OnSubscriptionEvent()
    {
        SubscribeCallbackHelper ch;
        ch.handle(event, status, phrase, nua, ssc, nh, op, sip, tags);
        if (ssc->sip_callback) {
            SubscriptionEvent evt(&ev);
            ssc->sip_callback->OnSubscriptionEvent(&evt);
        }
        return 0;
    }

//    int OnIncomingMessagingEvent()
//    {
//        return 0;
//    }
//
//    int OnIncomingNotifyEvent()
//    {
//        return 0;
//    }

  public:
    SipCallbackHelper(
        nua_event_t event,
        int status,
        const char* phrase,
        nua_t* nua,
        SipE2eContext* ssc,
        nua_handle_t* nh,
        SipE2eOperation* op,
        const sip_t* sip,
        tagi_t tags[]) :
        event(event),
        status(status),
        phrase(phrase),
        nua(nua),
        ssc(ssc),
        nh(nh),
        op(op),
        sip(sip),
        tags(tags),
        ev(event, status, phrase, nua, ssc, nh, op, sip, tags)
    {
    }

    int dispatchEvent()
    {
        switch (event) {
        case nua_r_shutdown:
            //            sip_r_shutdown(status, phrase, nua, ssc, nh, op, sip, tags);
            break;

        case nua_r_get_params:
            //            sip_r_get_params(status, phrase, nua, ssc, nh, op, sip, tags);
            break;

        case nua_r_register:
            OnRegistrationEvent();
            break;

        case nua_r_unregister:
            OnRegistrationEvent();
            break;

        case nua_i_options:
            OnOptionsEvent();
            break;

        case nua_r_options:
            OnOptionsEvent();
            break;

        case nua_r_invite:
            //    sip_r_invite(status, phrase, nua, ssc, nh, op, sip, tags);
            break;

        case nua_i_fork:
            //    sip_i_fork(status, phrase, nua, ssc, nh, op, sip, tags);
            break;

        case nua_i_invite:
            //    sip_i_invite(nua, ssc, nh, op, sip, tags);
            break;

        case nua_i_prack:
            //    sip_i_prack(nua, ssc, nh, op, sip, tags);
            break;

        case nua_i_state:
            //    sip_i_state(status, phrase, nua, ssc, nh, op, sip, tags);
            break;

        case nua_r_bye:
            //    sip_r_bye(status, phrase, nua, ssc, nh, op, sip, tags);
            break;

        case nua_i_bye:
            //    sip_i_bye(nua, ssc, nh, op, sip, tags);
            break;

        case nua_r_message:
            OnMessagingEvent();
            break;

        case nua_i_message:
            OnMessagingEvent();
            break;

        case nua_r_info:
            //    sip_r_info(status, phrase, nua, ssc, nh, op, sip, tags);
            break;

        case nua_i_info:
            //    sip_i_info(nua, ssc, nh, op, sip, tags);
            break;

        case nua_r_refer:
            //    sip_r_refer(status, phrase, nua, ssc, nh, op, sip, tags);
            break;

        case nua_i_refer:
            //    sip_i_refer(nua, ssc, nh, op, sip, tags);
            break;

        case nua_r_subscribe:
            OnSubscriptionEvent();
            break;

        case nua_r_unsubscribe:
            OnSubscriptionEvent();
            break;

        case nua_r_publish:
            OnPublicationEvent();
            break;

        case nua_r_unpublish:
            OnPublicationEvent();
            break;

        case nua_r_notify:
            //            sip_r_notify(status, phrase, nua, ssc, nh, op, sip, tags);
            break;

        case nua_i_notify:
            //            sip_i_notify(nua, ssc, nh, op, sip, tags);
            break;

        case nua_i_cancel:
            //    sip_i_cancel(nua, ssc, nh, op, sip, tags);
            break;

        case nua_i_error:
            //    sip_i_error(nua, ssc, nh, op, status, phrase, tags);
            break;

        case nua_i_active:
        case nua_i_ack:
        case nua_i_terminated:
            break;

        default:
            break;

        }
        return 0;
    }
};

void SipE2eSofiaHelper::dispatchEvent(
    nua_event_t event,
    int status,
    const char* phrase,
    nua_t* nua,
    SipE2eContext* ssc,
    nua_handle_t* nh,
    SipE2eOperation* op,
    const sip_t* sip,
    tagi_t tags[])
{
    SipCallbackHelper cb(event, status, phrase, nua, ssc, nh, op, sip, tags);
    cb.dispatchEvent();
}

SipEvent::SipEvent(SipE2eSofiaEvent* ev) : ev(ev)
{

}

SipMessage* SipEvent::GetSipMessage() const
{
    if (ev) {
        return new SipMessage(ev->ssc, ev->sip);
    }
    return nullptr;
}

nua_event_t SipEvent::GetType() const
{
    if (ev) {
        return ev->event;
    }
    return nua_event_t::nua_i_none;
}

int SipEvent::GetStatus() const
{
    if (ev) {
        return ev->status;
    }
    return -1;
}

MessagingEvent::MessagingEvent(SipE2eSofiaEvent* ev) :
    SipEvent(ev),
    is_incoming(true)
{

}

MessagingSession* MessagingEvent::GetSession(SipStack* stack)
{
    if (ev) {
        return new MessagingSession(stack, ev->nh);
    }
    return NULL;
}

OptionsEvent::OptionsEvent(SipE2eSofiaEvent* ev) :
    SipEvent(ev)
{

}

OptionsSession* OptionsEvent::GetSession(SipStack* stack)
{
    if (ev) {
        return new OptionsSession(stack, ev->nh);
    }
    return NULL;
}

StackEvent::StackEvent(SipE2eSofiaEvent* ev) :
    SipEvent(ev)
{

}

DialogEvent::DialogEvent(SipE2eSofiaEvent* ev) :
    SipEvent(ev)
{

}

InviteEvent::InviteEvent(SipE2eSofiaEvent* ev) :
    SipEvent(ev)
{

}

InfoEvent::InfoEvent(SipE2eSofiaEvent* ev) :
    SipEvent(ev)
{

}

PublicationEvent::PublicationEvent(SipE2eSofiaEvent* ev) :
    SipEvent(ev)
{

}

RegistrationEvent::RegistrationEvent(SipE2eSofiaEvent* ev) :
    SipEvent(ev)
{

}

SubscriptionEvent::SubscriptionEvent(SipE2eSofiaEvent* ev) :
    SipEvent(ev)
{

}

NotifyEvent::NotifyEvent(SipE2eSofiaEvent* ev) :
    SipEvent(ev),
    is_incoming(true)
{

}