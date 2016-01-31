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

#ifndef SIPMESSAGE_H__
#define SIPMESSAGE_H__

#include "SipCommon.h"

#include <sofia-sip/sip.h>

class SOFIA_IMS_EXPORT_FUN SipMessage
{
public:
	SipMessage(const sip_t* _msg);;
	virtual ~SipMessage();

	sip_method_t getRequestType();
	int getResponseCode();
	/**
	 * Get well-known header values and all unknown header values, including no parameters
	 * @param - 
	 */
	char* getSipHeaderValue(const char* name, unsigned index = 0);
	/**
	 * Get parameter value of well-known headers
	 * @param - 
	 */
	char* getSipHeaderParamValue(const char* name, const char* param, unsigned index = 0);
	unsigned getSipContentLength();
	unsigned getSipContent(void* output, unsigned maxsize);
private:
	const sip_t* msg;
};

#endif
