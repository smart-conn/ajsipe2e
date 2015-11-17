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
#ifndef IMSTRANSPORT_H_
#define IMSTRANSPORT_H_

#include <Common/GatewayExport.h>

#include <Common/Status.h>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>

#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc.

#include "Common/SyncQueue.h"

#include <qcc/String.h>


#include <map>

class SipStack;
class RegistrationSession;
class OptionsSession;
class MessagingSession;
class SubscriptionSession;
class PublicationSession;

class boost::thread;

class SimpleTimer;

namespace sipe2e {
namespace gateway {

namespace gwConsts {
    enum IMS_TRANSPORT_STATUS_ENUM;
}

class IMSTransportSipCallback;

struct SyncContext {
    boost::mutex mtx;
    boost::condition_variable con;
    boost::shared_array<char> content; 
};

/**
 * This class is for providing a session & transport layer functionality for CloudCommEngine.
 * IMS/SIP sessions will be maintained and managed in this module. Upper layers will be utilizing
 * this module to send/receive interaction messages through IMS network.
 */
class SIPE2E_GATEWAY_EXTERN IMSTransport
{
    friend class IMSTransportSipCallback;
public:
    IMSTransport();
    virtual ~IMSTransport();

    static IMSTransport* GetInstance();
    static IStatus DeleteInstance();
public:
    /**
     * Initialize the whole environment, including the stack and all required information to run the stack
     * @param -    
     */
    IStatus Init();

    gwConsts::IMS_TRANSPORT_STATUS_ENUM GetStatus();

    /**
     * 
     * @param remoteAccount - the gateway account address of the remote service
     */
    IStatus Subscribe(const char* remoteAccount);

    /**
     * 
     * @param - 
     */
    IStatus Unsubscribe(const char* remoteAccount);

    /**
     * Publish local services to the IMS network (presence server)
     * @param -    
     */
    IStatus PublishService(const char* introspectionXml);

    /**
     * Delete local services from the IMS network (presence server)
     * @param -    
     */
    IStatus DeleteService(const char* introspectionXml);

    /**
     * Read the incoming NOTIFY message from incomingNotifyQueue, may be blocked
     */
    boost::shared_array<char> ReadServiceNotification();

    void StopReadServiceNotification();

    /**
     *
     * @param msgBuf - 
     */
    IStatus ReadCloudMessage(char** msgBuf);

    /**
     *
     * @param isRequest - 1 for request, 0 for response
     * @param peer - 
     * @param callId - if it's request and callId is NULL, then generate the CallID here
     * @param msgBuf - 
     * @param resMsgBuf - 
     */
    IStatus SendCloudMessage(bool isRequest,
        const char* peer,
        const char* callId,
        const char* addr,
        const char* msgBuf,
        char** resMsgBuf);

private:
    /**
     * The thread function for registration task
     * @param - 
     */
    static void RegThreadFunc();

    /**
     * Subscription routine, periodically checking the map 'subscriptions'.
     * If there is some account that is not subscribed, then just send subscription
     * @param - parameters for subscription routine
     */
    static void SubFunc(void* para);

    /**
     * Heartbeat routine, generally sending OPTIONS periodically
     * @param para - parameters for heartbeat routine
     */
    static void HeartBeatFunc(void* para);

    /**
     * TBD
     * @param - 
     */
    IStatus doSubscribe(const char* remoteAccount);

private:
    /* The stack that supports the whole IMS communications */
    SipStack* stack;
    /* Callback for receiving incoming messages */
    IMSTransportSipCallback* sipCB;

    /* The status of the IMS Transport Layer */
    gwConsts::IMS_TRANSPORT_STATUS_ENUM imsTransportStatus; 

    /* Realm */
    qcc::String realm;
    /* Private identity */
    qcc::String impi;
    /* Public identity */
    qcc::String impu;
    /* Password */
    qcc::String password;
    /* pCSCF */
    qcc::String pcscf;
    /* pCSCF port */
    unsigned short pcscfPort;
    qcc::String pcscfTransport, pcscfIpversion;

    /* */
    RegistrationSession* regSession;
    /* */
    OptionsSession* opSession;
    /* */
    MessagingSession* msgSession;
    /* */
    std::map<std::string, SubscriptionSession*> subSessions;

    /* All subscribed account information, if the subscription is successful, the value is true */
    std::map<std::string, bool> subscriptions;

    /* The condition for waiting for the response of subscribing/unsubscribing */
    boost::mutex mtxSubscribe, mtxUnsubscribe;
    boost::condition_variable condSubscribe, condUnsubscribe;

    /* */
    std::map<std::string, PublicationSession*> pubSessions;

    /* The condition for waiting for the response of publishing/unpublishing */
    boost::mutex mtxPublish, mtxUnpublish;
    boost::condition_variable condPublish, condUnpublish;

    /**
     * The thread that will be started on initialization for registration task. 
     * Once any one of the following conditions is met, then task initiates
     * a new registration:
     *   > the registration is expired (the timer expires)
     *   > the heartbeat (OPTIONS) is not responded correctly (timed out or with 401 Unauthorized)
     *   > some normal service, like MESSAGE, is responded with 401 Unauthorized
     */
    boost::thread* regThread;

    /**
     * This is the queue to pass the command to the registration thread abovementioned.
     * The integer passed in means the registration expiration seconds.
     */
    SyncQueue<unsigned int> regCmdQueue;

    /**
     * TBD
     */
    SimpleTimer* timerSub;

    /* */
    SimpleTimer* timerHeartBeat;

    /* The condition for waiting for the response of HeartBeat (OPTIONS) */
    boost::mutex mtxHeartBeat;
    boost::condition_variable condHeartBeat;

    /* The condition for waiting for the response of unregister command */
    boost::mutex mtxUnregister;
    boost::condition_variable condUnregister;

    /* */
    unsigned int regExpires; // in seconds

    /** 
     * After sending out the request message, the sender thread will wait for the response.
     * When a response message arrives, the receiver (Callback) will have to dispatch it
     * to the waiting thread according to the CallID of the request-response pair.
     * The following map stores the information of CallID and its corresponding waiting thread
     */
    std::map<std::string, boost::shared_ptr<SyncContext>> responseDispatchTable;
    /* Mutex for protecting responseDispatchTable */
    boost::mutex mtxResponseDispatchTable;

    /** 
     * The incoming messages queue (FIFO). There are one producer (IMS receiver) and
     * multiple consumers (multi threads in the axis2_ims_receiver.dll)
     */
    SyncQueue<boost::shared_array<char>> incomingMsgQueue;

    /**
     * The incoming queue (FIFO) storing service subscription notifications.
     * CloudCommEngine will generate a thread to read the notifications by
     * calling 
     */
    SyncQueue<boost::shared_array<char>> incomingNotifyQueue;

    /* uuid generator for generating CallId */
    boost::uuids::random_generator callIdGenerator;
};

}
}

#endif // IMSTRANSPORT_H_


