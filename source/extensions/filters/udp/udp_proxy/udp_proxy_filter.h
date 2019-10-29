#pragma once

#include "envoy/config/filter/udp/udp_proxy/v2alpha/udp_proxy.pb.h"
#include "envoy/event/file_event.h"
#include "envoy/event/timer.h"
#include "envoy/network/filter.h"
#include "envoy/upstream/cluster_manager.h"

#include "absl/container/flat_hash_set.h"

namespace Envoy {
namespace Extensions {
namespace UdpFilters {
namespace UdpProxy {

class UdpProxyFilterConfig {
public:
  UdpProxyFilterConfig(Upstream::ClusterManager& cluster_manager,
                       const envoy::config::filter::udp::udp_proxy::v2alpha::UdpProxyConfig& config)
      : cluster_manager_(cluster_manager), config_(config) {}

  Upstream::ThreadLocalCluster* getCluster() const {
    return cluster_manager_.get(config_.cluster());
  }

private:
  Upstream::ClusterManager& cluster_manager_;
  const envoy::config::filter::udp::udp_proxy::v2alpha::UdpProxyConfig config_;
};

using UdpProxyFilterConfigSharedPtr = std::shared_ptr<const UdpProxyFilterConfig>;

class UdpProxyFilter : public Network::UdpListenerReadFilter, Logger::Loggable<Logger::Id::filter> {
public:
  UdpProxyFilter(Network::UdpReadFilterCallbacks& callbacks,
                 const UdpProxyFilterConfigSharedPtr& config)
      : UdpListenerReadFilter(callbacks), config_(config) {}

  // Network::UdpListenerReadFilter
  void onData(Network::UdpRecvData& data) override;

private:
  class ActiveSession {
  public:
    ActiveSession(UdpProxyFilter& parent, Network::Address::InstanceConstSharedPtr&& source_address,
                  const Upstream::HostConstSharedPtr& host);
    const Network::Address::Instance& sourceAddress() { return *source_address_; }

  private:
    void onIdleTimer();
    void onReadReady();

    UdpProxyFilter& parent_;
    const Network::Address::InstanceConstSharedPtr source_address_;
    const Event::TimerPtr idle_timer_;
    const Network::IoHandlePtr io_handle_;
    const Event::FileEventPtr socket_event_;
  };

  using ActiveSessionPtr = std::unique_ptr<ActiveSession>;

  struct HeterogeneousActiveSessionHash {
    // Specifying is_transparent indicates to the library infrastructure that
    // type-conversions should not be applied when calling find(), but instead
    // pass the actual types of the contained and searched-for objects directly to
    // these functors. See
    // https://en.cppreference.com/w/cpp/utility/functional/less_void for an
    // official reference, and https://abseil.io/tips/144 for a description of
    // using it in the context of absl.
    using is_transparent = void; // NOLINT(readability-identifier-naming)

    size_t operator()(const Network::Address::Instance& value) const {
      return absl::Hash<absl::string_view>()(value.asStringView());
    }
    size_t operator()(const ActiveSessionPtr& value) const {
      return absl::Hash<absl::string_view>()(value->sourceAddress().asStringView());
    }
  };

  struct HeterogeneousActiveSessionEqual {
    // See description for HeterogeneousActiveSessionHash::is_transparent.
    using is_transparent = void; // NOLINT(readability-identifier-naming)

    bool operator()(const ActiveSessionPtr& lhs, const Network::Address::Instance& rhs) const {
      return lhs->sourceAddress() == rhs;
    }
    bool operator()(const ActiveSessionPtr& lhs, const ActiveSessionPtr& rhs) const {
      return lhs->sourceAddress() == rhs->sourceAddress();
    }
  };

  const UdpProxyFilterConfigSharedPtr config_;
  absl::flat_hash_set<ActiveSessionPtr, HeterogeneousActiveSessionHash,
                      HeterogeneousActiveSessionEqual>
      sessions_;
};

} // namespace UdpProxy
} // namespace UdpFilters
} // namespace Extensions
} // namespace Envoy
