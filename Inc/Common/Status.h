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
#ifndef STATUS_H_
#define STATUS_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Enumerated list of values QStatus can return
 */
typedef enum {
    IC_OK = 0x0 /**< Success. */,
    IC_FAIL = 0x1 /**< Generic failure. */,
    IC_SYSTEM = 0x2,
    IC_OUT_OF_MEMORY = 0x3,
    IC_TIMEOUT = 0x4,
    IC_INIT_FAILED = 0x5,
    IC_BAD_ARG_1 = 0xa,
    IC_BAD_ARG_2 = 0xb,
    IC_BAD_ARG_3 = 0xc,
    IC_BAD_ARG_4 = 0xd,
    IC_BAD_ARG_5 = 0xe,
    IC_BAD_ARG_6 = 0xf,
    IC_BAD_ARG_7 = 0x10,
    IC_BAD_ARG_8 = 0x11,
    IC_INVALID_ADDRESS = 0x12,
    IC_INVALID_DATA = 0x13,
    IC_OPEN_FAILED = 0x14,
    IC_READ_ERROR = 0x15,
    IC_WRITE_ERROR = 0x16,
    IC_CONF_FILE_NOT_FOUND = 0x17,
    IC_CONF_XML_PARSING = 0x18,
    IC_INTROSPECTION_XML_PARSING = 0x19,
    IC_METHOD_NOT_FOUND = 0x1A,
    IC_TRANSPORT_FAIL = 0x60,
    IC_TRANSPORT_IMS_STACK_INIT_FAIL = 0x61,
    IC_TRANSPORT_IMS_STACK_START_FAIL = 0x62,
    IC_TRANSPORT_IMS_PUB_FAILED = 0x63,
    IC_TRANSPORT_IMS_SUB_FAILED = 0x64
} IStatus;

#ifdef __cplusplus
}
#endif

#endif // STATUS_H_