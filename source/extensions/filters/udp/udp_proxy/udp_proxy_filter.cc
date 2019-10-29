#include "extensions/filters/udp/udp_proxy/udp_proxy_filter.h"

#include "envoy/network/listener.h"

namespace Envoy {
namespace Extensions {
namespace UdpFilters {
namespace UdpProxy {

void UdpProxyFilter::onData(Network::UdpRecvData& data) {
  // fixfix peer and local address?
  const auto active_session_it = sessions_.find(*data.peer_address_);
  ActiveSession* active_session;
  if (active_session_it == sessions_.end()) {
    // fixfix keep track of cluster.
    Upstream::ThreadLocalCluster* cluster = config_->getCluster();
    if (cluster == nullptr) {
      ASSERT(false); // fixfix
    }

    // fixfix pass context
    Upstream::HostConstSharedPtr host = cluster->loadBalancer().chooseHost(nullptr);
    if (host == nullptr) {
      ASSERT(false); // fixfix
    }

    auto new_session = std::make_unique<ActiveSession>(*this, std::move(data.peer_address_), host);
    active_session = new_session.get();
    sessions_.emplace(std::move(new_session));
  } else {
    active_session = active_session_it->get();
  }
}

UdpProxyFilter::ActiveSession::ActiveSession(
    UdpProxyFilter& parent, Network::Address::InstanceConstSharedPtr&& source_address,
    const Upstream::HostConstSharedPtr& host)
    : parent_(parent), source_address_(std::move(source_address)),
      idle_timer_(parent.read_callbacks_->udpListener().dispatcher().createTimer(
          [this] { onIdleTimer(); })),
      io_handle_(host->address()->socket(Network::Address::SocketType::Datagram)),
      socket_event_(parent.read_callbacks_->udpListener().dispatcher().createFileEvent(
          io_handle_->fd(), [this](uint32_t) { onReadReady(); }, Event::FileTriggerType::Edge,
          Event::FileReadyType::Read)) {}

} // namespace UdpProxy
} // namespace UdpFilters
} // namespace Extensions
} // namespace Envoy
