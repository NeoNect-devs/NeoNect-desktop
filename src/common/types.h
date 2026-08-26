// src/common/types.h
#pragma once
#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <optional>
#include <functional>

namespace Avila {

enum class ConnectionStatus {
    Disconnected,
    Connecting,
    Connected,
    Error
};

enum class PresenceStatus {
    Offline,
    Online
};

struct UserProfile {
    QString username;
    bool isValid{false};
};

struct DeviceCredentials {
    QString deviceId;
    QString publicKeyBase64;
};

struct RelayMessagePacket {
    qint64 id{0};
    QString sender;
    QString target;
    QString content;
    qint64 timestamp{0};
    bool isFromMe{false};
};

struct FriendEntry {
    QString username;
    PresenceStatus status{PresenceStatus::Offline};
    QDateTime lastSeen;
};

struct EncryptedPayload {
    QByteArray cipherWithTag; // Ciphertext + 16 byte authentication tag
    QByteArray nonce;         // 12 byte GCM IV
    bool success{false};
    QString errorMessage;
};

template <typename T>
struct ServiceResult {
    bool success{false};
    QString message;
    std::optional<T> data{std::nullopt};

    static ServiceResult<T> ok(T data, const QString &msg = QString()) {
        return {true, msg, std::make_optional(std::move(data))};
    }

    static ServiceResult<T> fail(const QString &msg) {
        return {false, msg, std::nullopt};
    }
};

using VoidResult = ServiceResult<std::monostate>;

} // namespace Avila
