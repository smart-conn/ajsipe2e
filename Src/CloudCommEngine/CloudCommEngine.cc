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
#include "CloudCommEngine/IMSTransport/IMSTransportConstants.h"
#include <qcc/StringSource.h>
#include <qcc/XmlElement.h>
#include <qcc/StringUtil.h>

#include <alljoyn/about/AboutPropertyStoreImpl.h>
#include <alljoyn/about/AnnouncementRegistrar.h>

#include <Common/GuidUtil.h>

#include <signal.h>

#include "Common/GatewayConstants.h"
#include "Common/GatewayStd.h"
#include "Common/CommonBusListener.h"
#include "Common/CommonUtils.h"
#include "CloudCommEngine/IMSTransport/IMSTransportExport.h"
#include "CloudCommEngine/CloudCommEngineBusObject.h"


#define QCC_MODULE "SIPE2E"

using namespace std;
using namespace qcc;
using namespace ajn;

using namespace sipe2e;
using namespace gateway;

/** Top level message bus object. */
static BusAttachment* s_bus = NULL;
static CommonBusListener* s_busListener = NULL;
static services::AboutPropertyStoreImpl* s_propertyStoreImpl = NULL;
static services::AboutService* s_cceAboutService = NULL;
static CloudCommEngineBusObject* s_cceBusObject = NULL;
class CloudCommEngineAnnounceHandler;
static CloudCommEngineAnnounceHandler* s_announceHandler = NULL;

static volatile sig_atomic_t s_interrupt = false;

static volatile sig_atomic_t s_restart = false;

static void SigIntHandler(int sig)
{
    s_interrupt = true;
}

static void daemonDisconnectCB(void* arg)
{
    s_restart = true;
}


void cleanup()
{
    if (s_bus && s_busListener) {
        s_bus->UnregisterBusListener(*s_busListener);
        s_bus->UnbindSessionPort(s_busListener->getSessionPort());
    }
/*
    if (s_pceProxyBusObject->IsValid()) {
        ManagedObj<ProxyBusObject> tmp;
        s_pceProxyBusObject = tmp;
    }
*/
    if (s_cceBusObject) {
        if (s_bus) {
            s_bus->UnregisterBusObject(*s_cceBusObject);
        }
        s_cceBusObject->Cleanup();
        delete s_cceBusObject;
        s_cceBusObject = NULL;
    }
    /* Destroying the AboutService must be after deletion of s_pceBusObject where AboutService will unregister the s_pceBusObject */
    if (s_cceAboutService) {
        delete s_cceAboutService;
        s_cceAboutService = NULL;
    }
    if (s_busListener) {
        delete s_busListener;
        s_busListener = NULL;
    }
    if (s_propertyStoreImpl) {
        delete s_propertyStoreImpl;
        s_propertyStoreImpl = NULL;
    }
    if (s_announceHandler) {
        if (s_bus) {
            services::AnnouncementRegistrar::UnRegisterAllAnnounceHandlers(*s_bus);
        }
        delete s_announceHandler;
        s_announceHandler = NULL;
    }
    if (s_bus) {
        delete s_bus;
        s_bus = NULL;
    }
}


QStatus fillPropertyStore(services::AboutPropertyStoreImpl* propertyStore, String const& appIdHex,
                                            String const& appName, String const& deviceId, map<String, String> const& deviceNames,
                                            String const& defaultLanguage)
{
    if (!propertyStore) {
        return ER_BAD_ARG_1;
    }

    QStatus status = ER_OK;

    CHECK_RETURN(propertyStore->setDeviceId(deviceId))
    CHECK_RETURN(propertyStore->setAppId(appIdHex))
    CHECK_RETURN(propertyStore->setAppName(appName))

    std::vector<qcc::String> languages(2);
    languages[0] = "en";
    languages[1] = "zh";
    CHECK_RETURN(propertyStore->setSupportedLangs(languages))
    CHECK_RETURN(propertyStore->setDefaultLang(defaultLanguage))

    CHECK_RETURN(propertyStore->setModelNumber("sipe2e002"))
    CHECK_RETURN(propertyStore->setDateOfManufacture("1/1/2015"))
    CHECK_RETURN(propertyStore->setSoftwareVersion("1.0.0 build 1"))
    CHECK_RETURN(propertyStore->setAjSoftwareVersion(ajn::GetVersion()))
    CHECK_RETURN(propertyStore->setHardwareVersion("1.0.0"))

    map<String, String>::const_iterator iter = deviceNames.find(languages[0]);
    if (iter != deviceNames.end()) {
        CHECK_RETURN(propertyStore->setDeviceName(iter->second.c_str(), languages[0]));
    } else {
        CHECK_RETURN(propertyStore->setDeviceName(gwConsts::SIPE2E_CLOUDCOMMENGINE_NAME.c_str(), "en"));
    }

    iter = deviceNames.find(languages[1]);
    if (iter != deviceNames.end()) {
        CHECK_RETURN(propertyStore->setDeviceName(iter->second.c_str(), languages[1]));
    } else {
        CHECK_RETURN(propertyStore->setDeviceName("云端通讯引擎", "zh"));
    }

    CHECK_RETURN(propertyStore->setDescription("Cloud Communication Engine", "en"))
    CHECK_RETURN(propertyStore->setDescription("云端通讯引擎", "zh"))

    CHECK_RETURN(propertyStore->setManufacturer("Beijing HengShengDongYang Technology Ltd.", "en"))
    CHECK_RETURN(propertyStore->setManufacturer("北京恒胜东阳科技有限公司", "zh"))

    CHECK_RETURN(propertyStore->setSupportUrl("http://www.nane.cn"))

    return status;
}
QStatus prepareAboutService(BusAttachment* bus, services::AboutPropertyStoreImpl* propertyStore,
                                              CommonBusListener* busListener, uint16_t port)
{
    if (!bus) {
        return ER_BAD_ARG_1;
    }

    if (!propertyStore) {
        return ER_BAD_ARG_2;
    }

    if (!busListener) {
        return ER_BAD_ARG_3;
    }

    s_cceAboutService = new services::AboutService(*bus, *propertyStore);
    if (!s_cceAboutService) {
        return ER_BUS_NOT_ALLOWED;
    }

    busListener->setSessionPort(port);
    bus->RegisterBusListener(*busListener);

    TransportMask transportMask = TRANSPORT_ANY;
    SessionPort sp = port;
    SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, false, SessionOpts::PROXIMITY_ANY, transportMask);

    QStatus status = bus->BindSessionPort(sp, opts, *busListener);
    if (status != ER_OK) {
        return status;
    }

    status = s_cceAboutService->Register(port);
    if (status != ER_OK) {
        return status;
    }

    return (bus->RegisterBusObject(*s_cceAboutService));
}



int main(int argc, char** argv, char** envArg)
{
    /* Install SIGINT handler */
    signal(SIGINT, SigIntHandler);

    QStatus status = ER_OK;

start:
    // Initialize the IMSTransport
    if (0 != ITInitialize())
    {
        return ER_FAIL;
    }

    while (gwConsts::IMS_TRANSPORT_STATUS_REGISTERED != ITGetStatus()) {
        qcc::Sleep(100);
    }

    /* Prepare bus attachment */
    s_bus = new BusAttachment(gwConsts::SIPE2E_CLOUDCOMMENGINE_NAME.c_str(), true);
    if (!s_bus) {
        status = ER_OUT_OF_MEMORY;
        return status;
    }
    status = s_bus->Start();
    if (ER_OK != status) {
        delete s_bus;
        return status;
    }

    /* Prepare the BusListener */
    s_busListener = new CommonBusListener(s_bus, daemonDisconnectCB);
    s_bus->RegisterBusListener(*s_busListener);

    status = s_bus->Connect();
    if (ER_OK != status) {
        delete s_bus;
        return status;
    }

    /* Prepare About */
    qcc::String device_id, app_id;
    qcc::String app_name = gwConsts::SIPE2E_CLOUDCOMMENGINE_NAME;
    map<String, String> deviceNames;
    deviceNames.insert(pair<String, String>("en", gwConsts::SIPE2E_CLOUDCOMMENGINE_NAME.c_str()));
    deviceNames.insert(pair<String, String>("zh", "云端通讯引擎"));
    services::GuidUtil::GetInstance()->GetDeviceIdString(&device_id);
    services::GuidUtil::GetInstance()->GenerateGUID(&app_id);

    s_propertyStoreImpl = new services::AboutPropertyStoreImpl();
    status = fillPropertyStore(s_propertyStoreImpl, app_id, app_name, device_id, deviceNames, String("en"));
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while filling the property store"));
        cleanup();
        return status;
    }
    status = prepareAboutService(s_bus, s_propertyStoreImpl, s_busListener, gwConsts::SIPE2E_CLOUDCOMMENGINE_SESSION_PORT);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while preparing the about service"));
        cleanup();
        return status;
    }
    s_cceBusObject = new CloudCommEngineBusObject(gwConsts::SIPE2E_CLOUDCOMMENGINE_OBJECTPATH, gwConsts::CLOUD_METHOD_CALL_THREAD_POOL_SIZE);
    status = s_cceBusObject->Init(*s_bus, *s_cceAboutService);
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while initializing CloudCommEngine"));
        cleanup();
        return status;
    }

    status = s_cceAboutService->Announce();
    if (ER_OK != status) {
        QCC_LogError(status, ("Error while announcing"));
        cleanup();
        return status;
    }


    while (s_interrupt == false) {
        qcc::Sleep(100);
    }

    cleanup();

    ITShutdown();


    return 0;
}