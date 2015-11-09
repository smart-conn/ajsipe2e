/******************************************************************************
 * Copyright (c) 2014-2015, Beijing HengShengDongYang Technology Ltd. All rights reserved.
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
#if defined(QCC_OS_GROUP_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#endif
#include <qcc/platform.h>
#include "CloudCommEngine/IMSTransport/IMSTransportSipCallback.h"
#include "CloudCommEngine/IMSTransport/ContentType.h"
#include "CloudCommEngine/IMSTransport/IMSTransportConstants.h"
#include "CloudCommEngine/IMSTransport/IMSTransport.h"

#include <SipStack.h>
#include <SipEvent.h>
#include <SipMessage.h>
#include <SipSession.h>


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
/*
			IMSTransport* ims = IMSTransport::GetInstance();
			for (int i = 0; i < 5; i++) {
				if (ims->stack->/ *deInitialize* /stop()) {
					break;
				}
			}
			for (int i = 0; i < 5; i++) {
				if (ims->stack->/ *initialize* /start()) {
					break;
				}
			}
*/
		}
		break;
	default:
		break;
	}
	return 0;
}

int IMSTransportSipCallback::OnRegistrationEvent(const RegistrationEvent* e)
{
	switch (e->getType())
	{
	default:
	case tsip_i_newreg:
	case tsip_i_register:
		break;
	case tsip_ao_register: // the response to the outgoing registration request
		{
			SipMessage* msg = (SipMessage*)e->getSipMessage();
			short resCode = msg->getResponseCode();
			if (resCode >= 300) {
				// error occurs while trying to registering the UAC
				// retry to register immediately
				IMSTransport* ims = IMSTransport::GetInstance();
				ims->regCmdQueue.Enqueue(ims->regExpires);
			}
		}
		break;
	case tsip_ao_unregister: // the response to the outgoing unregistration request
		{
			SipMessage* msg = (SipMessage*)e->getSipMessage();
			short resCode = msg->getResponseCode();
			if (resCode < 300) {// unregister successful
				IMSTransport* ims = IMSTransport::GetInstance();
				ims->condUnregister.notify_one();
			} else { // unregister failed
			}
		}
		break;
	case tsip_i_unregister: // network initiated unregister ?
		{
			// to be continued
		}
		break;
	}
	return 0;
}

int IMSTransportSipCallback::OnOptionsEvent(const OptionsEvent* e)
{
	switch (e->getType())
	{
	case tsip_i_options:
		{
			OptionsSession* opSessionI = (OptionsSession*)e->getSession();
			if (opSessionI == NULL)
			{
				if ((opSessionI = e->takeSessionOwnership()) != NULL)
				{
					ActionConfig* config = new ActionConfig();
					config->addHeader("Allow", "INVITE, ACK, CANCEL, BYE, MESSAGE, OPTIONS, NOTIFY, PRACK, UPDATE, REFER");
					opSessionI->accept(config);
					delete opSessionI;
					delete config;
				}
			}
		}
		break;
	case tsip_ao_options: // the response to the outgoing OPTIONS
		{
			IMSTransport* ims = IMSTransport::GetInstance();
			const SipMessage* msg = e->getSipMessage();
			short resCode = ((SipMessage*)msg)->getResponseCode();
			if (resCode < 300) {
// 				boost::lock_guard<boost::mutex> lock(ims->mtxHeartBeat);
				ims->condHeartBeat.notify_one();
			} else {
				// error occurs during HeartBeat, then re-register the UAC
				ims->regCmdQueue.Enqueue(ims->regExpires);
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
	switch (e->getType())
	{
	case tsip_i_message:
		{
			IMSTransport* ims = IMSTransport::GetInstance();
			const SipMessage* msg = e->getSipMessage();
			MessagingSession* session = (MessagingSession*)e->getSession();
			if (!session) {
				session = e->takeSessionOwnership();
			}
			if (msg) {
				session->accept();
				char* contentType = ((SipMessage*)msg)->getSipHeaderValue("c");
				if (contentType) {
					if (!strcmp(contentType, gwConsts::contenttype::ALLJOYN_XML)) {
						// RPC message
						// First, we should check it is a incoming request or a response for previous
						// outgoing request. If it is a request, then we should get its From&CallID headers
						// and make the content buffer like the format "lyh@nane.cn^CallID^Addr^request content"
						// and hand it over to Axis' IMS Receiver (axis_ims_receiver.dll). Here 'Addr' means
						// the local called address, if it's AllJoyn, like 'BusName/ObjectPath/InterfaceName'
						// If it is a response, then we should also get its From&CallID headers and try to 
						// find the outgoing request worker thread corresponding to From&CallID information
						// like "lyh@nane.cn^CallID", and if found, invoke the outgoing worker thread and process
						// the response
						char* msgType = ((SipMessage*)msg)->getSipHeaderValue(gwConsts::customheader::RPC_MSG_TYPE);
						if (!msgType) {
							return -1;
						}
						char* peer = ((SipMessage*)msg)->getSipHeaderValue("f");
						if (!peer) {
							return -1;
						}
						char* tmp = strchr(peer, ':');
						if (tmp) {
							peer = tmp + 1; // if the address is like "sip:lyh@nane.cn", then just trim the "sip:"
						}
						if (strlen(peer) > gwConsts::MAX_SIP_ADDR_LEN) {
							return -1;
						}
						char *callId = ((SipMessage*)msg)->getSipHeaderValue(gwConsts::customheader::RPC_CALL_ID);
						if (!callId || strlen(callId) > gwConsts::MAX_RPC_MSG_CALLID_LEN) {
							return -1;
						}
						unsigned int contentLen = ((SipMessage*)msg)->getSipContentLength();
						switch (atoi(msgType)) {
						default:
						case gwConsts::customheader::RPC_MSG_TYPE_REQ:
							{
								// if it is an incoming request message
								// Fill the request buffer as the format "lyh@nane.cn^CallID^Addr^Request Content"
								char *addr = ((SipMessage*)msg)->getSipHeaderValue(gwConsts::customheader::RPC_ADDR);
								if (!addr) {
									return -1;
								}
								boost::shared_array<char> rpcXml(new char[contentLen + 1 + gwConsts::MAX_SIP_ADDR_LEN + gwConsts::MAX_RPC_MSG_CALLID_LEN + 2]);
								strcpy(rpcXml.get(), peer);
								strcat(rpcXml.get(), "^");
								strcat(rpcXml.get(), callId);
								strcat(rpcXml.get(), "^");
								strcat(rpcXml.get(), addr);
								strcat(rpcXml.get(), "^");
								size_t len = strlen(rpcXml.get());
								((SipMessage*)msg)->getSipContent(&rpcXml[len], contentLen + 1);
								rpcXml[len + contentLen] = '\0';

								// Put the request in the incoming request buffer
								ims->incomingMsgQueue.Enqueue(rpcXml);
							}
							break;
						case gwConsts::customheader::RPC_MSG_TYPE_RES:
							{
								// if it is a response message
								std::string reqKey(peer);
								reqKey += "^";
								reqKey += callId;
								{
									boost::lock_guard<boost::mutex> lock(ims->mtxResponseDispatchTable);
									std::map<std::string, boost::shared_ptr<SyncContext>>::iterator itr =
										ims->responseDispatchTable.find(reqKey);
									if (itr != ims->responseDispatchTable.end()) {
										boost::shared_ptr<SyncContext> syncCtx = itr->second;
										{
											boost::lock_guard<boost::mutex> lock(syncCtx->mtx);
											// Prepare the response buffer and pass it to the sender worker thread
                                            if (contentLen > 0) {
                                                boost::shared_array<char> rpcXml(new char[contentLen + 1]);
                                                ((SipMessage*)msg)->getSipContent(rpcXml.get(), contentLen + 1);
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
			} else {
				session->reject();
				return -1;
			}
		}
		break;
	case tsip_ao_message: // the response to the outgoing messages
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
	switch (e->getType())
	{
	case tsip_i_notify:
		{
			IMSTransport* ims = IMSTransport::GetInstance();
			const SipMessage* msg = e->getSipMessage();
			if (msg) {
				char* contentType = ((SipMessage*)msg)->getSipHeaderValue("c");
				if (contentType) {
					if (!strcmp(contentType, gwConsts::contenttype::PIDF)) {
						// basic presence information notification

						// First try to find the subscription information corresponding to
						// the notification. How?

						// Secondly try to get the content and subscribe the service described
						// in the content to local by calling ProximalCommEngine::AJSubscribeCloudServiceToLocal

						// Get the peer address
						char* peer = ((SipMessage*)msg)->getSipHeaderValue("f");
						if (!peer) {
							return -1;
						}
						char* tmp = strchr(peer, ':');
						if (tmp) {
							peer = tmp + 1; // if the address is like "sip:lyh@nane.cn", then just trim the "sip:"
						}
						if (strlen(peer) > gwConsts::MAX_SIP_ADDR_LEN) {
							return -1;
						}
						// Get the notification type based the Expire header
						// If the notification expires, then unsubscribe the service, or if not, the subscribe it
						char* subState = ((SipMessage*)msg)->getSipHeaderValue("Subscription-State");
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

						unsigned int contentLen = ((SipMessage*)msg)->getSipContentLength();
						boost::shared_array<char> serviceXml(new char[contentLen + 1 + gwConsts::MAX_SIP_ADDR_LEN + 1]);
						strcpy(serviceXml.get(), peer);
						strcat(serviceXml.get(), "^");
						strcat(serviceXml.get(), subState);
						strcat(serviceXml.get(), "^");
						size_t len = strlen(serviceXml.get());
						((SipMessage*)msg)->getSipContent(&serviceXml[len], contentLen + 1);
						serviceXml[len + contentLen] = '\0';

						// Trying to call ProximalCommEngine::AJSubscribeCloudServiceToLocal
						// Just push the notify content in the FIFO and the CloudCommEngine will pop out
						// the content and call ProximalCommEngine::AJSubscribeCloudServiceToLocal
						ims->incomingNotifyQueue.Enqueue(serviceXml);
					}
				} else { // No content type available, just ignore it
					//return -1;
				}
			}
			else {
				return -1;
			}
		}
		break;
	case tsip_ao_subscribe: // the response to the outgoing subscription
		{
			IMSTransport* ims = IMSTransport::GetInstance();
			const SipMessage* msg = e->getSipMessage();
			short resCode = ((SipMessage*)msg)->getResponseCode();
			if (resCode < 300) {
				ims->condSubscribe.notify_one();
			} else {
				// error occurs while subscribing. Do something?
			}
		}
		break;
	case tsip_ao_unsubscribe: // the response to the outgoing unsubscription
		{
			IMSTransport* ims = IMSTransport::GetInstance();
			const SipMessage* msg = e->getSipMessage();
			short resCode = ((SipMessage*)msg)->getResponseCode();
			if (resCode < 300) {
				ims->condUnsubscribe.notify_one();
			} else {
				// error occurs while subscribing. Do something?
			}
		}
		break;
	default:
	case tsip_i_subscribe:
	case tsip_i_unsubscribe:
	case tsip_ao_notify:
		break;
	}
	return 0;
}

int IMSTransportSipCallback::OnPublicationEvent(const PublicationEvent* e)
{
	switch (e->getType())
	{
	case tsip_ao_publish: // the response to the outgoing publication
		{
			// according to RFC3903:
			//	For each successful PUBLISH request, the ESC will generate and assign
			//	an entity-tag and return it in the SIP-ETag header field of the 2xx
			//	response.
			//	When updating previously published event state, PUBLISH requests MUST
			//	contain a single SIP-If-Match header field identifying the specific
			//	event state that the request is refreshing, modifying or removing.
			//	This header field MUST contain a single entity-tag that was returned
			//	by the ESC in the SIP-ETag header field of the response to a previous
			//	publication.
			// so we have to get the SIP-ETag header value from the successful 2xx response,
			// and store it somewhere for later refreshing, deletion or modification
			IMSTransport* ims = IMSTransport::GetInstance();
			const SipMessage* msg = e->getSipMessage();
			short resCode = ((SipMessage*)msg)->getResponseCode();
			if (resCode < 300) {
				ims->condPublish.notify_one();
			} else {
				// error occurs while publishing service. Do something?
			}
		}
		break;
	case tsip_ao_unpublish: // the response to the outgoing unpublication
		{
			IMSTransport* ims = IMSTransport::GetInstance();
			const SipMessage* msg = e->getSipMessage();
			short resCode = ((SipMessage*)msg)->getResponseCode();
			if (resCode < 300) {
				ims->condUnpublish.notify_one();
			} else {
				// error occures while publishing service. Do something?
			}
		}
		break;
	case tsip_i_publish:
	case tsip_i_unpublish:
	default:
		break;
	}
	return 0;
}

} // namespace gateway
} // namespace sipe2e