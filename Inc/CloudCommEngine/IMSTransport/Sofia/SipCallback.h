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

#ifndef SIPCALLBACK_H__
#define SIPCALLBACK_H__

#include "SipCommon.h"
#include <sofia-sip/nua.h>

class Sipe2eSofiaEvent;
class SipMessage;
class MessagingSession;
class OptionsSession;
class SipStack;

class SOFIA_IMS_EXPORT_FUN SipEvent
{
public:
    SipEvent(Sipe2eSofiaEvent* ev);
	/**
	 * Create a SipMessage which should be released after use by the caller
	 * @param - 
	 */
	SipMessage* GetSipMessage() const;
	nua_event_t GetType() const;
protected:
    Sipe2eSofiaEvent* ev;

};

class SOFIA_IMS_EXPORT_FUN StackEvent: public SipEvent
{
public:
    StackEvent(Sipe2eSofiaEvent* ev);
};
class SOFIA_IMS_EXPORT_FUN DialogEvent: public SipEvent
{
public:
    DialogEvent(Sipe2eSofiaEvent* ev);
};

class SOFIA_IMS_EXPORT_FUN InviteEvent: public SipEvent
{
public:
    InviteEvent(Sipe2eSofiaEvent* ev);
};

class SOFIA_IMS_EXPORT_FUN MessagingEvent: public SipEvent
{
public:
    MessagingEvent(Sipe2eSofiaEvent* ev);
	MessagingSession* GetSession(SipStack* stack);
    bool is_incoming;
};

class SOFIA_IMS_EXPORT_FUN InfoEvent: public SipEvent
{
public:
    InfoEvent(Sipe2eSofiaEvent* ev);
};

class SOFIA_IMS_EXPORT_FUN OptionsEvent: public SipEvent
{
public:
    OptionsEvent(Sipe2eSofiaEvent* ev);
	OptionsSession* GetSession(SipStack* stack);
};

class SOFIA_IMS_EXPORT_FUN PublicationEvent: public SipEvent
{
public:
    PublicationEvent(Sipe2eSofiaEvent* ev);
};

class SOFIA_IMS_EXPORT_FUN RegistrationEvent: public SipEvent
{
public:
    RegistrationEvent(Sipe2eSofiaEvent* ev);
};

class SOFIA_IMS_EXPORT_FUN SubscriptionEvent: public SipEvent
{
public:
    SubscriptionEvent(Sipe2eSofiaEvent* ev);
};

class SOFIA_IMS_EXPORT_FUN NotifyEvent: public SipEvent
{
public:
    NotifyEvent(Sipe2eSofiaEvent* ev);
    bool is_incoming; // always true
};

class SOFIA_IMS_EXPORT_FUN SipCallback
{
public:
    SipCallback()
    {
    }
    virtual ~SipCallback()
    {
    }
    virtual int OnDialogEvent(const DialogEvent* e)
    {
        return -1;
    }
    virtual int OnStackEvent(const StackEvent* e)
    {
        return -1;
    }

    virtual int OnInviteEvent(const InviteEvent* e)
    {
        return -1;
    }
    virtual int OnMessagingEvent(const MessagingEvent* e)
    {
        return -1;
    }
    virtual int OnInfoEvent(const InfoEvent* e)
    {
        return -1;
    }
    virtual int OnOptionsEvent(const OptionsEvent* e)
    {
        return -1;
    }
    virtual int OnPublicationEvent(const PublicationEvent* e)
    {
        return -1;
    }
    virtual int OnRegistrationEvent(const RegistrationEvent* e)
    {
        return -1;
    }
    virtual int OnSubscriptionEvent(const SubscriptionEvent* e)
    {
        return -1;
    }
    virtual int OnIncomingMessagingEvent(const MessagingEvent* e)
    {
        return -1;
    }
    virtual int OnIncomingNotifyEvent(const NotifyEvent* e)
    {
        return -1;
    }

private:

};

#endif
