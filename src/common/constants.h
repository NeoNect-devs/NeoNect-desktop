/**
 * @file constants.h
 * @brief Global architectural constants, REST API routes, cryptographic parameters, and storage keys.
 * @details Central source of truth for protocol specifications, API v1 contracts, and client-side policies.
 * 
 * @author NeoNect Development Team
 * @version 1.0.0
 */

#pragma once
#include <QString>
#include <cstddef>

namespace NeoNect {

/**
 * @namespace NeoNect::Constants
 * @brief Global architectural constants, REST API routes, cryptographic parameters, and storage keys.
 * @details Central source of truth for protocol specifications, API v1 contracts, and client-side policies.
 */
namespace Constants {

    // =========================================================================
    // Network Endpoints (NeoNect API v1 Contract)
    // =========================================================================

    /** @brief Default fallback server URI utilized when unconfigured. */
    inline constexpr const char* DEFAULT_SERVER_URL = "http://localhost:8080";

    /** @brief Health check and system liveness probe (`GET`). */
    inline constexpr const char* EP_HEALTH = "/api/v1/health";

    /** @brief Real-time presence query and beacon endpoint (`GET`). */
    inline constexpr const char* EP_PRESENCE = "/api/v1/presence";

    /** @brief Security verification and TLS identity probe (`GET`). */
    inline constexpr const char* EP_SECURITY_VERIFY = "/api/v1/security/verify";

    /** @brief User authentication endpoint (`POST` for login, `DELETE` for session invalidation). */
    inline constexpr const char* EP_AUTH = "/api/v1/auth";

    /** @brief User registration endpoint (`POST`). */
    inline constexpr const char* EP_USERS = "/api/v1/users";

    /** @brief Username availability check (`GET ?u=<username>`). */
    inline constexpr const char* EP_USERS_AVAILABILITY = "/api/v1/users/availability";

    /** @brief Current authenticated user profile lookup (`GET`). */
    inline constexpr const char* EP_USERS_ME = "/api/v1/users/me";

    /** @brief Device registration and public key enrollment (`POST`). */
    inline constexpr const char* EP_DEVICE_REGISTER = "/api/v1/device/register";

    /** @brief Query public key for a specific device (`GET ?device_id=...`). */
    inline constexpr const char* EP_DEVICE_KEY = "/api/v1/device/key";

    /** @brief Device inventory management (`DELETE` for device revocation). */
    inline constexpr const char* EP_DEVICE = "/api/v1/device";

    /** @brief Retrieve prekeys and active devices for a target user (`GET ?u=...`). */
    inline constexpr const char* EP_RELAY_KEYS = "/api/v1/relay/keys";

    /** @brief Transmit an encrypted message packet through the server relay (`POST`). */
    inline constexpr const char* EP_RELAY_SEND = "/api/v1/relay/send";

    /** @brief HTTP long-polling mailbox fallback when WebSocket is unavailable (`GET ?device_id=...`). */
    inline constexpr const char* EP_RELAY_POLL = "/api/v1/relay/poll";

    /** @brief Acknowledge receipt and purge delivered relay message (`POST`). */
    inline constexpr const char* EP_RELAY_ACK = "/api/v1/relay/ack";

    /** @brief Full-duplex WebSocket real-time relay stream (`GET / WebSocket Upgrade`). */
    inline constexpr const char* EP_RELAY_WS = "/api/v1/relay/ws";

    /** @brief Authoritative mutual friendship management (`GET`, `POST`, `DELETE`). */
    inline constexpr const char* EP_FRIENDS = "/api/v1/friends";

    // =========================================================================
    // NeoNect Validation & Policy Constraints
    // =========================================================================

    /** @brief Minimum allowed character length for user account handles. */
    inline constexpr int MIN_USERNAME_LENGTH = 3;

    /** @brief Minimum allowed character length for authentication passwords. */
    inline constexpr int MIN_PASSWORD_LENGTH = 8;

    /** @brief Standard server session expiry window in hours. */
    inline constexpr int SESSION_DURATION_HOURS = 24;

    /** @brief Standard HTTP cookie name utilized for session token transmission. */
    inline constexpr const char* SESSION_COOKIE_NAME = "neonect_sid";

    // =========================================================================
    // Cryptographic Parameters (AES-256-GCM / PBKDF2)
    // =========================================================================

    /** @brief AES-256 key size in bytes (256 bits). */
    inline constexpr std::size_t AES_256_KEY_SIZE = 32;

    /** @brief Standard AES-GCM Initialization Vector (nonce) length in bytes (96 bits). */
    inline constexpr std::size_t AES_GCM_IV_SIZE = 12;

    /** @brief Standard AES-GCM authentication tag length in bytes (128 bits). */
    inline constexpr std::size_t AES_GCM_TAG_SIZE = 16;

    /** @brief Iteration count for PBKDF2 key derivation from user passwords. */
    inline constexpr int PBKDF2_ITERATIONS = 10000;

    /** @brief Application-level salt string for local key derivation. */
    inline constexpr const char* STATIC_SALT_VAULT = "NEONECT_STATIC_NETWORK_SALT_VAULT";

    // =========================================================================
    // Timers, Intervals & Watchdogs
    // =========================================================================

    /** @brief Polling frequency in milliseconds for the HTTP relay polling fallback. */
    inline constexpr int RELAY_POLL_INTERVAL_MS = 2500;

    /** @brief Threshold in seconds after which an unresponsive peer is flagged offline. */
    inline constexpr qint64 PRESENCE_TIMEOUT_SECS = 30;

    // =========================================================================
    // Storage Keys (QSettings & Registry Groups)
    // =========================================================================

    /** @brief Root organization group name for platform settings. */
    inline constexpr const char* SETTINGS_ROOT_GROUP = "NeoNect";

    /** @brief Default profile group identifier when multi-profile flag is omitted. */
    inline constexpr const char* DEFAULT_PROFILE_GROUP = "DesktopClient";

    /** @brief Storage key for configured remote server URL. */
    inline constexpr const char* KEY_SERVER_URL = "server_url";

    /** @brief Storage key for encrypted authentication session token. */
    inline constexpr const char* KEY_AUTH_TOKEN = "auth_token";

    /** @brief Storage key for the active account username. */
    inline constexpr const char* KEY_USERNAME = "username";

    /** @brief Storage key for device UUID identifier. */
    inline constexpr const char* KEY_DEVICE_ID = "device_id";

    /** @brief Storage key for device cryptographic public key. */
    inline constexpr const char* KEY_PUBLIC_KEY = "public_key";

    /** @brief Base storage key for cached friend usernames. */
    inline constexpr const char* KEY_FRIENDS = "friends";

    /** @brief Base storage key for pending friend request usernames. */
    inline constexpr const char* KEY_PENDING_REQUESTS = "pending_requests";

    /** @brief Storage key for user server bookmarks. */
    inline constexpr const char* KEY_BOOKMARKS = "bookmarks";

    /** @brief Base storage key for active open conversations list. */
    inline constexpr const char* KEY_OPEN_CONVERSATIONS = "open_conversations";

    /** @brief Base storage key for customized user display name. */
    inline constexpr const char* KEY_DISPLAY_NAME = "display_name";

    /** @brief Base storage key for peer display name overrides dictionary. */
    inline constexpr const char* KEY_PEER_DISPLAY_NAMES = "peer_display_names";

    /** @brief Base storage key for current user avatar URI. */
    inline constexpr const char* KEY_AVATAR_URL = "avatar_url";

    /** @brief Base storage key for peer avatar URLs dictionary. */
    inline constexpr const char* KEY_PEER_AVATARS = "peer_avatars";

} // namespace Constants
} // namespace NeoNect
