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

#include <alljoyn/about/AboutPropertyStoreImpl.h>
#include <alljoyn/about/AnnouncementRegistrar.h>

#include "Common/GuidUtil.h"

#include <qcc/Thread.h>

#include "Common/GatewayConstants.h"
#include "Common/GatewayStd.h"
#include "CloudCommEngine/IMSTransport/IMSTransportExport.h"
#include "CloudCommEngine/IMSTransport/IMSTransportConstants.h"
#include "Common/CommonBusListener.h"

#include "ProximalCommEngine/ProximalCommEngineBusObject.h"
#include "CloudCommEngine/CloudCommEngineBusObject.h"

#define QCC_MODULE "SIPE2E"

using namespace std;
using namespace qcc;
using namespace ajn;

using namespace sipe2e;
using namespace gateway;

extern ThreadReturn STDCALL ProximalCommEngineThreadFunc(void* arg);
extern ThreadReturn STDCALL CloudCommEngineThreadFunc(void* arg);

static BusAttachment* s_Bus = NULL;
static CommonBusListener* s_BusListener = NULL;
static ProximalCommEngineBusObject* s_pceBusObject = NULL;
static CloudCommEngineBusObject* s_cceBusObject = NULL;

static volatile sig_atomic_t s_pceInterrupt = false;

static volatile sig_atomic_t s_pceRestart = false;

static void SigIntHandler(int sig)
{
    s_pceInterrupt = true;
}

static void daemonDisconnectCB()
{
    s_pceRestart = true;
}

int main(int argc, char** argv, char** envArg)
{
    QStatus status = ER_OK;

    Thread cloudCommEngineThread("CloudCommEngineThread", CloudCommEngineThreadFunc);
    cloudCommEngineThread.Start();

    Thread proximalCommEngineThread("ProximalCommEngineThread", ProximalCommEngineThreadFunc);
    proximalCommEngineThread.Start();

    // Wait until the gateway client is successfully registered
    while (ITGetStatus() == sipe2e::gateway::gwConsts::IMS_TRANSPORT_STATUS_UNREGISTERED) {
        qcc::Sleep(100);
    }

    // For demo purpose, 
    ITSubscribe("sip:renwei@nane.cn");

    proximalCommEngineThread.Join();
    cloudCommEngineThread.Join();


/*
    signal(SIGINT, SigIntHandler);

    s_Bus = new BusAttachment("SIPE2E Connector", true, 8);
    if (!s_Bus) {
        status = ER_OUT_OF_MEMORY;
        return status;
    }
    status = s_Bus->Start();
    if (ER_OK != status) {
        delete s_Bus;
        return status;
    }

    // Prepare the BusListener
    s_BusListener = new CommonBusListener(s_Bus, daemonDisconnectCB);
    s_Bus->RegisterBusListener(*s_BusListener);

    status = s_Bus->Connect();
    if (ER_OK != status) {
        delete s_Bus;
        return status;
    }

    // Prepare About
*/

    return 0;
}