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

#ifndef IMSTRANSPORTSIPCALLBACK_H_
#define IMSTRANSPORTSIPCALLBACK_H_

#include "CloudCommEngine/IMSTransport/Sofia/SipCallback.h"

namespace sipe2e {
namespace gateway {

class IMSTransportSipCallback : public SipCallback {
  public:
    IMSTransportSipCallback();
    virtual ~IMSTransportSipCallback(void);

    virtual int OnDialogEvent(const DialogEvent* e);
    virtual int OnStackEvent(const StackEvent* e);

    virtual int OnRegistrationEvent(const RegistrationEvent* e);

    virtual int OnOptionsEvent(const OptionsEvent* e);

    virtual int OnMessagingEvent(const MessagingEvent* e);
    /**
     * Callback while receiving NOTIFY
     * @param e - the subscription event
     */
    virtual int OnSubscriptionEvent(const SubscriptionEvent* e);

    virtual int OnPublicationEvent(const PublicationEvent* e);

};

} // namespace gateway
} // namespace sipe2e

#endif // IMSTRANSPORTSIPCALLBACK_H_