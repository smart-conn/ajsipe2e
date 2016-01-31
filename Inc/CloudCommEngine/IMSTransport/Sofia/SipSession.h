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

#ifndef SIPSESSION_H__
#define SIPSESSION_H__

#include "SipCommon.h"

struct nua_handle_s;
typedef struct nua_handle_s nua_handle_t;

class SipStack;
class Sipe2eOperation;

class SOFIA_IMS_EXPORT_FUN SipSession
{
public:
    SipSession(SipStack* stack);
    virtual ~SipSession();

	bool addHeader(const char* name, const char* value);
	bool removeHeader(const char* name);
	bool addCaps(const char* name, const char* value);
	bool addCaps(const char* name);
	bool removeCaps(const char* name);
	bool setExpires(unsigned expires);
	bool setFromUri(const char* fromUriString);
	virtual bool setToUri(const char* toUriString);

protected:
    const SipStack* m_pStack;
    Sipe2eOperation* m_pOperation;
};

class SOFIA_IMS_EXPORT_FUN MessagingSession : public SipSession
{
public:
    MessagingSession(SipStack* pStack, nua_handle_t* nh = NULL);
    virtual ~MessagingSession();

public:
    bool send(const char* payload);
	bool accept();
	bool reject();
private:
};

class SOFIA_IMS_EXPORT_FUN OptionsSession : public SipSession
{
public:
    OptionsSession(SipStack* pStack, nua_handle_t* nh = NULL);
    virtual ~OptionsSession();

public:
    bool send();
	bool accept();
	bool reject();
};

class SOFIA_IMS_EXPORT_FUN PublicationSession : public SipSession
{
public:
    PublicationSession(SipStack* pStack, nua_handle_t* nh = NULL);
    virtual ~PublicationSession();

public:
    bool publish(const char* payload);
    bool unPublish();
};

class SOFIA_IMS_EXPORT_FUN RegistrationSession : public SipSession
{
public:
    RegistrationSession(SipStack* pStack, nua_handle_t* nh = NULL);
    virtual ~RegistrationSession();

    virtual bool setToUri(const char* toUriString) override;
public:
    bool register_();
    bool unRegister();
};

class SOFIA_IMS_EXPORT_FUN SubscriptionSession : public SipSession
{
public:
    SubscriptionSession(SipStack* pStack, nua_handle_t* nh = NULL);
    virtual ~SubscriptionSession();

public:
    bool subscribe();
    bool unSubscribe();
private:
};

#endif
