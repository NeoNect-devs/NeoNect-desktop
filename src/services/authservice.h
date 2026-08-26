// src/services/authservice.h
#pragma once
#include <QObject>
#include <memory>
#include "../transport/ihttptransport.h"
#include "../storage/isettingsrepository.h"

namespace Avila {
namespace Services {

class AuthService : public QObject {
    Q_OBJECT
public:
    explicit AuthService(std::shared_ptr<Transport::IHttpTransport> transport,
                         std::shared_ptr<Storage::ISettingsRepository> storage,
                         QObject *parent = nullptr);
    ~AuthService() override = default;

    void verifyServer(const QString &address);
    void checkUsernameAvailability(const QString &username);
    void registerUser(const QString &username, const QString &password);
    void loginUser(const QString &username, const QString &password);
    void logoutUser();
    void fetchUserProfile();

signals:
    void verificationResult(bool success, const QString &message);
    void availabilityResult(const QString &username, bool available, const QString &error);
    void registrationResult(bool success, const QString &message);
    void loginResult(bool success, const QString &tokenOrError);
    void userProfileFetched(bool success, const QString &username);
    void authSessionExpired();

private:
    std::shared_ptr<Transport::IHttpTransport> m_transport;
    std::shared_ptr<Storage::ISettingsRepository> m_storage;
};

} // namespace Services
} // namespace Avila
