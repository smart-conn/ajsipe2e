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
#include <qcc/platform.h>
#include "CloudCommEngine/IMSTransport/IMSTransport.h"
#include "CloudCommEngine/IMSTransport/IMSTransportSipCallback.h"
#include "CloudCommEngine/IMSTransport/IMSTransportConstants.h"
#include "CloudCommEngine/IMSTransport/pidf.h"

#include "Common/SimpleTimer.h"
#include "Common/GuidUtil.h"
#include "Common/sha256.h"

#include <qcc/XmlElement.h>
#include <qcc/StringSource.h>
#include <qcc/StringUtil.h>
#include <alljoyn/Init.h>

#include <mutex>

#include <SipStack.h>
#include <SipSession.h>

#if defined( _MSC_VER )
#include <direct.h>        // _getcwd
// #include <crtdbg.h>
#include <windows.h>
#elif defined(MINGW32) || defined(__MINGW32__)
#include <io.h>  // mkdir
#else
#include <sys/stat.h>    // mkdir
#endif


using namespace qcc;

namespace sipe2e {
namespace gateway {

static IMSTransport* imsIns = new IMSTransport();


IMSTransport::IMSTransport()
    : stack(NULL), sipCB(NULL), imsTransportStatus(gwConsts::IMS_TRANSPORT_STATUS_UNREGISTERED),
    realm(gwConsts::DEFAULT_REALM), pcscfPort(gwConsts::DEFAULT_PCSCF_PORT),
    regSession(NULL), opSession(NULL), msgSession(NULL),
    regThread(NULL), timerHeartBeat(new Timer(String("HeartBeat"))), timerSub(new Timer(String("Subscribe"))), 
    regExpires(gwConsts::REGISTRATION_DEFAULT_EXPIRES)
{
}


IMSTransport::~IMSTransport()
{
    if (regThread) {
        regCmdQueue.StopQueue();
        regThread->join();
//         regThread->try_join_for(boost::chrono::milliseconds(1000)); /// wait for the thread to end
    }
    timerHeartBeat->Stop();
    timerHeartBeat->Join();
    delete timerHeartBeat;
    timerHeartBeat = NULL;

    timerSub->Stop();
    timerSub->Join();
    delete timerSub;
    timerSub = NULL;

    incomingNotifyQueue.StopQueue();
    incomingMsgQueue.StopQueue();

    if (opSession) {
        delete opSession;
        opSession = NULL;
    }
    if (msgSession) {
        delete msgSession;
        msgSession = NULL;
    }
    std::map<qcc::String, SubscriptionSession*>::iterator itrSubsession = subSessions.begin();
    while (itrSubsession != subSessions.end()) {
        SubscriptionSession* subSession = itrSubsession->second;
        if (subSession) {
            // do not need to unsubscribe to the remote account, since the service may be still on
            // but the subscriber may be (temporarily) down.
            subSession->unSubscribe();
            delete subSession;
            subSession = NULL;
        }
        itrSubsession++;
    }
    std::map<qcc::String, PublicationInfo>::iterator itrPubsession = publications.begin();
    while (itrPubsession != publications.end()) {
        PublicationSession* pubSession = itrPubsession->second.pubSession;
        if (pubSession) {
            pubSession->unPublish();
            {
                std::unique_lock<std::mutex> lock(mtxUnpublish);
                if (condUnpublish.wait_for(lock, std::chrono::milliseconds(gwConsts::PUBLICATION_DEFAULT_TIMEOUT))) {
                    break; // unregister successful
                }
            }
            delete pubSession;
            pubSession = NULL;
        }
        itrPubsession++;
    }
    publications.clear();
    if (regSession) {
        for (int i = 0; i < 5; i++) { // try to unregister for 5 times with 5 seconds timeout every time
            regSession->unRegister();
            {
                std::unique_lock<std::mutex> lock(mtxUnregister);
                if (condUnregister.wait_for(lock, std::chrono::milliseconds(gwConsts::REGISTRATION_DEFAULT_TIMEOUT))) {
                    break; // unregister successful
                }
            }
        }
        delete regSession;
        regSession = NULL;
        imsTransportStatus = gwConsts::IMS_TRANSPORT_STATUS_UNREGISTERED;
    }
    if (stack) {
        stack->stop();
        stack->deInitialize();
        delete stack;
        stack = NULL;
    }
    if (sipCB) {
        delete sipCB;
        sipCB = NULL;
    }
    AllJoynShutdown();
}


gwConsts::IMS_TRANSPORT_STATUS_ENUM IMSTransport::GetStatus()
{
    return imsTransportStatus;
}


IMSTransport* IMSTransport::GetInstance()
{
    if (!imsIns) {
        imsIns = new IMSTransport();
    }
    return imsIns;
}

IStatus IMSTransport::DeleteInstance()
{
    if (imsIns) {
        delete imsIns;
        imsIns = NULL;
    }
    return IC_OK;
}

IStatus IMSTransport::Init()
{
    // First get the configuration xml file, and read IMS configurations,
    // including IMPI, IMPU, password, pCSCF settings

    /**
     *  <Transport>
     *    <IMS>
     *      <Realm>nane.cn</Realm>
     *      <IMPI>sip:lyh@nane.cn</IMPI>
     *      <IMPU>lyh@nane.cn</IMPU>
     *      <Password>test123</Password>
     *      <pCSCF domain="pcscf.nane.cn" port="5060" transport="tcp" ipversion="ipv4" />
     *    </IMS>
     *  </Transport>
     */
    QStatus status = ER_OK;
    status = AllJoynInit();
    if (ER_OK != status) {
        return IC_TRANSPORT_FAIL;
    }
    char confFile[MAX_PATH];
    confFile[0] = '\0';
    char* tmpRet = NULL;
#ifdef _WIN32
    tmpRet = _getcwd(confFile, MAX_PATH);
    strcat(confFile, "\\conf.xml");
#else // linux
    tmpRet = getcwd(confFile, MAX_PATH);
    strcat(confFile, "/conf.xml");
#endif
    if (NULL == tmpRet) {// indicating an error, like the max length exceeded
        return IC_SYSTEM;
    }

    String confFileContent;
    FILE* confFileHandle = fopen(confFile, "r");
    if (!confFileHandle) {
        return IC_CONF_FILE_NOT_FOUND;
    }
    char tmpBuf[128];
    while (fgets(tmpBuf, 128, confFileHandle)) {
        confFileContent += (const char*)tmpBuf;
    }
    StringSource source(confFileContent);
    XmlParseContext pc(source);
    status = XmlElement::Parse(pc);
    if (ER_OK != status) {
        return IC_CONF_XML_PARSING;
    }
    const XmlElement* rootNode = pc.GetRoot();
    if (rootNode == NULL) {
        return IC_CONF_XML_PARSING;
    }
    const XmlElement* transportNode = rootNode->GetChild((String)"Transport");
    if (!transportNode) {
        return IC_CONF_XML_PARSING;
    }
    const XmlElement* imsNode = transportNode->GetChild((String)"IMS");
    if (!imsNode) {
        return IC_CONF_XML_PARSING;
    }
    const std::vector<XmlElement*>& imsChildrenNodes = imsNode->GetChildren();
    for (size_t imsChildIndx = 0; imsChildIndx < imsChildrenNodes.size(); imsChildIndx++) {
        const XmlElement* imsChildNode = imsChildrenNodes[imsChildIndx];
        if (imsChildNode) {
            const String& imsChildName = imsChildNode->GetName();
            if (imsChildName == "Realm") {
                realm = imsChildNode->GetContent();
            } else if (imsChildName == "IMPI") {
                impi = imsChildNode->GetContent();
            } else if (imsChildName == "IMPU") {
                impu = imsChildNode->GetContent();
            } else if (imsChildName == "Password") {
                password = imsChildNode->GetContent();
            } else if (imsChildName == "pCSCF") {
                pcscf = imsChildNode->GetAttribute("domain");
                const String& portStr = imsChildNode->GetAttribute("port");
                pcscfPort = StringToU32(portStr);
                pcscfTransport = imsChildNode->GetAttribute("transport");
                pcscfIpversion = imsChildNode->GetAttribute("ipversion");
            } else if (imsChildName == "Expires") {
                const String& expiresStr = imsChildNode->GetContent();
                regExpires = StringToU32(expiresStr);
            }
        }
    }
    
    // subscriptions
    const XmlElement* subscriptionsNode = rootNode->GetChild((String)"Subscriptions");
    if (subscriptionsNode) {
        std::vector<const XmlElement*>& subscriptionNodes = subscriptionsNode->GetChildren((String)"Subscription");
        for (size_t subIndx = 0; subIndx < subscriptionNodes.size(); subIndx++) {
            const XmlElement* subscriptionNode = subscriptionNodes[subIndx];
            if (subscriptionNode) {
                const String& subAccount = subscriptionNode->GetContent();
                subscriptions.insert(std::make_pair(subAccount, false));
            }
        }
    }

    sipCB = new IMSTransportSipCallback();
    stack = new SipStack(sipCB, realm.c_str(), impi.c_str(), impu.c_str());
    if (password.size() > 0) {
        stack->setPassword(password.c_str());
    }
    stack->setProxyCSCF(pcscf.c_str(), pcscfPort, pcscfTransport.c_str(), pcscfIpversion.c_str());

    stack->addHeader("Allow", "INVITE, ACK, CANCEL, BYE, MESSAGE, OPTIONS, NOTIFY, PRACK, UPDATE, REFER");
    stack->addHeader("User-Agent", "SIPE2E-Gateway/v1.0.0");

    if (!stack->initialize()) {
        return IC_TRANSPORT_IMS_STACK_INIT_FAIL;
    }
    if (!stack->start()) {
        return IC_TRANSPORT_IMS_STACK_START_FAIL;
    }

    regSession = new RegistrationSession(stack);
    regSession->addCaps("+g.oma.sip-im");
    regSession->addCaps("+g.3gpp.smsip");
    regSession->addCaps("language", "\"zh,en\"");
//     regSession->setExpires(expires);

//     opSession = new OptionsSession(stack);

    msgSession = new MessagingSession(stack);
    
    /**
     * Here we start a new thread for registration task. 
     * Once any one of the following conditions is met, then task initiates
     * a new registration:
     *   > the registration is expired (the timer expires)
     *   > the heartbeat (OPTIONS) is not responded correctly (timed out or with 401 Unauthorized)
     *   > some normal service, like MESSAGE, is responded with 401 Unauthorized
     */
    regCmdQueue.Enqueue(regExpires); // registration command
    regThread = new std::thread(IMSTransport::RegThreadFunc);

    /**
     * TBD
     */
    timerSub->Start();
    SubTimerAlarmListener* subTimerAlarmer = new SubTimerAlarmListener();
    uint32_t alarmTime = /*gwConsts::SIPSTACK_HEARTBEAT_INTERVAL*/100;
    Alarm subAlarm(alarmTime, subTimerAlarmer, this, gwConsts::SIPSTACK_HEARTBEAT_INTERVAL);
    timerSub->AddAlarm(subAlarm);

    /**
     * Start a timer for heartbeat OPTIONS
     */
    timerHeartBeat->Start();
    HeartBeatTimerAlarmListener* heartBeatTimerAlarmer = new HeartBeatTimerAlarmListener();
    alarmTime = /*gwConsts::SIPSTACK_HEARTBEAT_INTERVAL*/100;
    Alarm heartBeatAlarm(alarmTime, heartBeatTimerAlarmer, this, gwConsts::SIPSTACK_HEARTBEAT_INTERVAL);
    timerHeartBeat->AddAlarm(heartBeatAlarm);

    return IC_OK;
}

IStatus IMSTransport::Subscribe(const char* remoteAccount)
{
    if (!remoteAccount) {
        return IC_BAD_ARG_1;
    }
    std::map<qcc::String, bool>::iterator itrSub = subscriptions.find((qcc::String)remoteAccount);
    if (itrSub != subscriptions.end()) {
        itrSub->second = false;
    } else {
        subscriptions.insert(std::pair<qcc::String, bool>((qcc::String)remoteAccount, false));
    }
    return IC_OK;
}

IStatus IMSTransport::doSubscribe(const char* remoteAccount)
{
    if (!remoteAccount) {
        return IC_BAD_ARG_1;
    }
    SubscriptionSession* subSession = NULL;
    bool isNewCreated = false;
    std::map<qcc::String, SubscriptionSession*>::iterator itrSubsession = subSessions.find((qcc::String)remoteAccount);
    if (itrSubsession != subSessions.end()) {
        subSession = itrSubsession->second;
    }
    if (!subSession) {
        subSession = new SubscriptionSession(stack);
        subSession->setToUri(remoteAccount);
        subSession->addHeader("Event", "presence");
        isNewCreated = true;
        subscriptions.insert(std::make_pair((qcc::String)remoteAccount, false));
    }
    if (subSession->subscribe()) {
        // wait for the response of the SUBSCRIBE
        std::unique_lock<std::mutex> lock(imsIns->mtxSubscribe);

        // Note: here we set a timer to wait for the response. This mechanism is not correct in case of
        // multi-thread environment where multi users call Subscribe at the same time.
        // Should be changed using the session Call-ID to differentiate multiple subscribe sessions
        // TBD
        if (condSubscribe.wait_for(lock, std::chrono::milliseconds(gwConsts::SUBSCRIPTION_DEFAULT_TIMEOUT))) {
            // if subscribing is correctly responded
            if (isNewCreated) {
                subSessions.insert(std::make_pair((qcc::String)remoteAccount, subSession));
            }
            subscriptions[(qcc::String)remoteAccount] = true;
//             subscriptions.erase((qcc::String)remoteAccount);
//             subscriptions.insert(std::make_pair((qcc::String)remoteAccount, true));
        } else {
            // if subscribing is not correctly responded or timed out (no response)
            if (isNewCreated) {
                delete subSession;
            }
            return IC_TRANSPORT_IMS_SUB_FAILED;
        }
    } else {
        if (isNewCreated) {
            delete subSession;
        }
        return IC_TRANSPORT_IMS_SUB_FAILED;
    }
    return IC_OK;
}

IStatus IMSTransport::Unsubscribe(const char* remoteAccount)
{
    if (!remoteAccount) {
        return IC_BAD_ARG_1;
    }

    SubscriptionSession* subSession = NULL;
    std::map<qcc::String, SubscriptionSession*>::iterator itrSubsession = subSessions.find((qcc::String)remoteAccount);
    if (itrSubsession != subSessions.end()) {
        SubscriptionSession* subSession = itrSubsession->second;
    } else {
        // If a subsession is not present, should construct a new subsession
        subSession = new SubscriptionSession(stack);
        subSession->setToUri(remoteAccount);
        subSession->addHeader("Event", "presence");
        subSessions.insert(std::pair<qcc::String, SubscriptionSession*>((qcc::String)remoteAccount, subSession));
    }
    if (subSession) {
        if (subSession->unSubscribe()) {
            std::unique_lock<std::mutex> lock(imsIns->mtxUnsubscribe);

            // Note: here we set a timer to wait for the response. This mechanism is not correct in case of
            // multi-thread environment where multi users call Subscribe at the same time.
            // Should be changed using the session Call-ID to differentiate multiple subscribe sessions
            // TBD
            if (condUnsubscribe.wait_for(lock, std::chrono::milliseconds(gwConsts::SUBSCRIPTION_DEFAULT_TIMEOUT))) {
                delete subSession;
                subSessions.erase(itrSubsession);
            } else {
                return IC_TRANSPORT_IMS_SUB_FAILED;
            }
        } else {
            // Unsubscribe is not finished correctly
            return IC_TRANSPORT_IMS_SUB_FAILED;
        }
    } else {
        return IC_TRANSPORT_IMS_SUB_FAILED;
    }
    return IC_OK;
}

IStatus IMSTransport::PublishService(const char* introspectionXml)
{
    // 1. Construct the PUBLISH content and send it out
    // 2. Save the publishing content as value with the service path as the key
    //    later if the publish is responded with 2xx, then extract the SIP-ETag
    //    header value and save it corresponding to the publishing service path
    //    if the publish is responded with errors or timeout, then delete it

    // Note that due to many reasons like network problems or registration problems
    // this publishing action may be failed, in case of which, we should take some
    // actions to re-try or cache the publishing request to re-try later

    // First, get the service path from introspectionXml string
    if (!introspectionXml) {
        return IC_BAD_ARG_1;
    }
    StringSource source(introspectionXml);
    XmlParseContext pc(source);
    if (ER_OK != XmlElement::Parse(pc)) {
        return IC_INTROSPECTION_XML_PARSING;
    }
    const XmlElement* rootNode = pc.GetRoot();
    if (rootNode == NULL) {
        return IC_INTROSPECTION_XML_PARSING;
    }
//     const String& busName = rootNode->GetAttribute("name");
    const XmlElement* aboutEle = rootNode->GetChild((String)"about");
    if (!aboutEle) {
        return IC_INTROSPECTION_XML_PARSING;
    }
    const std::vector<XmlElement*>& aboutDatas = aboutEle->GetChildren();
    if (aboutDatas.empty()) {
        return IC_INTROSPECTION_XML_PARSING;
    }
    String appId, deviceId;
    bool appIdFound = false, deviceIdFound = false;
    for (size_t aboutDataIndx = 0; aboutDataIndx < aboutDatas.size(); aboutDataIndx++) {
        const XmlElement* aboutData = aboutDatas[aboutDataIndx];
        if (aboutData) {
            const String& key = aboutData->GetAttribute("key");
            if (key == "AppId") {
                appId = aboutData->GetAttribute("value");
                appIdFound = true;
            } else if (key == "DeviceId") {
                deviceId = aboutData->GetAttribute("value");
                deviceIdFound = true;
            }
        }
        if (appIdFound && deviceIdFound) {
            break;
        }
    }

    
    String serviceId = appId + "@" + deviceId + "@";
    serviceId += sha256(introspectionXml).c_str();

    PublicationSession* pubSession = NULL;
    bool isNewCreated = false;
    std::map<qcc::String, PublicationInfo>::iterator itrPubsession = publications.find(serviceId);
    if (itrPubsession != publications.end()) {
        pubSession = itrPubsession->second.pubSession;
        if (itrPubsession->second.introspectionXml == introspectionXml) {
            // publish the exactly same service, just ignore it
            return IC_OK;
        }
    }

    ims::service _service;
    _service.SetIntrospectionXml((String)introspectionXml);
    ims::status _status;
    _status.SetBasicStatus(ims::basic::open);
    ims::tuple _tuple;
    _tuple.SetId(serviceId);
    _tuple.SetStatus(_status);
    _tuple.SetService(_service);
    ims::presence _presence;
    _presence.AddTuple(_tuple);

    char* impiCopy = strdup(impi.c_str());
    char* presUri = strchr(impiCopy, ':');
    if (!presUri) {
        presUri = impiCopy;
    } else {
        presUri += 1;
    }
    String entity("pres:");
    entity += (const char*)presUri;
    _presence.SetEntity(entity);
    free(impiCopy);

    String presenceXml;
    _presence.Serialize(presenceXml);

    if (!pubSession) {
        pubSession = new PublicationSession(stack);
        pubSession->addHeader("Event", "presence");
        pubSession->addHeader("Content-Type", gwConsts::contenttype::PIDF);
        pubSession->setToUri(impu.c_str());
        isNewCreated = true;
    }
    if (pubSession->publish(presenceXml.c_str(), presenceXml.size())) {
    // Wait for the response of the PUBLISH
        std::unique_lock<std::mutex> lock(imsIns->mtxPublish);
        if (condPublish.wait_for(lock, std::chrono::milliseconds(gwConsts::PUBLICATION_DEFAULT_TIMEOUT))) {
            // if publishing is correctly responded
            if (isNewCreated) {
                if (itrPubsession != publications.end()) {
                    itrPubsession->second.pubSession = pubSession;
                    itrPubsession->second.introspectionXml = introspectionXml;
                } else {
                    PublicationInfo pi;
                    pi.pubSession = pubSession;
                    pi.introspectionXml = introspectionXml;
                    publications.insert(std::pair<qcc::String, PublicationInfo>(serviceId, pi));
                }
            }
        } else {
            // if publishing is not correctly responded or timed out (no response)
            if (isNewCreated) {
                delete pubSession;
            }
            return IC_TRANSPORT_IMS_PUB_FAILED;
        }
    } else {
        if (isNewCreated) {
            delete pubSession;
        }
        return IC_TRANSPORT_IMS_PUB_FAILED;
    }

    return IC_OK;
}

IStatus IMSTransport::DeleteService(const char* introspectionXml)
{
    // 1. Get the SIP-ETag corresponding to the service path in the introspectionXml
    // 2. Construct the PUBLISH content with SIP-If-Match header equal to previous SIP-ETag
    // 3. Send the PUBLISH message out
    // 4. if success, delete the SIP-ETag information
    if (!introspectionXml) {
        return IC_BAD_ARG_1;
    }
    StringSource source(introspectionXml);
    XmlParseContext pc(source);
    if (ER_OK != XmlElement::Parse(pc)) {
        return IC_INTROSPECTION_XML_PARSING;
    }
    const XmlElement* rootNode = pc.GetRoot();
    if (rootNode == NULL) {
        return IC_INTROSPECTION_XML_PARSING;
    }
    const String& servicePath = rootNode->GetAttribute("name");

    std::map<qcc::String, PublicationInfo>::iterator itrPubsession = publications.find(servicePath);
    if (itrPubsession != publications.end()) {
        PublicationSession* pubSession = itrPubsession->second.pubSession;
        if (pubSession) {
            if (pubSession->unPublish()) {
                std::unique_lock<std::mutex> lock(imsIns->mtxUnpublish);
                if (condUnpublish.wait_for(lock, std::chrono::milliseconds(gwConsts::PUBLICATION_DEFAULT_TIMEOUT))) {
                    delete pubSession;
                    publications.erase(itrPubsession);
                } else {
                    return IC_TRANSPORT_IMS_PUB_FAILED;
                }
            } else {
                // Unpublish is not finished correctly
                return IC_TRANSPORT_IMS_PUB_FAILED;
            }
        } else {
            return IC_TRANSPORT_IMS_PUB_FAILED;
        }
    } else {
        // the publication session is not found
        return IC_TRANSPORT_IMS_PUB_FAILED;
    }
    return IC_OK;
}

IStatus IMSTransport::ReadServiceNotification(char** msgBuf)
{
    if (!msgBuf) {
        return IC_FAIL;
    }
    if (incomingNotifyQueue.TryDequeue(*msgBuf)) {
        // the queue is not empty and not stopped, meaning a correct notification returned
        
    } else {
        // the queue has been stopped
        // The queue has been stopped
        *msgBuf = NULL;
        return IC_FAIL;
    }
    return IC_OK;
}

void IMSTransport::StopReadServiceNotification()
{
    incomingNotifyQueue.StopQueue();
}

IStatus IMSTransport::ReadCloudMessage(char** msgBuf)
{
    if (!msgBuf) {
        return IC_FAIL;
    }
    if (incomingMsgQueue.TryDequeue(*msgBuf)) {
        // will hopefully be deallocated in ReleaseBuf()
    } else {
        // The queue has been stopped
        *msgBuf = NULL;
        return IC_FAIL;
    }
    return IC_OK;
}

void IMSTransport::StopReadCloudMessage()
{
    incomingMsgQueue.StopQueue();
}

IStatus IMSTransport::SendCloudMessage(int msgType, 
                                       const char* peer, 
                                       const char* callId, 
                                       const char* addr,
                                       const char* msgBuf, 
                                       char** resMsgBuf)
{
    if (!peer) {
        return IC_BAD_ARG_2;
    }
    if (!msgBuf) {
        return IC_BAD_ARG_5;
    }
    if (!msgSession) {
        // The stack or the Message Session are not correctly initialized
        return IC_TRANSPORT_FAIL;
    }
    msgSession->addHeader("Content-Type", gwConsts::contenttype::ALLJOYN_XML);
    char peerUri[gwConsts::MAX_SIP_ADDR_LEN];
    strcpy(peerUri, "sip:");
    strcat(peerUri, peer);
    msgSession->setToUri(peerUri);
    msgSession->addHeader(gwConsts::customheader::RPC_MSG_TYPE, qcc::I32ToString(msgType).c_str());
    switch (msgType) {
    case gwConsts::customheader::RPC_MSG_TYPE_METHOD_CALL:
    case gwConsts::customheader::RPC_MSG_TYPE_PROPERTY_CALL:
    case gwConsts::customheader::RPC_MSG_TYPE_SIGNAL_CALL:
        {
            // if it is an outgoing request, the callId should be NULL, and will be generated here
            // send out request through IMS engine and wait for the response to arrive (blocked for some time)
            if (!addr) {
                return IC_BAD_ARG_4;
            }
            if (!resMsgBuf) {
                return IC_BAD_ARG_6;
            }
            qcc::String callIdStr;
            ajn::services::GuidUtil::GetInstance()->GenerateGUID(&callIdStr);
            msgSession->addHeader(gwConsts::customheader::RPC_CALL_ID, callIdStr.c_str());
            msgSession->addHeader(gwConsts::customheader::RPC_ADDR, addr);
            if (!msgSession->send(msgBuf, strlen(msgBuf))) {
                // if error sending out the message
                return IC_TRANSPORT_FAIL;
            }

            // after sending the request out, the sending thread will be blocked to wait for the response for some time
            if (msgType == gwConsts::customheader::RPC_MSG_TYPE_METHOD_CALL
                || msgType == gwConsts::customheader::RPC_MSG_TYPE_PROPERTY_CALL) {
                // only method call will be hold waiting for the answer
                std::shared_ptr<SyncContext> syncCtx(new SyncContext());
                syncCtx->content = NULL;
                {
                    std::lock_guard<std::mutex> lock(mtxResponseDispatchTable);

                    String reqKey(peer);
                    reqKey += "^";
                    reqKey += callIdStr;
                    responseDispatchTable.insert(std::pair<qcc::String, std::shared_ptr<SyncContext>>(reqKey, syncCtx));
                }
                {
                    std::unique_lock<std::mutex> lock(syncCtx->mtx);
                    if (syncCtx->con.wait_for(lock, 
                        std::chrono::milliseconds(gwConsts::RPC_REQ_TIMEOUT))) {
                        // if no timeout waiting for the response
                        if (syncCtx->content) {
                            *resMsgBuf = syncCtx->content; // will hopefully be deallocated in ReleaseBuf()
                            syncCtx->content = NULL;
                        }/* else {
                            return IC_FAIL;
                        }*/
                    } else {
                        // timeout
                        return IC_TIMEOUT;
                    }
                }
            }
        }
        break;
    case gwConsts::customheader::RPC_MSG_TYPE_METHOD_RET:
    case gwConsts::customheader::RPC_MSG_TYPE_PROPERTY_RET:
    case gwConsts::customheader::RPC_MSG_TYPE_SIGNAL_RET:
        {
            // if it is an outgoing response, just send it out
            if (!callId) {
                return IC_BAD_ARG_3;
            }
            msgSession->addHeader(gwConsts::customheader::RPC_CALL_ID, callId);
            if (!msgSession->send(msgBuf, strlen(msgBuf))) {
                // if error sending out the message
                return IC_TRANSPORT_FAIL;
            }
        }
        break;
    case gwConsts::customheader::RPC_MSG_TYPE_UPDATE_SIGNAL_HANDLER:
        {
            if (!addr) {
                return IC_BAD_ARG_4;
            }
            msgSession->addHeader(gwConsts::customheader::RPC_ADDR, addr);
            if (!msgSession->send(msgBuf, strlen(msgBuf))) {
                // if error sending out the message
                return IC_TRANSPORT_FAIL;
            }
        }
        break;
    default:
        break;
    }
    return IC_OK;
}

void IMSTransport::RegThreadFunc()
{
    unsigned int expires = gwConsts::REGISTRATION_DEFAULT_EXPIRES;
    IMSTransport* ims = IMSTransport::GetInstance();
    while (ims->regCmdQueue.TryDequeue(expires)) {
        ims->regSession->setExpires(expires);
        if (!ims->regSession->register_()) {
            // log
        }
    }
}

/*
void IMSTransport::SubFunc(void* para)
{
    qcc::String subAccount;
    IMSTransport* ims = (IMSTransport*)para;
    
    std::map<qcc::String, bool>::iterator itrSub = ims->subscriptions.begin();
    while (itrSub != ims->subscriptions.end()) {
        if (!itrSub->second) {
            ims->doSubscribe(itrSub->first.c_str());
        }
        itrSub++;
    }
}
*/

/*
void IMSTransport::HeartBeatFunc(void* para)
{
    static bool restartOpSession = false;
    IMSTransport* ims = (IMSTransport*)para;
    if (!ims) {
        return;
    }
    // If the scscf is not present which means REGISTER is not successful, should retry to REGISTER
    if (ims->scscf.empty()) {
        ims->regCmdQueue.Enqueue(ims->regExpires);
        restartOpSession = true;
        return;
    }
    if (restartOpSession || !ims->opSession) {
        if (ims->opSession) {
            delete ims->opSession;
            ims->opSession = NULL;
        }
        ims->opSession = new OptionsSession(ims->stack);
        String scscfUri("sip:");
        scscfUri += ims->scscf;
        ims->opSession->setToUri(scscfUri.c_str());
        restartOpSession = false;
    }
    if (!ims->opSession->send()) {
        // if the heartbeat is not successfully sent, re-register the UAC
        ims->regCmdQueue.Enqueue(ims->regExpires);
        restartOpSession = true;
        return;
    }
    // Wait for the response of the OPTIONS heartbeat
    {
        std::unique_lock<std::mutex> lock(ims->mtxHeartBeat);
        if (!ims->condHeartBeat.wait_for(lock, std::chrono::milliseconds(gwConsts::SIPSTACK_HEARTBEAT_EXPIRES))) {
            // if timeout, the re-register the UAC
            ims->regCmdQueue.Enqueue(ims->regExpires);
            restartOpSession = true;
        }

    }
}
*/


void IMSTransport::SubTimerAlarmListener::AlarmTriggered(const qcc::Alarm& alarm, QStatus reason)
{
    qcc::String subAccount;
    IMSTransport* ims = (IMSTransport*)alarm->GetContext();

    if (ims->imsTransportStatus == gwConsts::IMS_TRANSPORT_STATUS_UNREGISTERED) {
        return;
    }
    std::map<qcc::String, bool>::iterator itrSub = ims->subscriptions.begin();
    while (itrSub != ims->subscriptions.end()) {
        if (!itrSub->second) {
            ims->doSubscribe(itrSub->first.c_str());
        }
        itrSub++;
    }
}


void IMSTransport::HeartBeatTimerAlarmListener::AlarmTriggered(const qcc::Alarm& alarm, QStatus reason)
{
    static bool restartOpSession = false;
    IMSTransport* ims = (IMSTransport*)alarm->GetContext();
    if (!ims) {
        return;
    }
    // If the scscf is not present which means REGISTER is not successful, should retry to REGISTER
    if (ims->scscf.empty()) {
        ims->regCmdQueue.Enqueue(ims->regExpires);
        restartOpSession = true;
        return;
    }
    if (restartOpSession || !ims->opSession) {
        if (ims->opSession) {
            delete ims->opSession;
            ims->opSession = NULL;
        }
        ims->opSession = new OptionsSession(ims->stack);
        String scscfUri("sip:");
        scscfUri += ims->scscf;
        ims->opSession->setToUri(scscfUri.c_str());
        restartOpSession = false;
    }
    if (!ims->opSession->send()) {
        // if the heartbeat is not successfully sent, re-register the UAC
        ims->regCmdQueue.Enqueue(ims->regExpires);
        restartOpSession = true;
        return;
    }
    // Wait for the response of the OPTIONS heartbeat
    {
        std::unique_lock<std::mutex> lock(ims->mtxHeartBeat);
        if (!ims->condHeartBeat.wait_for(lock, std::chrono::milliseconds(gwConsts::SIPSTACK_HEARTBEAT_EXPIRES))) {
            // if timeout, the re-register the UAC
            ims->regCmdQueue.Enqueue(ims->regExpires);
            restartOpSession = true;
        }

    }
}

} // namespace gateway
} // namespace sipe2e