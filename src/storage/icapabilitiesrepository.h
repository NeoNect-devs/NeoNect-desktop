#pragma once
#include <QtGlobal>
#include <optional>

namespace NeoNect {
namespace Storage {

enum class CapabilityState {
    VALID,
    UNKNOWN,
    UNAVAILABLE,
    UNSUPPORTED,
    INVALID
};

class ICapabilitiesRepository {
public:
    virtual ~ICapabilitiesRepository() = default;

    virtual std::optional<qint64> maxHttpBodyBytes() const = 0;
    virtual std::optional<int> maxEnvelopeBytes() const = 0;
    virtual std::optional<int> maxDevicesPerUser() const = 0;
    virtual CapabilityState capabilityState() const = 0;

    virtual void replace(
        std::optional<qint64> httpBody,
        std::optional<int> envelope,
        std::optional<int> devices,
        CapabilityState state) = 0;
};

} // namespace Storage
} // namespace NeoNect
