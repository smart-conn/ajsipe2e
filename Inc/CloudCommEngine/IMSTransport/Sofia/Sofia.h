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

/**
 *
 * The api in this file should only be used internally.
 * e.g. included by *.cpp
 * it is *illegal* to be included in any exported *.h
 * it should never be exported in any release deployment.
 *
 * */

#pragma once

#include "SipCommon.h"
// the 3 magic need to be defined before the sofia-sip include.
#define NUA_MAGIC_T   class Sipe2eContex
#define NUA_IMAGIC_T  class Sipe2eOperation
#define NUA_HMAGIC_T  class Sipe2eOperation
#include <sofia-sip/sip_header.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_glib.h>
#include <sofia-sip/sl_utils.h>
#include <sofia-sip/nua.h>
#include <sofia-sip/nua_tag.h>

class Sipe2eUserProfile
{
public:
    char name[BUFFER_SIZE_B];
    char impu[BUFFER_SIZE_B];
    char impi[BUFFER_SIZE_B];
    char pcscf[BUFFER_SIZE_B];
    char realm[BUFFER_SIZE_B];
    char password[BUFFER_SIZE_B];
    bool is_tcp;
    int local_port;
    Sipe2eUserProfile() : local_port(5060), is_tcp(false)
    {
        name[0] = impu[0] = impi[0] = //
                pcscf[0] = realm[0] = password[0] = '\0';
    }
};

class Sipe2eContex
{
public:
    su_home_t sip_home[1]; /* memory home */
    su_root_t *sip_root; /* root object */
    nua_t *sip_nua; /* NUA stack object */
    class SipCallback* sip_callback;
    GMainLoop *loop;
    Sipe2eUserProfile profile;

};

class Sipe2eOperation
{
public:
    Sipe2eContex *op_ssc;
    nua_handle_t *op_handle;
};

class Sipe2eSofiaEvent
{
public:
    nua_event_t event;
    int status;
    const char* phrase;
    nua_t* nua;
    Sipe2eContex* ssc;
    nua_handle_t* nh;
    Sipe2eOperation* op;
    const sip_t* sip;
    tagi_t* tags;
    Sipe2eSofiaEvent(nua_event_t event, int status, const char* phrase,
            nua_t* nua, Sipe2eContex* ssc, nua_handle_t* nh,
            Sipe2eOperation* op, const sip_t* sip, tagi_t tags[]) :
            event(event), status(status), phrase(phrase), nua(nua), ssc(ssc), nh(
                    nh), op(op), sip(sip), tags(tags)
    {
    }
};

class Sipe2eSofiaHelper
{
public:
    Sipe2eOperation* createOperation(Sipe2eContex* ssc, sip_method_t method, const char* name,
            tag_type_t tag, tag_value_t value, ...);

    Sipe2eOperation* createOperationWithHandle(Sipe2eContex* ssc, sip_method_t method, const char* name,
		nua_handle_t* nh);

	void deleteOperation(Sipe2eContex* ssc, Sipe2eOperation* op);

    void authenticate(Sipe2eContex* ssc, Sipe2eOperation* op, const sip_t* sip,
            tagi_t* tags);
    void dispatchEvent(nua_event_t event, int status, const char* phrase,
            nua_t* nua, Sipe2eContex* ssc, nua_handle_t* nh,
            Sipe2eOperation* op, const sip_t* sip, tagi_t tags[]);
    void guessVia(char *address_url, int port, bool is_tcp);
private:
    int guessInterfaceAddress(int type, char *address, int size);
    void _authenticate(Sipe2eContex* ssc, Sipe2eOperation* op,
            const char* scheme, const char* realm);

};

// for test only
int __sofia_main__(int argc, char *argv[]);
