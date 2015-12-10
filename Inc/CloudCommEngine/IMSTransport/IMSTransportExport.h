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

#ifndef IMSTRANSPORTEXPORT_H_
#define IMSTRANSPORTEXPORT_H_

#include <Common/GatewayExport.h>

/*
#ifdef SIPE2E_GATEWAY_CLOUDCOMMENGINE
#include <boost/shared_array.hpp>
#endif
*/

#ifdef __cplusplus
extern "C"
{
#endif
    SIPE2E_GATEWAY_EXTERN int SIPE2E_GATEWAY_CALL
        ITInitialize();
    SIPE2E_GATEWAY_EXTERN int SIPE2E_GATEWAY_CALL
        ITShutdown();

    SIPE2E_GATEWAY_EXTERN int SIPE2E_GATEWAY_CALL
        ITGetStatus();

    SIPE2E_GATEWAY_EXTERN int SIPE2E_GATEWAY_CALL
        ITSubscribe(const char* remoteAccount);

    SIPE2E_GATEWAY_EXTERN int SIPE2E_GATEWAY_CALL
        ITUnsubscribe(const char* remoteAccount);

    SIPE2E_GATEWAY_EXTERN int SIPE2E_GATEWAY_CALL
        ITPublishService(const char* introspectionXml);

    SIPE2E_GATEWAY_EXTERN int SIPE2E_GATEWAY_CALL
        ITDeleteService(const char* introspectionXml);

    SIPE2E_GATEWAY_EXTERN int SIPE2E_GATEWAY_CALL
        ITReadCloudMessage(char** msgBuf);

    SIPE2E_GATEWAY_EXTERN int SIPE2E_GATEWAY_CALL
        ITSendCloudMessage(int reqType,
            const char* peer,
            const char* callId,
            const char* addr,
            const char* msgBuf,
            char** resMsgBuf);

    SIPE2E_GATEWAY_EXTERN void SIPE2E_GATEWAY_CALL
        ITReleaseBuf(char* buf);
#ifdef _DEBUG
    SIPE2E_GATEWAY_EXTERN const char* SIPE2E_GATEWAY_CALL
        ITTestServiceSerialize(char* buf);
#endif
#ifdef __cplusplus
}
#endif

#endif // IMSTRANSPORTEXPORT_H_