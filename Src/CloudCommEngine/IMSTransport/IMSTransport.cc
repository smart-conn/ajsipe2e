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
#include "CloudCommEngine/IMSTransport/IMSTransport.h"
#include "CloudCommEngine/IMSTransport/IMSTransportSipCallback.h"
#include "CloudCommEngine/IMSTransport/IMSTransportConstants.h"
#include "CloudCommEngine/IMSTransport/pidf.h"

#include "Common/SimpleTimer.h"

#include <qcc/XmlElement.h>
#include <qcc/StringSource.h>
#include <qcc/StringUtil.h>

#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

#include <SipStack.h>
#include <SipSession.h>

#if defined( _MSC_VER )
#include <direct.h>        // _getcwd
// #include <crtdbg.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(MINGW32) || defined(__MINGW32__)
#include <io.h>  // mkdir
#else
#include <sys/stat.h>    // mkdir
#endif


using namespace qcc;

namespace sipe2e {
namespace gateway {

static IMSTransport* imsIns = NULL;


IMSTransport::IMSTransport()
    : stack(NULL), sipCB(NULL), imsTransportStatus(gwConsts::IMS_TRANSPORT_STATUS_UNREGISTERED),
    realm(gwConsts::DEFAULT_REALM), pcscfPort(gwConsts::DEFAULT_PCSCF_PORT),
    regSession(NULL), opSession(NULL), msgSession(NULL),
    regThread(NULL), timerHeartBeat(NULL), regExpires(gwConsts::REGISTRATION_DEFAULT_EXPIRES)
{
}


IMSTransport::~IMSTransport()
{
    if (regThread) {
        regCmdQueue.StopQueue();
        regThread->try_join_for(boost::chrono::milliseconds(1000)); /// wait for the thread to end
    }
    if (timerHeartBeat) {
        timerHeartBeat->Stop();
        delete timerHeartBeat;
        timerHeartBeat = NULL;
    }
    if (timerSub) {
        timerSub->Stop();
        delete timerSub;
        timerSub = NULL;
    }
    incomingNotifyQueue.StopQueue();
    incomingMsgQueue.StopQueue();
    if (regSession) {
        for (int i = 0; i < 5; i++) { // try to unregister for 5 times with 5 seconds timeout every time
            regSession->unRegister();
            {
                boost::unique_lock<boost::mutex> lock(mtxUnregister);
                if (condUnregister.timed_wait(lock, boost::posix_time::milliseconds(gwConsts::REGISTRATION_DEFAULT_TIMEOUT))) {
                    break; // unregister successful
                }
            }
        }
        delete regSession;
        regSession = NULL;
        imsTransportStatus = gwConsts::IMS_TRANSPORT_STATUS_UNREGISTERED;
    }
    if (opSession) {
        delete opSession;
        opSession = NULL;
    }
    if (msgSession) {
        delete msgSession;
        msgSession = NULL;
    }
    std::map<std::string, SubscriptionSession*>::iterator itrSubsession = subSessions.begin();
    while (itrSubsession != subSessions.end()) {
        SubscriptionSession* subSession = itrSubsession->second;
        if (subSession) {
            // do not need to unsubscribe to the remote account, since the service may be still on
            // but the subscriber may be (temporarily) down.
//             subSession->unSubscribe();
            delete subSession;
            subSession = NULL;
        }
    }
    std::map<std::string, PublicationSession*>::iterator itrPubsession = pubSessions.begin();
    while (itrPubsession != pubSessions.end()) {
        PublicationSession* pubSession = itrPubsession->second;
        if (pubSession) {
            pubSession->unPublish();
            delete pubSession;
            pubSession = NULL;
        }
        itrPubsession++;
    }
    if (stack) {
        stack->deInitialize();
        stack->stop();
        delete stack;
        stack = NULL;
    }
    if (sipCB) {
        delete sipCB;
        sipCB = NULL;
    }
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

/*
    xmlDocPtr doc = xmlParseFile(confFile);
    if (doc == NULL) {
        return IC_CONF_FILE_NOT_FOUND;
    }
    xmlNodePtr rootNode = xmlDocGetRootElement(doc);
    if (rootNode == NULL) {
        return IC_CONF_XML_PARSING;
    }
    xmlNodePtr childNode = rootNode->xmlChildrenNode;
    while (childNode != NULL) {
        if ((!xmlStrcmp(childNode->name, (const xmlChar*)"Transport"))) {
            xmlNodePtr transportChildNode = childNode->xmlChildrenNode;
            while (transportChildNode != NULL) {
                if ((!xmlStrcmp(transportChildNode->name, (const xmlChar*)"IMS"))) {
                    xmlNodePtr imsChildNode = transportChildNode->xmlChildrenNode;
                    while (imsChildNode != NULL) {
                        if ((!xmlStrcmp(imsChildNode->name, (const xmlChar*)"Realm"))) {
                            xmlChar* realmXmlChar = xmlNodeGetContent(imsChildNode);
                            if (realmXmlChar) {
                                realm = (const char*)realmXmlChar;
                            }
                        } else if ((!xmlStrcmp(imsChildNode->name, (const xmlChar*)"IMPI"))) {
                            xmlChar* impiXmlChar = xmlNodeGetContent(imsChildNode);
                            if (impiXmlChar) {
                                impi = (const char*)impiXmlChar;
                            } else {
                                return IC_CONF_XML_PARSING;
                            }
                        } else if ((!xmlStrcmp(imsChildNode->name, (const xmlChar*)"IMPU"))) {
                            xmlChar* impuXmlChar = xmlNodeGetContent(imsChildNode);
                            if (impuXmlChar) {
                                impu = (const char*)impuXmlChar;
                            } else {
                                return IC_CONF_XML_PARSING;
                            }
                        } else if ((!xmlStrcmp(imsChildNode->name, (const xmlChar*)"Password"))) {
                            xmlChar* passwordXmlChar = xmlNodeGetContent(imsChildNode);
                            if (passwordXmlChar) {
                                password = (const char*)passwordXmlChar;
                            }
                        } else if ((!xmlStrcmp(imsChildNode->name, (const xmlChar*)"pCSCF"))) {
                            xmlChar* pcscfXmlChar = xmlGetProp(imsChildNode, (const xmlChar*)"domain");
                            if (pcscfXmlChar) {
                                pcscf = (const char*)pcscfXmlChar;
                            } else {
                                return IC_CONF_XML_PARSING;
                            }

                            xmlChar* pcscfPortXmlChar = xmlGetProp(imsChildNode, (const xmlChar*)"port");
                            if (pcscfPortXmlChar) {
                                pcscfPort = atoi((const char*)pcscfPortXmlChar);
                            } else {
                                return IC_CONF_XML_PARSING;
                            }

                            xmlChar* pcscfTransportXmlChar = xmlGetProp(imsChildNode, (const xmlChar*)"transport");
                            if (pcscfTransportXmlChar) {
                                pcscfTransport = (const char*)pcscfTransportXmlChar;
                            } else {
                                return IC_CONF_XML_PARSING;
                            }

                            xmlChar* pcscfIpversionXmlChar = xmlGetProp(imsChildNode, (const xmlChar*)"transport");
                            if (pcscfIpversionXmlChar) {
                                pcscfIpversion = (const char*)pcscfIpversionXmlChar;
                            } else {
                                return IC_CONF_XML_PARSING;
                            }

                        } else if ((!xmlStrcmp(imsChildNode->name, (const xmlChar*)"Expires"))) {
                            xmlChar* expiresXmlChar = xmlNodeGetContent(imsChildNode);
                            if (expiresXmlChar) {
                                regExpires = atoi((const char*)expiresXmlChar);
                            }
                        }
                        imsChildNode = imsChildNode->next;
                    }
                    break;
                }
                transportChildNode = transportChildNode->next;
            }
            break;
        }
        childNode = childNode->next;
    }
*/

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
    regThread = new boost::thread(IMSTransport::RegThreadFunc);
    regCmdQueue.Enqueue(regExpires); // registration command

    /**
     * TBD
     */
    timerSub = new SimpleTimer();
    timerSub->Start(gwConsts::SIPSTACK_HEARTBEAT_INTERVAL, IMSTransport::SubFunc, this);

    /**
     * Start a timer for heartbeat OPTIONS
     */
    timerHeartBeat = new SimpleTimer();
    timerHeartBeat->Start(gwConsts::SIPSTACK_HEARTBEAT_INTERVAL, IMSTransport::HeartBeatFunc, this);

    return IC_OK;
}

IStatus IMSTransport::Subscribe(const char* remoteAccount)
{
    if (!remoteAccount) {
        return IC_BAD_ARG_1;
    }
    std::map<std::string, bool>::iterator itrSub = subscriptions.find((std::string)remoteAccount);
    if (itrSub != subscriptions.end()) {
        itrSub->second = false;
    } else {
        subscriptions.insert(std::pair<std::string, bool>((std::string)remoteAccount, false));
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
    std::map<std::string, SubscriptionSession*>::iterator itrSubsession = subSessions.find((std::string)remoteAccount);
    if (itrSubsession != subSessions.end()) {
        subSession = itrSubsession->second;
    }
    if (!subSession) {
        subSession = new SubscriptionSession(stack);
        subSession->setToUri(remoteAccount);
        subSession->addHeader("Event", "presence");
        isNewCreated = true;
    }
    if (subSession->subscribe()) {
        // wait for the response of the SUBSCRIBE
        boost::unique_lock<boost::mutex> lock(imsIns->mtxSubscribe);

        // Note: here we set a timer to wait for the response. This mechanism is not correct in case of
        // multi-thread environment where multi users call Subscribe at the same time.
        // Should be changed using the session Call-ID to differentiate multiple subscribe sessions
        // TBD
        if (condSubscribe.timed_wait(lock, boost::posix_time::milliseconds(gwConsts::SUBSCRIPTION_DEFAULT_TIMEOUT))) {
            // if subscribing is correctly responded
            if (isNewCreated) {
                subSessions.insert(std::pair<std::string, SubscriptionSession*>((std::string)remoteAccount, subSession));
            }
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
    std::map<std::string, SubscriptionSession*>::iterator itrSubsession = subSessions.find((std::string)remoteAccount);
    if (itrSubsession != subSessions.end()) {
        SubscriptionSession* subSession = itrSubsession->second;
    } else {
        // If a subsession is not present, should construct a new subsession
        subSession = new SubscriptionSession(stack);
        subSession->setToUri(remoteAccount);
        subSession->addHeader("Event", "presence");
        subSessions.insert(std::pair<std::string, SubscriptionSession*>((std::string)remoteAccount, subSession));
    }
    if (subSession) {
        if (subSession->unSubscribe()) {
            boost::unique_lock<boost::mutex> lock(imsIns->mtxUnsubscribe);

            // Note: here we set a timer to wait for the response. This mechanism is not correct in case of
            // multi-thread environment where multi users call Subscribe at the same time.
            // Should be changed using the session Call-ID to differentiate multiple subscribe sessions
            // TBD
            if (condUnsubscribe.timed_wait(lock, boost::posix_time::milliseconds(gwConsts::SUBSCRIPTION_DEFAULT_TIMEOUT))) {
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
/*
    xmlDocPtr doc = xmlParseMemory(introspectionXml, strlen(introspectionXml));
    if (doc == NULL) {
        return IC_INTROSPECTION_XML_PARSING;
    }
    xmlNodePtr rootNode = xmlDocGetRootElement(doc);
    if (rootNode == NULL) {
        return IC_INTROSPECTION_XML_PARSING;
    }
    xmlChar* servicePath = xmlGetProp(rootNode, (const xmlChar*)"name");
    if (!servicePath) {
        return IC_INTROSPECTION_XML_PARSING;
    }
    */
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

    ims::service _service;
    _service.SetIntrospectionXml((String)introspectionXml);
    ims::status _status;
    _status.SetBasicStatus(ims::basic::open);
    ims::tuple _tuple;
    _tuple.SetId(servicePath);
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

    PublicationSession* pubSession = NULL;
    bool isNewCreated = false;
    std::map<std::string, PublicationSession*>::iterator itrPubsession = pubSessions.find((std::string)(const char*)servicePath.c_str());
    if (itrPubsession != pubSessions.end()) {
        pubSession = itrPubsession->second;
    }
    if (!pubSession) {
        pubSession = new PublicationSession(stack);
        pubSession->addHeader("Event", "presence");
        pubSession->addHeader("Content-Type", gwConsts::contenttype::PIDF);
        pubSession->setToUri(impu.c_str());
        isNewCreated = true;
    }
    if (pubSession->publish(presenceXml.c_str(), presenceXml.size())) {
    // Wait for the response of the PUBLISH
        boost::unique_lock<boost::mutex> lock(imsIns->mtxPublish);
        if (condPublish.timed_wait(lock, boost::posix_time::milliseconds(gwConsts::PUBLICATION_DEFAULT_TIMEOUT))) {
            // if publishing is correctly responded
            if (isNewCreated) {
                pubSessions.insert(std::pair<std::string, PublicationSession*>((std::string)(const char*)servicePath.c_str(), pubSession));
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
/*
    xmlDocPtr doc = xmlParseMemory(introspectionXml, strlen(introspectionXml));
    if (doc == NULL) {
        return IC_INTROSPECTION_XML_PARSING;
    }
    xmlNodePtr rootNode = xmlDocGetRootElement(doc);
    if (rootNode == NULL) {
        return IC_INTROSPECTION_XML_PARSING;
    }
    xmlChar* servicePath = xmlGetProp(rootNode, (const xmlChar*)"name");
    if (!servicePath) {
        return IC_INTROSPECTION_XML_PARSING;
    }
*/
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

    std::map<std::string, PublicationSession*>::iterator itrPubsession = pubSessions.find((std::string)(const char*)servicePath.c_str());
    if (itrPubsession != pubSessions.end()) {
        PublicationSession* pubSession = itrPubsession->second;
        if (pubSession) {
            if (pubSession->unPublish()) {
                boost::unique_lock<boost::mutex> lock(imsIns->mtxUnpublish);
                if (condUnpublish.timed_wait(lock, boost::posix_time::milliseconds(gwConsts::PUBLICATION_DEFAULT_TIMEOUT))) {
                    delete pubSession;
                    pubSessions.erase(itrPubsession);
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

boost::shared_array<char> IMSTransport::ReadServiceNotification()
{
    boost::shared_array<char> notification;
    if (incomingNotifyQueue.TryDequeue(notification)) {
        // the queue is not empty and not stopped, meaning a correct notification returned
        
    } else {
        // the queue has been stopped
    }
    return notification;
}

void IMSTransport::StopReadServiceNotification()
{
    incomingNotifyQueue.StopQueue();
}

IStatus IMSTransport::ReadCloudMessage(char** msgBuf)
{
#if 0
    ::Sleep(1000);
#endif
    if (!msgBuf) {
        return IC_FAIL;
    }
    boost::shared_array<char> msg;
    if (incomingMsgQueue.TryDequeue(msg)) {
        size_t len = strlen(msg.get());
        *msgBuf = new char[len + 1]; // will hopefully be deallocated in ReleaseBuf()
        memcpy(*msgBuf, msg.get(), len + 1);
    } else {
        // The queue has been stopped
        *msgBuf = NULL;
        return IC_FAIL;
    }
    return IC_OK;
}

IStatus IMSTransport::SendCloudMessage(bool isRequest, 
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
    if (isRequest) {
        // if it is an outgoing request, the callId should be NULL, and will be generated here
        // send out request through IMS engine and wait for the response to arrive (blocked for some time)
        if (!addr) {
            return IC_BAD_ARG_4;
        }
        if (!resMsgBuf) {
            return IC_BAD_ARG_6;
        }
        msgSession->addHeader(gwConsts::customheader::RPC_MSG_TYPE, "1");
        boost::uuids::uuid callIdTmp = callIdGenerator();
        std::string callIdStr = boost::lexical_cast<std::string>(callIdTmp);
        msgSession->addHeader(gwConsts::customheader::RPC_CALL_ID, callIdStr.c_str());
        msgSession->addHeader(gwConsts::customheader::RPC_ADDR, addr);
        if (!msgSession->send(msgBuf, strlen(msgBuf))) {
            // if error sending out the message
            return IC_TRANSPORT_FAIL;
        }

        // after sending the request out, the sending thread will be blocked to wait for the response for some time
        boost::shared_ptr<SyncContext> syncCtx(new SyncContext());
        syncCtx->content = NULL;
        {
            boost::lock_guard<boost::mutex> lock(mtxResponseDispatchTable);

            std::string reqKey(peer);
            reqKey += "^";
            reqKey += callIdStr;
            responseDispatchTable.insert(std::pair<std::string, boost::shared_ptr<SyncContext>>(reqKey, syncCtx));
        }
        {
            boost::unique_lock<boost::mutex> lock(syncCtx->mtx);
            if (syncCtx->con.timed_wait(lock, 
                boost::posix_time::milliseconds(gwConsts::RPC_REQ_TIMEOUT))) {
                // if no timeout waiting for the response
                if (syncCtx->content) {
                    size_t len = strlen(syncCtx->content.get());
                    *resMsgBuf = new char[len + 1]; // will hopefully be deallocated in ReleaseBuf()
                    memcpy(*resMsgBuf, syncCtx->content.get(), len + 1);
                }/* else {
                    return IC_FAIL;
                }*/
            } else {
                // timeout
                return IC_TIMEOUT;
            }
        }
    } else {
        // if it is an outgoing response, just send it out
        if (!callId) {
            return IC_BAD_ARG_3;
        }
        msgSession->addHeader(gwConsts::customheader::RPC_MSG_TYPE, "0");
        msgSession->addHeader(gwConsts::customheader::RPC_CALL_ID, callId);
        if (!msgSession->send(msgBuf, strlen(msgBuf))) {
            // if error sending out the message
            return IC_TRANSPORT_FAIL;
        }
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

void IMSTransport::SubFunc(void* para)
{
    std::string subAccount;
    IMSTransport* ims = (IMSTransport*)para;
    
    std::map<std::string, bool>::iterator itrSub = ims->subscriptions.begin();
    while (itrSub != ims->subscriptions.begin()) {
        if (!itrSub->second) {
            ims->doSubscribe(itrSub->first.c_str());
        }
        itrSub++;
    }
}

void IMSTransport::HeartBeatFunc(void* para)
{
    static bool restartOpSession = false;
    IMSTransport* ims = (IMSTransport*)para;
    if (!ims) {
        return;
    }
    // If the scscf is not present which means REGISTER is not successfull, should retry to REGISTER
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
        boost::unique_lock<boost::mutex> lock(ims->mtxHeartBeat);
        if (!ims->condHeartBeat.timed_wait(lock, boost::posix_time::milliseconds(gwConsts::SIPSTACK_HEARTBEAT_EXPIRES))) {
            // if timeout, the re-register the UAC
            ims->regCmdQueue.Enqueue(ims->regExpires);
            restartOpSession = true;
        }

    }
}

} // namespace gateway
} // namespace sipe2e