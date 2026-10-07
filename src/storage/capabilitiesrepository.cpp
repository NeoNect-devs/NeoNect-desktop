#include "capabilitiesrepository.h"

namespace NeoNect {
namespace Storage {

std::optional<qint64> CapabilitiesRepository::maxHttpBodyBytes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxHttpBodyBytes;
}

std::optional<int> CapabilitiesRepository::maxEnvelopeBytes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxEnvelopeBytes;
}

std::optional<int> CapabilitiesRepository::maxDevicesPerUser() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxDevicesPerUser;
}

CapabilityState CapabilitiesRepository::capabilityState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

void CapabilitiesRepository::replace(
        std::optional<qint64> httpBody,
        std::optional<int> envelope,
        std::optional<int> devices,
        CapabilityState state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxHttpBodyBytes = httpBody;
    m_maxEnvelopeBytes = envelope;
    m_maxDevicesPerUser = devices;
    m_state = state;
}

} // namespace Storage
} // namespace NeoNect
