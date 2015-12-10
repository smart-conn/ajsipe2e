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
#ifndef PROXIMALPROXYBUSOBJECTWRAPPER_H_
#define PROXIMALPROXYBUSOBJECTWRAPPER_H_

#include <qcc/platform.h>

#include <vector>

#include <alljoyn/InterfaceDescription.h>
#include <alljoyn/MessageReceiver.h>
#include <alljoyn/ProxyBusObject.h>

namespace sipe2e {
namespace gateway {

class ProximalProxyBusObjectWrapper;
typedef qcc::ManagedObj<ProximalProxyBusObjectWrapper> _ProximalProxyBusObjectWrapper;

class ProximalProxyBusObjectWrapper : public ajn::MessageReceiver
{
    friend class ProximalCommEngineBusObject;
public:
    ProximalProxyBusObjectWrapper(ajn::_ProxyBusObject _proxy, ajn::BusAttachment* bus, qcc::ManagedObj<ajn::ProxyBusObject> cloudEnginePBO, ProximalCommEngineBusObject* owner);
    virtual ~ProximalProxyBusObjectWrapper();

    QStatus IntrospectProxyChildren();
    qcc::String GenerateProxyIntrospectionXml();

    /**
     * AnnounceHandler is a callback registered to receive AllJoyn Signal.
     * @param[in] member
     * @param[in] srcPath
     * @param[in] message
     */
    void CommonSignalHandler(const ajn::InterfaceDescription::Member* member, const char* srcPath, ajn::Message& msg);

private:
    ajn::_ProxyBusObject proxy;
    ajn::BusAttachment* proxyBus;
    std::vector<_ProximalProxyBusObjectWrapper> children;
    qcc::ManagedObj<ajn::ProxyBusObject> cloudEngineProxyBusObject;
    ProximalCommEngineBusObject* ownerBusObject;
};

}
}

#endif // PROXIMALPROXYBUSOBJECTWRAPPER_H_