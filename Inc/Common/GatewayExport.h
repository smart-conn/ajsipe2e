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

#ifndef GATEWAYEXPORT_H_
#define GATEWAYEXPORT_H_

#ifdef __cplusplus
extern "C"
{
#endif

    /**
      *   Exporting
      */
#if defined(WIN32) && !defined(SIPE2E_GATEWAY_STATIC)
#define SIPE2E_GATEWAY_EXPORT __declspec(dllexport)
#else
#define SIPE2E_GATEWAY_EXPORT
#endif

    /**
      *   Importing
      */
    #if defined(WIN32)
#define SIPE2E_GATEWAY_IMPORT __declspec(dllimport)
#else
#define SIPE2E_GATEWAY_IMPORT
#endif

    /**
      *   Calling Conventions
      */
#if defined(__GNUC__)
#if defined(__i386)
#define SIPE2E_GATEWAY_CALL __attribute__((cdecl))
#define SIPE2E_GATEWAY_WUR __attribute__((warn_unused_result))
#else
#define SIPE2E_GATEWAY_CALL
#define SIPE2E_GATEWAY_WUR
#endif

#else

#if defined(__unix)
#define SIPE2E_GATEWAY_CALL
#define SIPE2E_GATEWAY_WUR
#else                           /* WIN32 */
#define SIPE2E_GATEWAY_CALL __stdcall
#define SIPE2E_GATEWAY_WUR
#endif

#endif



#if defined(SIPE2E_GATEWAY_EXPORTS)
#define SIPE2E_GATEWAY_EXTERN                    SIPE2E_GATEWAY_EXPORT
#define SIPE2E_GATEWAY_EXTERN_NONSTD             SIPE2E_GATEWAY_EXPORT
#define SIPE2E_GATEWAY_DECLARE_DATA
#else
#define SIPE2E_GATEWAY_EXTERN                    SIPE2E_GATEWAY_IMPORT
#define SIPE2E_GATEWAY_EXTERN_NONSTD             SIPE2E_GATEWAY_IMPORT
#define SIPE2E_GATEWAY_DECLARE_DATA
#endif


#ifdef __cplusplus
}
#endif

#endif // GATEWAYEXPORT_H_
