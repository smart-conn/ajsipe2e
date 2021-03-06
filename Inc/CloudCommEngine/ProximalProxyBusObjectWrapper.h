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
#ifndef PROXIMALPROXYBUSOBJECTWRAPPER_H_
#define PROXIMALPROXYBUSOBJECTWRAPPER_H_

#include <qcc/platform.h>

#include <vector>

#include <alljoyn/InterfaceDescription.h>
#include <alljoyn/MessageReceiver.h>
#include <alljoyn/ProxyBusObject.h>

namespace sipe2e {
namespace gateway {

// BUGBUG: "friend declaration" is weak and it doesn't provide the solid forward declaration for further usage,
// so we have to forward declare CloudCommEngineBusObject here.
class CloudCommEngineBusObject;

class ProximalProxyBusObjectWrapper;
typedef qcc::ManagedObj<ProximalProxyBusObjectWrapper> _ProximalProxyBusObjectWrapper;

class ProximalProxyBusObjectWrapper : public ajn::MessageReceiver {
    friend class CloudCommEngineBusObject;

  public:
    ProximalProxyBusObjectWrapper();
    ProximalProxyBusObjectWrapper(ajn::_ProxyBusObject _proxy, ajn::BusAttachment* bus, CloudCommEngineBusObject* owner);
    virtual ~ProximalProxyBusObjectWrapper();

    QStatus IntrospectProxyChildren();
    qcc::String GenerateProxyIntrospectionXml(ajn::SessionPort& wellKnownPort, bool topLevel);

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
    CloudCommEngineBusObject* ownerBusObject;
};

}
}

#endif // PROXIMALPROXYBUSOBJECTWRAPPER_H_
