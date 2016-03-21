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

#ifndef GATEWAYCONSTANTS_H_
#define GATEWAYCONSTANTS_H_

#include <qcc/String.h>

namespace sipe2e {
namespace gateway {
namespace gwConsts {

static const uint16_t DATE_PROPERTY_TYPE = 0;
static const uint16_t TIME_PROPERTY_TYPE = 1;

static const uint16_t ANNOUNCMENT_PORT_NUMBER = 900;

static const qcc::String SIPE2E_GATEWAY_OBJECTPATH_PREFIX = "/SIPE2E/Gateway";

/*
   static const uint16_t SIPE2E_PROXIMALCOMMENGINE_SESSION_PORT = 5060;
   static const qcc::String SIPE2E_PROXIMALCOMMENGINE_NAME = "ProximalCommEngine";
   static const qcc::String SIPE2E_PROXIMALCOMMENGINE_OBJECTPATH = SIPE2E_GATEWAY_OBJECTPATH_PREFIX + "/" + SIPE2E_PROXIMALCOMMENGINE_NAME;
   static const qcc::String SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_INTERFACE = "cn.nane.SIPE2E.Gateway.ProximalCommEngine.AllJoynEngine";
   static const qcc::String SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_LOCALMETHODCALL = "AJLocalMethodCall";
   static const qcc::String SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_LOCALSIGNALCALL = "AJLocalSignalCall";
   static const qcc::String SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_SUBSCRIBE = "AJSubscribeCloudServiceToLocal";
   static const qcc::String SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_UNSUBSCRIBE = "AJUnsubscribeCloudServiceFromLocal";
   static const qcc::String SIPE2E_PROXIMALCOMMENGINE_ALLJOYNENGINE_UPDATESIGNALHANDLERINFO = "AJUpdateSignalHandlerInfoToLocal";
 */

static const uint16_t SIPE2E_CLOUDCOMMENGINE_SESSION_PORT = 5060;
static const qcc::String SIPE2E_CLOUDCOMMENGINE_NAME = "CloudCommEngine";
static const qcc::String SIPE2E_CLOUDCOMMENGINE_OBJECTPATH = SIPE2E_GATEWAY_OBJECTPATH_PREFIX + "/" + SIPE2E_CLOUDCOMMENGINE_NAME;
static const qcc::String SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_INTERFACE = "cn.nane.SIPE2E.Gateway.CloudCommEngine.AllJoynEngine";
/*
   static const qcc::String SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_CLOUDMETHODCALL = "AJCloudMethodCall";
   static const qcc::String SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_CLOUDSIGNALCALL = "AJCloudSignalCall";
   static const qcc::String SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_PUBLISH = "AJPublishLocalServiceToCloud";
   static const qcc::String SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_DELETE = "AJDeleteLocalServiceFromCloud";
   static const qcc::String SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_UPDATESIGNALHANDLERINFO = "AJUpdateSignalHandlerInfoToCloud";
 */
static const qcc::String SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_SUBSCRIBE = "AJSubscribe";
static const qcc::String SIPE2E_CLOUDCOMMENGINE_ALLJOYNENGINE_UNSUBSCRIBE = "AJUnsubscribe";

static const qcc::String SIPE2E_CLOUDSERVICEAGENT_ALLJOYNENGINE_INTERFACE = "cn.nane.SIPE2E.Gateway.CloudServiceAgent.AllJoynEngine";


static const uint32_t CLOUD_METHOD_CALL_THREAD_POOL_SIZE = 1024;

static const uint32_t LOCAL_SERVICE_ANNOUNCE_HANDLER_TASK_POOL_SIZE = 32;
static const uint32_t PING_BUS_TIMEOUT = 5000; // in ms

static const qcc::String AJPARAM_EMPTY = "";
static const qcc::String AJPARAM_VAR = "v";
static const qcc::String AJPARAM_STR = "s";
static const qcc::String AJPARAM_BOOL = "b";
static const qcc::String AJPARAM_UINT16 = "q";
static const qcc::String AJPARAM_INT16 = "n";
static const qcc::String AJPARAM_UINT32 = "u";
static const qcc::String AJPARAM_INT32 = "i";
static const qcc::String AJPARAM_UINT64 = "t";
static const qcc::String AJPARAM_INT64 = "x";
static const qcc::String AJPARAM_DOUBLE = "d";
static const qcc::String AJPARAM_DICT_UINT16_VAR = "{qv}";
static const qcc::String AJPARAM_DICT_STR_VAR = "{sv}";
static const qcc::String AJPARAM_ARRAY_DICT_UINT16_VAR = "a{qv}";
static const qcc::String AJPARAM_ARRAY_DICT_STR_VAR = "a{sv}";
static const qcc::String AJPARAM_ARRAY_UINT16 = "aq";
static const qcc::String AJPARAM_STRUCT_VAR_STR = "(vs)";
static const qcc::String AJPARAM_ARRAY_STRUCT_VAR_STR = "a(vs)";
static const qcc::String AJPARAM_STRUCT_VAR_VAR_VAR = "(vvv)";
static const qcc::String AJPARAM_DATE_OR_TIME = "(q(qqq))";

static const qcc::String AJ_ERROR_UNKNOWN = "Unknown Error";
static const qcc::String AJ_ERROR_UNKNOWN_MESSAGE = "An Unknown Error occured";

} //namespace gwConsts
} //namespace gateway
} //namespace sipe2e

#endif


