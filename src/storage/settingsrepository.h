// src/storage/settingsrepository.h
#pragma once
#include "isettingsrepository.h"
#include <QSettings>
#include <memory>
#include <mutex>

namespace Avila {
namespace Storage {

class SettingsRepository : public ISettingsRepository {
public:
    explicit SettingsRepository(const QString &profileName = QString());
    ~SettingsRepository() override = default;

    void setProfile(const QString &profileName) override;
    QString profile() const override;

    QString serverUrl() const override;
    void setServerUrl(const QString &url) override;

    QString authToken() const override;
    void setAuthToken(const QString &token) override;

    QString username() const override;
    void setUsername(const QString &username) override;

    QString deviceId() const override;
    void setDeviceId(const QString &id) override;

    QString publicKey() const override;
    void setPublicKey(const QString &key) override;

    QStringList friends() const override;
    void setFriends(const QStringList &friends) override;

    void clearSession() override;

private:
    QString getGroupName() const;

    mutable std::mutex m_mutex;
    QString m_profile;
};

} // namespace Storage
} // namespace Avila
