/******************************************************************************
 * Copyright (c) 2014-2015, AllSeen Alliance. All rights reserved.
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

#ifndef SIPCOMMON_H__
#define SIPCOMMON_H__

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cassert>

#include <vector>
#include <list>
#include <map>
#include <string>
#include <memory>

using std::vector;
using std::list;
using std::map;
using std::string;
using std::make_shared;
using std::shared_ptr;


#define BUFFER_SIZE_A 64
#define BUFFER_SIZE_B 256
#define BUFFER_SIZE_C 1024
#define BUFFER_SIZE_D 4096

#define sipe2e_OK 0
#define sipe2e_ERR (-1)
#define sipe2e_log printf


#if defined(_WIN32) && \
	(defined(_MSC_VER) || defined(__BORLANDC__) ||  \
	defined(__CYGWIN__) || defined(__MINGW32__))

#ifdef SOFIA_IMSTRANSPORT_EXPORTS
#define SOFIA_IMS_EXPORT_FUN __declspec(dllexport)
#else
#define SOFIA_IMS_EXPORT_FUN __declspec(dllimport)
#endif

#else

#define SOFIA_IMS_EXPORT_FUN

#endif

#endif