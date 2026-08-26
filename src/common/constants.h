// src/common/constants.h
#pragma once
#include <QString>
#include <cstddef>

namespace Avila {
namespace Constants {

    // Network Endpoints
    inline constexpr const char* DEFAULT_SERVER_URL = "http://localhost:8090";
    inline constexpr const char* EP_HEALTH = "/api/v1/health";
    inline constexpr const char* EP_HEALTH_FALLBACK = "/health";
    inline constexpr const char* EP_AUTH = "/api/v1/auth";
    inline constexpr const char* EP_USERS = "/api/v1/users";
    inline constexpr const char* EP_USERS_AVAILABILITY = "/api/v1/users/availability";
    inline constexpr const char* EP_USERS_ME = "/api/v1/users/me";
    inline constexpr const char* EP_DEVICE_REGISTER = "/api/v1/device/register";
    inline constexpr const char* EP_DEVICE_KEY = "/api/v1/device/key";
    inline constexpr const char* EP_RELAY_SEND = "/api/v1/relay/send";
    inline constexpr const char* EP_RELAY_POLL = "/api/v1/relay/poll";
    inline constexpr const char* EP_RELAY_ACK = "/api/v1/relay/ack";

    // Cryptographic Parameters
    inline constexpr std::size_t AES_256_KEY_SIZE = 32;
    inline constexpr std::size_t AES_GCM_IV_SIZE = 12;
    inline constexpr std::size_t AES_GCM_TAG_SIZE = 16;
    inline constexpr int PBKDF2_ITERATIONS = 10000;
    inline constexpr const char* STATIC_SALT_VAULT = "AVILA_STATIC_NETWORK_SALT_VAULT";

    // Timers & Intervals
    inline constexpr int RELAY_POLL_INTERVAL_MS = 2500;
    inline constexpr qint64 PRESENCE_TIMEOUT_SECS = 30;

    // Storage Keys
    inline constexpr const char* SETTINGS_ROOT_GROUP = "Avila";
    inline constexpr const char* DEFAULT_PROFILE_GROUP = "DesktopClient";
    inline constexpr const char* KEY_SERVER_URL = "server_url";
    inline constexpr const char* KEY_AUTH_TOKEN = "auth_token";
    inline constexpr const char* KEY_USERNAME = "username";
    inline constexpr const char* KEY_DEVICE_ID = "device_id";
    inline constexpr const char* KEY_PUBLIC_KEY = "public_key";
    inline constexpr const char* KEY_FRIENDS = "friends";

} // namespace Constants
} // namespace Avila
