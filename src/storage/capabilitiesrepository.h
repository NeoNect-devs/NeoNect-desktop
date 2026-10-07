#pragma once
#include "icapabilitiesrepository.h"
#include <mutex>

namespace NeoNect {
namespace Storage {

class CapabilitiesRepository : public ICapabilitiesRepository {
public:
    CapabilitiesRepository() = default;
    ~CapabilitiesRepository() override = default;

    std::optional<qint64> maxHttpBodyBytes() const override;
    std::optional<int> maxEnvelopeBytes() const override;
    std::optional<int> maxDevicesPerUser() const override;
    CapabilityState capabilityState() const override;

    void replace(
        std::optional<qint64> httpBody,
        std::optional<int> envelope,
        std::optional<int> devices,
        CapabilityState state) override;

private:
    mutable std::mutex m_mutex;
    std::optional<qint64> m_maxHttpBodyBytes;
    std::optional<int> m_maxEnvelopeBytes;
    std::optional<int> m_maxDevicesPerUser;
    CapabilityState m_state{CapabilityState::UNKNOWN};
};

} // namespace Storage
} // namespace NeoNect
