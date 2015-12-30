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
#include <qcc/Debug.h>
#include <qcc/StringSource.h>
#include <qcc/XmlElement.h>
#include <qcc/StringUtil.h>

#include "AboutObjApi.h"
#include "AJInitializer.h"

#include <signal.h>

#include "Common/GatewayConstants.h"
#include "Common/GatewayStd.h"
#include "Common/CommonBusListener.h"
#include "Common/CommonUtils.h"
#include "CloudCommEngine/CloudCommEngineBusObject.h"
#include "CloudCommEngine/IMSTransport/IMSTransportExport.h"
#include "CloudCommEngine/IMSTransport/IMSTransport.h"
#include "CloudCommEngine/IMSTransport/IMSTransportConstants.h"


#define QCC_MODULE "CloudCommEngine"

using namespace std;
using namespace qcc;
using namespace ajn;

using namespace sipe2e;
using namespace gateway;

AJInitializer ajInit;

/** Top level message bus object. */
static BusAttachment* s_bus = NULL;
static CommonBusListener* s_busListener = NULL;
static AboutObj* s_cceAboutObj = NULL;
static AboutData* s_cceAboutData = NULL;
static CloudCommEngineBusObject* s_cceBusObject = NULL;

static volatile sig_atomic_t s_interrupt = false;

static volatile sig_atomic_t s_restart = false;

static void CDECL_CALL SigIntHandler(int sig)
{
    s_interrupt = true;
}

static void daemonDisconnectCB(void* arg)
{
    s_restart = true;
}

QStatus fillAboutData(AboutData* aboutdata, qcc::String const& appIdHex,
                                        qcc::String const& appName, qcc::String const& deviceId, std::map<qcc::String, qcc::String> const& deviceNames,
                                        qcc::String const& defaultLanguage)
{
    if (!aboutdata) {
        return ER_BAD_ARG_1;
    }

    QStatus status = ER_OK;

    if (!appIdHex.empty()) {
        CHECK_RETURN(aboutdata->SetAppId(appIdHex.c_str()));
    }

    if (deviceId != "") {
        CHECK_RETURN(aboutdata->SetDeviceId(deviceId.c_str()))
    }

    std::vector<qcc::String> languages(2);
    languages[0] = "en";
    languages[1] = "zh";

    for (size_t i = 0; i < languages.size(); i++) {
        CHECK_RETURN(aboutdata->SetSupportedLanguage(languages[i].c_str()))
    }

    if (defaultLanguage != "") {
        CHECK_RETURN(aboutdata->SetDefaultLanguage(defaultLanguage.c_str()))
    }

    if (appName != "") {
        CHECK_RETURN(aboutdata->SetAppName(appName.c_str(), languages[0].c_str()))
        CHECK_RETURN(aboutdata->SetAppName(appName.c_str(), languages[1].c_str()))
    }

    CHECK_RETURN(aboutdata->SetModelNumber("Thunder001"))
    CHECK_RETURN(aboutdata->SetDateOfManufacture("12/1/2015"))
    CHECK_RETURN(aboutdata->SetSoftwareVersion("1.0.0"))
    CHECK_RETURN(aboutdata->SetHardwareVersion("1.0.0"))

    std::map<qcc::String, qcc::String>::const_iterator iter = deviceNames.find(languages[0]);
    if (iter != deviceNames.end()) {
        CHECK_RETURN(aboutdata->SetDeviceName(iter->second.c_str(), languages[0].c_str()));
    } else {
        CHECK_RETURN(aboutdata->SetDeviceName("CloudCommEngine", "en"));
    }

    iter = deviceNames.find(languages[1]);
    if (iter != deviceNames.end()) {
        CHECK_RETURN(aboutdata->SetDeviceName(iter->second.c_str(), languages[1].c_str()));
    } else {
        CHECK_RETURN(aboutdata->SetDeviceName("云端通讯引擎", "zh"));
    }


    CHECK_RETURN(aboutdata->SetDescription("Cloud Communication Engine", "en"))
    CHECK_RETURN(aboutdata->SetDescription("云端通讯引擎", "zh"))

    CHECK_RETURN(aboutdata->SetManufacturer("AllSeen Alliance", "en"))
    CHECK_RETURN(aboutdata->SetManufacturer("AllSeen联盟", "zh"))

    CHECK_RETURN(aboutdata->SetSupportUrl("http://www.allseenalliance.org"))

    if (!aboutdata->IsValid()) {
        printf("failed to setup about data.\n");
        return ER_FAIL;
    }
    return status;
}

QStatus prepareAboutService(BusAttachment* bus, AboutData* aboutData, AboutObj* aboutObj,
                                              CommonBusListener* busListener, uint16_t port)
{
    if (!bus) {
        return ER_BAD_ARG_1;
    }

    if (!aboutData) {
        return ER_BAD_ARG_2;
    }

    if (!busListener) {
        return ER_BAD_ARG_3;
    }

    services::AboutObjApi::Init(bus, aboutData, aboutObj);
    services::AboutObjApi* aboutService = services::AboutObjApi::getInstance();
    if (!aboutService) {
        return ER_BUS_NOT_ALLOWED;
    }
    aboutService->SetPort(port);

    busListener->setSessionPort(port);
    bus->RegisterBusListener(*busListener);

    TransportMask transportMask = TRANSPORT_ANY;
    SessionPort sp = port;
    SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, false, SessionOpts::PROXIMITY_ANY, transportMask);

    QStatus status = bus->BindSessionPort(sp, opts, *busListener);
    if (status != ER_OK) {
        return status;
    }

    return ER_OK;
}

void cleanup()
{
    if (s_bus && s_busListener) {
        s_bus->UnregisterBusListener(*s_busListener);
        s_bus->UnbindSessionPort(s_busListener->getSessionPort());
    }
    if (s_cceBusObject) {
        if (s_bus) {
            s_bus->UnregisterBusObject(*s_cceBusObject);
        }
        s_cceBusObject->Cleanup();
        delete s_cceBusObject;
        s_cceBusObject = NULL;
    }
    /* Destroying the AboutService must be after deletion of s_pceBusObject where AboutService will unregister the s_pceBusObject */
    if (s_cceAboutData) {
        delete s_cceAboutData;
        s_cceAboutData = NULL;
    }
    if (s_cceAboutObj) {
        delete s_cceAboutObj;
        s_cceAboutObj = NULL;
    }
    services::AboutObjApi::DestroyInstance();
    if (s_busListener) {
        delete s_busListener;
        s_busListener = NULL;
    }
    if (s_bus) {
        delete s_bus;
        s_bus = NULL;
    }
    ITShutdown();
}


int CDECL_CALL main(int argc, char** argv, char** envArg)
{
    /* Install SIGINT handler */
    signal(SIGINT, SigIntHandler);

    QStatus status = ER_OK;

    status = ajInit.Initialize();
    if (ER_OK != status) {
        return -1;
    }

start:
    // Initialize the IMSTransport
    if (0 != ITInitialize())
    {
        return ER_FAIL;
    }

    while (gwConsts::IMS_TRANSPORT_STATUS_REGISTERED != ITGetStatus()) {
#ifdef QCC_OS_GROUP_WINDOWS
        ::sleep(200);
#else
        usleep(200000);
#endif
    }

    /* Prepare bus attachment */
    s_bus = new BusAttachment(gwConsts::SIPE2E_CLOUDCOMMENGINE_NAME.c_str(), true);
    if (!s_bus) {
        ITShutdown();
        status = ER_OUT_OF_MEMORY;
        return status;
    }
    status = s_bus->Start();
    if (ER_OK != status) {
        ITShutdown();
        delete s_bus;
        return status;
    }

    /* Prepare the BusListener */
    s_busListener = new CommonBusListener(s_bus, daemonDisconnectCB);
    s_bus->RegisterBusListener(*s_busListener);

    status = s_bus->Connect();
    if (ER_OK != status) {
        ITShutdown();
        delete s_bus;
        return status;
    }

    /* Prepare About */
    qcc::String device_id("366AAE9E-F6C0-4E6D-8509-87155B870393"), app_id("93778825-238D-4B05-8406-5CC95C7F630E");
    qcc::String app_name = gwConsts::SIPE2E_CLOUDCOMMENGINE_NAME;
    map<qcc::String, qcc::String> deviceNames;
    deviceNames.insert(pair<String, String>("en", gwConsts::SIPE2E_CLOUDCOMMENGINE_NAME.c_str()));
    deviceNames.insert(pair<String, String>("zh", "云端通讯引擎"));

    s_cceAboutData = new AboutData("en");
    status = fillAboutData(s_cceAboutData, app_id, (gwConsts::SIPE2E_CLOUDCOMMENGINE_NAME + " App").c_str(),
        device_id, deviceNames, "en");
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while filling the AboutData"));
        cleanup();
        return status;
    }

    s_cceAboutObj = new AboutObj(*s_bus);
    status = prepareAboutService(s_bus, s_cceAboutData, s_cceAboutObj, s_busListener, gwConsts::SIPE2E_CLOUDCOMMENGINE_SESSION_PORT);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while preparing the about service"));
        cleanup();
        return status;
    }
    s_cceBusObject = new CloudCommEngineBusObject(gwConsts::SIPE2E_CLOUDCOMMENGINE_OBJECTPATH, gwConsts::CLOUD_METHOD_CALL_THREAD_POOL_SIZE);
    status = s_cceBusObject->Init(*s_bus/*, *s_cceAboutObj*/);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while initializing CloudCommEngine"));
        cleanup();
        return status;
    }

    status = services::AboutObjApi::getInstance()->Announce();
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while announcing"));
        cleanup();
        return status;
    }


    while (s_interrupt == false) {
        qcc::Sleep(200);
    }

    cleanup();


    return 0;
}