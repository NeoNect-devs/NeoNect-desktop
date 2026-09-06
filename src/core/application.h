// src/core/application.h
#pragma once
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QString>
#include <memory>

class CryptoManager;
class NetworkManager;
class AudioManager;
#include "services/messageservice.h"
#include "storage/imessagerepository.h"

namespace NeoNect {

namespace Storage { class SettingsRepository; }
namespace Crypto { class ICryptoService; }
namespace Transport { class IHttpTransport; }

namespace Core { class NotificationManager; }
namespace Services { class RelayService; }

/**
 * @brief Application Controller / Bootstrap class managing lifecycle,
 * command line arguments, services, and QML engine initialization.
 */
class Application {
public:
    explicit Application(int &argc, char **argv);
    ~Application();

    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    Application(Application &&) = delete;
    Application &operator=(Application &&) = delete;

    int run();

private:
    void setupLogging();
    void parseCommandLine();
    void initializeServices();
    void registerQmlTypes();
    bool loadMainUi();

    std::unique_ptr<QGuiApplication> m_app;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    
    // Core Infrastructure
    std::shared_ptr<Storage::SettingsRepository> m_storage;
    std::shared_ptr<Storage::IMessageRepository> m_messageRepo;
    std::shared_ptr<Crypto::ICryptoService> m_cryptoService;
    std::shared_ptr<Transport::IHttpTransport> m_transport;

    // Facades & Managers
    std::unique_ptr<::CryptoManager> m_cryptoManager;
    std::unique_ptr<::NetworkManager> m_networkManager;
    std::unique_ptr<::AudioManager> m_audioManager;
    std::unique_ptr<Core::NotificationManager> m_notificationManager;
    std::unique_ptr<Services::MessageService> m_messageService;
    std::shared_ptr<Services::RelayService> m_relayService;
    
    QString m_profile;
    bool m_isMockMode{false};
};

} // namespace NeoNect
