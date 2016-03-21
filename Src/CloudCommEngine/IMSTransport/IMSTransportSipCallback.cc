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
#include <qcc/platform.h>
#include "CloudCommEngine/IMSTransport/IMSTransportSipCallback.h"
#include "CloudCommEngine/IMSTransport/ContentType.h"
#include "CloudCommEngine/IMSTransport/IMSTransportConstants.h"
#include "CloudCommEngine/IMSTransport/IMSTransport.h"

#include "CloudCommEngine/IMSTransport/Sofia/SipStack.h"
#include "CloudCommEngine/IMSTransport/Sofia/SipMessage.h"
#include "CloudCommEngine/IMSTransport/Sofia/SipSession.h"


namespace sipe2e {
namespace gateway {

IMSTransportSipCallback::IMSTransportSipCallback()
{
}


IMSTransportSipCallback::~IMSTransportSipCallback()
{
}

int IMSTransportSipCallback::OnDialogEvent(const DialogEvent* e)
{
    return 0;
}

int IMSTransportSipCallback::OnStackEvent(const StackEvent* e)
{
/*
    switch (e->getCode())
    {
    case tsip_event_code_stack_starting:
        {

        }
        break;
    case tsip_event_code_stack_started:
        {

        }
        break;
    case tsip_event_code_stack_failed_to_start:
        {

        }
        break;
    case tsip_event_code_stack_stopping:
        {

        }
        break;
    case tsip_event_code_stack_stopped:
        {

        }
        break;
    case tsip_event_code_stack_failed_to_stop:
        {

        }
        break;
    case tsip_event_code_stack_disconnected:
        {
            // when connection stopped due to, say, wireless issue
            // should report to the upper layer and the upper layer could remove the whole stack and reconstruct it
        }
        break;
    default:
        break;
    }
 */
    return 0;
}

int IMSTransportSipCallback::OnRegistrationEvent(const RegistrationEvent* e)
{
    switch (e->GetType()) {
    case nua_event_t::nua_r_register:
        {
            int resCode = e->GetStatus();
            IMSTransport* ims = IMSTransport::GetInstance();
            if (resCode >= 300 && resCode < 400) {
                // error occurs while trying to registering the UAC
                // retry to register immediately
                ims->regCmdQueue.Enqueue(ims->regExpires);
            } else if (resCode >= 401 && resCode < 600) {
                ims->imsTransportStatus = gwConsts::IMS_TRANSPORT_STATUS_UNREGISTERED;
            } else if (resCode >= 200) {
                // REGISTER successfully
                ims->imsTransportStatus = gwConsts::IMS_TRANSPORT_STATUS_REGISTERED;
                // The 'Service-Route' header will be like:
                //     Service-Route: <sip:orig@scscf.nane.cn:6060;lr>
                SipMessage* msg = e->GetSipMessage();
                if (!msg || !msg->isValid()) {
#ifndef NDEBUG
                    printf("Received a invalid response to SIP REGISTER\n");
#endif
                    return -1;
                }
                char* serviceRoute = msg->getSipHeaderValue("Service-Route");
                if (serviceRoute) {
                    char* atSymbol = strchr(serviceRoute, '@');
                    if (atSymbol) {
                        ims->scscf = atSymbol + 1;
                        size_t colon = ims->scscf.find_first_of(':');
                        if (colon != qcc::String::npos) {
                            ims->scscf.erase(colon, ims->scscf.size() - colon);
                        }
                    }
                }
                delete msg;
            }
        }
        break;

    case nua_event_t::nua_r_unregister:
        {
            int resCode = e->GetStatus();
            if (resCode >= 200 && resCode < 300) { // unregister successful
                IMSTransport* ims = IMSTransport::GetInstance();
                ims->condUnregister.notify_one();
                ims->imsTransportStatus = gwConsts::IMS_TRANSPORT_STATUS_UNREGISTERED;
            } else { // unregister failed
            }
        }
        break;

    default:
        break;
    }

    return 0;
}

int IMSTransportSipCallback::OnOptionsEvent(const OptionsEvent* e)
{
    switch (e->GetType()) {
    case nua_event_t::nua_i_options:
        {
            IMSTransport* ims = IMSTransport::GetInstance();
            OptionsSession* opSessionI = ((OptionsEvent*)e)->GetSession(ims->stack);
            if (opSessionI != NULL) {
//              ActionConfig* config = new ActionConfig();
//              config->addHeader("Allow", "INVITE, ACK, CANCEL, BYE, MESSAGE, OPTIONS, NOTIFY, PRACK, UPDATE, REFER");
                opSessionI->accept(/*config*/);
                delete opSessionI;
//              delete config;
            }
        }
        break;

    case nua_event_t::nua_r_options: // the response to the outgoing OPTIONS
        {
            IMSTransport* ims = IMSTransport::GetInstance();
            int resCode = e->GetStatus();
            if (resCode >= 200 && resCode < 300) {
//                 std::lock_guard<std::mutex> lock(ims->mtxHeartBeat);
                ims->condHeartBeat.notify_one();
                ims->imsTransportStatus = gwConsts::IMS_TRANSPORT_STATUS_REGISTERED;
            } else if (resCode >= 400 && resCode < 600) {
                // error occurs during HeartBeat, then re-register the UAC
                ims->regCmdQueue.Enqueue(ims->regExpires);
                ims->imsTransportStatus = gwConsts::IMS_TRANSPORT_STATUS_UNREGISTERED;
            }
        }
        break;

    default:
        break;
    }
    return 0;
}

int IMSTransportSipCallback::OnMessagingEvent(const MessagingEvent* e)
{
    switch (e->GetType()) {
    case nua_event_t::nua_i_message:
        {
            IMSTransport* ims = IMSTransport::GetInstance();
            SipMessage* msg = e->GetSipMessage();
            if (!msg || !msg->isValid()) {
#ifndef NDEBUG
                printf("Received a invalid SIP MESSAGE\n");
#endif
                return -1;
            }
            MessagingSession* session = ((MessagingEvent*)e)->GetSession(ims->stack);
            if (!session) {
                return -1;
            }
            if (msg) {
                session->accept();
                char* contentType = ((SipMessage*)msg)->getSipHeaderValue("c");
#ifndef NDEBUG
                printf("Received a SIP MESSAGE with content type '%s'\n", contentType);
#endif
                if (contentType) {
                    if (!strcmp(contentType, gwConsts::contenttype::ALLJOYN_XML)) {
                        // RPC message
                        // First, we should check it is a incoming request or a response for previous
                        // outgoing request. If it is a request, then we should get its From&CallID headers
                        // and make the content buffer like the format "lyh@nane.cn^CallID^Addr^request content"
                        // and hand it over to Axis' IMS Receiver . Here 'Addr' means
                        // the local called address, if it's AllJoyn, like 'BusName/ObjectPath/InterfaceName'
                        // If it is a response, then we should also get its From&CallID headers and try to
                        // find the outgoing request worker thread corresponding to From&CallID information
                        // like "lyh@nane.cn^CallID", and if found, invoke the outgoing worker thread and process
                        // the response
                        char* msgType = ((SipMessage*)msg)->getSipHeaderValue(gwConsts::customheader::RPC_MSG_TYPE);
                        if (!msgType) {
#ifndef NDEBUG
                            printf("Cannot get the msg type\n");
#endif
                            delete session;
                            delete msg;
                            return -1;
                        }
                        char* peer = ((SipMessage*)msg)->getSipHeaderValue("f");
                        if (!peer) {
#ifndef NDEBUG
                            printf("Cannot get the from header value\n");
#endif
                            delete session;
                            delete msg;
                            return -1;
                        }
                        char* tmp = strchr(peer, ':');
                        if (tmp) {
                            peer = tmp + 1; // if the address is like "sip:lyh@nane.cn", then just trim the "sip:"
                        }
                        if (strlen(peer) > gwConsts::MAX_SIP_ADDR_LEN) {
                            delete session;
                            delete msg;
                            return -1;
                        }
                        char*callId = ((SipMessage*)msg)->getSipHeaderValue(gwConsts::customheader::RPC_CALL_ID);
                        if (!callId || strlen(callId) > gwConsts::MAX_RPC_MSG_CALLID_LEN) {
#ifndef NDEBUG
                            printf("Cannot get the callid header value\n");
#endif
                            delete session;
                            delete msg;
                            return -1;
                        }


                        uint32_t contentLen = ((SipMessage*)msg)->getSipContentLength();
#ifndef NDEBUG
                        printf("Received a SIP MESSAGE:\nmsgType:%s\npeer:%s\ncontentLen:%i\n", msgType, peer, contentLen);
#endif
                        switch (atoi(msgType)) {
                        default:
                        case gwConsts::customheader::RPC_MSG_TYPE_METHOD_CALL:
                        case gwConsts::customheader::RPC_MSG_TYPE_PROPERTY_CALL:
                        case gwConsts::customheader::RPC_MSG_TYPE_SIGNAL_CALL:
                        case gwConsts::customheader::RPC_MSG_TYPE_UPDATE_SIGNAL_HANDLER:
                            {
                                // if it is an incoming method call request message
                                // Fill the request buffer as the format "lyh@nane.cn^CallID^Addr^Request Content"
                                char*addr = ((SipMessage*)msg)->getSipHeaderValue(gwConsts::customheader::RPC_ADDR);
                                if (!addr) {
                                    delete session;
                                    delete msg;
                                    return -1;
                                }
                                char* rpcXml = new char[contentLen + 3 + gwConsts::MAX_SIP_ADDR_LEN + gwConsts::MAX_RPC_MSG_CALLID_LEN + 2];
                                strcpy(rpcXml, msgType);
                                strcat(rpcXml, "^");
                                strcat(rpcXml, peer);
                                strcat(rpcXml, "^");
                                strcat(rpcXml, callId);
                                strcat(rpcXml, "^");
                                strcat(rpcXml, addr);
                                strcat(rpcXml, "^");
                                size_t len = strlen(rpcXml);
                                ((SipMessage*)msg)->getSipContent(&rpcXml[len], contentLen + 1);
                                rpcXml[len + contentLen] = '\0';

                                // Put the request in the incoming request buffer
                                ims->incomingMsgQueue.Enqueue(rpcXml);
                            }
                            break;

                        case gwConsts::customheader::RPC_MSG_TYPE_METHOD_RET:
                        case gwConsts::customheader::RPC_MSG_TYPE_PROPERTY_RET:
                        case gwConsts::customheader::RPC_MSG_TYPE_SIGNAL_RET:
                            {
                                // if it is a response message
                                qcc::String reqKey(peer);
                                reqKey += "^";
                                reqKey += callId;
                                {
                                    std::lock_guard<std::mutex> lock(ims->mtxResponseDispatchTable);
                                    std::map<qcc::String, std::shared_ptr<SyncContext> >::iterator itr =
                                        ims->responseDispatchTable.find(reqKey);
                                    if (itr != ims->responseDispatchTable.end()) {
                                        std::shared_ptr<SyncContext> syncCtx = itr->second;
                                        {
                                            std::lock_guard<std::mutex> lock(syncCtx->mtx);
                                            // Prepare the response buffer and pass it to the sender worker thread
                                            if (contentLen > 0) {
                                                char* rpcXml = new char[contentLen + 1];
                                                ((SipMessage*)msg)->getSipContent(rpcXml, contentLen + 1);
                                                syncCtx->content = rpcXml;
                                            }
                                            syncCtx->con.notify_one();
                                        }
                                        ims->responseDispatchTable.erase(itr);
                                    } else {
                                        // if the outgoing request worker thread is not found, just ignore it
                                    }
                                }
                            }
                            break;
                        }

                    }
                } else { // No content type available, just ignore it
                    //return -1;
                }
                delete session;
                delete msg;
            } else {
                session->reject();
                delete session;
                return -1;
            }
        }
        break;

    case nua_event_t::nua_r_message:     // the response to the outgoing messages
        {

        }
        break;

    default:
        break;

    }
    return 0;
}

int IMSTransportSipCallback::OnSubscriptionEvent(const SubscriptionEvent* e)
{
    switch (e->GetType()) {
    case nua_event_t::nua_i_notify:
        {
            IMSTransport* ims = IMSTransport::GetInstance();
            SipMessage* msg = e->GetSipMessage();
            if (msg && msg->isValid()) {
                char* contentType = ((SipMessage*)msg)->getSipHeaderValue("c");
                if (contentType) {
                    if (!strcmp(contentType, gwConsts::contenttype::PIDF)) {
                        // basic presence information notification

                        // First try to find the subscription information corresponding to
                        // the notification. How?

                        // Secondly try to get the content and subscribe the service described
                        // in the content to local by calling CloudCommEngine::SubscribeCloudServiceToLocal

                        // Get the peer address
                        char* peer = ((SipMessage*)msg)->getSipHeaderValue("f");
                        if (!peer) {
                            delete msg;
                            return -1;
                        }
                        char* tmp = strchr(peer, ':');
                        if (tmp) {
                            peer = tmp + 1; // if the address is like "sip:lyh@nane.cn", then just trim the "sip:"
                        }
                        if (strlen(peer) > gwConsts::MAX_SIP_ADDR_LEN) {
                            delete msg;
                            return -1;
                        }
                        // Get the notification type based the Expire header
                        // If the notification expires, then unsubscribe the service, or if not, the subscribe it
                        const char* subState = ((SipMessage*)msg)->getSipHeaderValue("Subscription-State");
                        if (subState) {
                            if (!strcmp(subState, "terminated")) {
                                // Indicating the subscription is terminated. The header should contain a reason parameter "reason=timeout"
                                // --RFC3265 3.1.6.4
                                subState = "0"; // meaning terminated
                            } else {
                                subState = "1";
                            }
                        } else {
                            subState = "1";
                        }

                        uint32_t contentLen = ((SipMessage*)msg)->getSipContentLength();
                        char* serviceXml = new char[contentLen + 1 + gwConsts::MAX_SIP_ADDR_LEN + 1];
                        strcpy(serviceXml, peer);
                        strcat(serviceXml, "^");
                        strcat(serviceXml, subState);
                        strcat(serviceXml, "^");
                        size_t len = strlen(serviceXml);
                        ((SipMessage*)msg)->getSipContent(&serviceXml[len], contentLen + 1);
                        serviceXml[len + contentLen] = '\0';

                        // Trying to call CloudCommEngine::SubscribeCloudServiceToLocal
                        // Just push the notify content in the FIFO and the CloudCommEngine will pop out
                        // the content and call CloudCommEngine::SubscribeCloudServiceToLocal
                        ims->incomingNotifyQueue.Enqueue(serviceXml);
                    }
                } else { // No content type available, just ignore it
                    //return -1;
                }
                delete msg;
            } else {
#ifndef NDEBUG
                printf("Received a invalid SIP NOTIFY\n");
#endif

                return -1;
            }
        }
        break;

    case nua_event_t::nua_r_subscribe:     // the response to the outgoing subscription
        {
            IMSTransport* ims = IMSTransport::GetInstance();
            int resCode = e->GetStatus();
            if (resCode >= 200 && resCode < 300) {
                ims->condSubscribe.notify_one();
            } else if (resCode >= 400) {
                // error occurs while subscribing. Do something?
                // In the future, the error reason should be retrieved, and if the reason
                // is because of authorization, should re-register the whole client
                // TBD
                switch (resCode) {
                case 401: // authorization problem ?
                    {
                        ims->regCmdQueue.Enqueue(ims->regExpires);
                        ims->imsTransportStatus = gwConsts::IMS_TRANSPORT_STATUS_UNREGISTERED;
                    }
                    break;

                case 403: // forbidden ?
                    break;

                case 404: // user not found or not registered, should re-try
                    {
                        SipMessage* msg = e->GetSipMessage();
                        if (!msg || !msg->isValid()) {
                            return -1;
                        }
                        char* peer = msg->getSipHeaderValue("f");
                        if (peer) {
                            std::map<qcc::String, bool>::iterator itrSub = ims->subscriptions.find((qcc::String)peer);
                            if (itrSub != ims->subscriptions.end()) {
                                itrSub->second = false;
                            } else {
                                ims->subscriptions.insert(std::pair<qcc::String, bool>((qcc::String)peer, false));
                            }
                        }
                        delete msg;
                    }
                    break;

                default:
                    break;
                }
            }
        }
        break;

    case nua_event_t::nua_r_unsubscribe: // the response to the outgoing unsubscription
        {
            IMSTransport* ims = IMSTransport::GetInstance();
            int resCode = e->GetStatus();
            if (resCode >= 200 && resCode < 300) {
                ims->condUnsubscribe.notify_one();
            } else if (resCode >= 400) {
                // error occurs while subscribing. Do something?
                // In the future, the error reason should be retrieved, and if the reason
                // is because of authorization, should re-register the whole client
                // TBD
                switch (resCode) {
                case 401: // authorization problem ?
                    {
                        ims->regCmdQueue.Enqueue(ims->regExpires);
                        ims->imsTransportStatus = gwConsts::IMS_TRANSPORT_STATUS_UNREGISTERED;
                    }
                    break;

                case 403: // forbidden ?
                    break;

                case 404: // user not found or not registered, should re-try
                    {
                        SipMessage* msg = e->GetSipMessage();
                        if (!msg) {
                            return -1;
                        }
                        char* peer = msg->getSipHeaderValue("f");
                        if (peer) {
                            std::map<qcc::String, bool>::iterator itrSub = ims->subscriptions.find((qcc::String)peer);
                            if (itrSub != ims->subscriptions.end()) {
                                itrSub->second = true;
                            } else {
                                ims->subscriptions.insert(std::pair<qcc::String, bool>((qcc::String)peer, true));
                            }
                        }
                        delete msg;
                    }
                    break;

                default:
                    break;
                }
            }
        }
        break;

    default:
    case nua_event_t::nua_i_subscribe:
    case nua_event_t::nua_i_subscription:
        break;
    }
    return 0;
}

int IMSTransportSipCallback::OnPublicationEvent(const PublicationEvent* e)
{
    switch (e->GetType()) {
    case nua_event_t::nua_r_publish:     // the response to the outgoing publication
        {
            // according to RFC3903:
            //    For each successful PUBLISH request, the ESC will generate and assign
            //    an entity-tag and return it in the SIP-ETag header field of the 2xx
            //    response.
            //    When updating previously published event state, PUBLISH requests MUST
            //    contain a single SIP-If-Match header field identifying the specific
            //    event state that the request is refreshing, modifying or removing.
            //    This header field MUST contain a single entity-tag that was returned
            //    by the ESC in the SIP-ETag header field of the response to a previous
            //    publication.
            // so we have to get the SIP-ETag header value from the successful 2xx response,
            // and store it somewhere for later refreshing, deletion or modification
            IMSTransport* ims = IMSTransport::GetInstance();
            int resCode = e->GetStatus();
            if (resCode >= 200 && resCode < 300) {
                ims->condPublish.notify_one();
            } else if (resCode >= 400) {
                // error occurs while publishing service. Do something?
                // In the future, the error reason should be retrieved, and if the reason
                // is because of authorization, should re-register the whole client
                // TBD
                ims->regCmdQueue.Enqueue(ims->regExpires);
                ims->imsTransportStatus = gwConsts::IMS_TRANSPORT_STATUS_UNREGISTERED;
            }
        }
        break;

    case nua_event_t::nua_r_unpublish:     // the response to the outgoing unpublication
        {
            IMSTransport* ims = IMSTransport::GetInstance();
            int resCode = e->GetStatus();
            if (resCode >= 200 && resCode < 300) {
                ims->condUnpublish.notify_one();
            } else if (resCode >= 400 && resCode < 600) {
                // error occures while publishing service. Do something?
                // In the future, the error reason should be retrieved, and if the reason
                // is because of authorization, should re-register the whole client
                // TBD
                ims->regCmdQueue.Enqueue(ims->regExpires);
                ims->imsTransportStatus = gwConsts::IMS_TRANSPORT_STATUS_UNREGISTERED;
            }
        }
        break;

    case nua_event_t::nua_i_publish:
    default:
        break;
    }
    return 0;
}

} // namespace gateway
} // namespace sipe2e
