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

#include "Common/CommonBusListener.h"

#define QCC_MODULE "SIPE2E"

namespace sipe2e {

namespace gateway {

CommonBusListener::CommonBusListener(ajn::BusAttachment* bus /*= NULL*/, void(*daemonDisconnectCB)() /*= NULL*/)
	: BusListener(), SessionPortListener(), m_SessionPort(0), m_Bus(bus), m_DaemonDisconnectCB(daemonDisconnectCB)
{

}

CommonBusListener::~CommonBusListener()
{

}

void CommonBusListener::setSessionPort(ajn::SessionPort sessionPort)
{
	m_SessionPort = sessionPort;
}

ajn::SessionPort CommonBusListener::getSessionPort()
{
	return m_SessionPort;
}

bool CommonBusListener::AcceptSessionJoiner(ajn::SessionPort sessionPort, const char* joiner, const ajn::SessionOpts& opts)
{
	if (sessionPort != m_SessionPort) {
		return false;
	}

	std::cout << "Accepting JoinSessionRequest" << std::endl;
	return true;
}

void CommonBusListener::SessionJoined(ajn::SessionPort sessionPort, ajn::SessionId id, const char* joiner)
{
	std::cout << "Session has been joined successfully" << std::endl;
	if (m_Bus) {
		m_Bus->SetSessionListener(id, this);
	}
	m_SessionIds.push_back(id);
}

void CommonBusListener::SessionLost(ajn::SessionId sessionId, SessionLostReason reason)
{
	std::cout << "Session has been lost" << std::endl;
	std::vector<ajn::SessionId>::iterator it = std::find(m_SessionIds.begin(), m_SessionIds.end(), sessionId);
	if (it != m_SessionIds.end()) {
		m_SessionIds.erase(it);
	}
}

const std::vector<ajn::SessionId>& CommonBusListener::getSessionIds() const
{
	return m_SessionIds;
}

void CommonBusListener::BusDisconnected()
{
	std::cout << "Bus has been disconnected" << std::endl;
	if (m_DaemonDisconnectCB) {
		m_DaemonDisconnectCB();
	}
}


} // namespace gateway

} // namespace sipe2e