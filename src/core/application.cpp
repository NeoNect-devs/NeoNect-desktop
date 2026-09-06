#include <QStandardPaths>
#include <QDir>
// src/core/application.cpp
#include "application.h"
#include "networkmanager.h"
#include "cryptomanager.h"
#include "chatmessagemodel.h"
#include "audiomanager.h"
#include "notificationmanager.h"
#include "../themedata.h"
#include "../storage/settingsrepository.h"
#include "../../tests/mocks/mockhttptransport.h"
#include "storage/sqlmessagerepository.h"
#include "../transport/httptransport.h"
#include "../crypto/cryptoservice.h"

#include <QQmlContext>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <iostream>
#include <fstream>

namespace NeoNect {

static void customLogHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    Q_UNUSED(type);
    Q_UNUSED(context);
    std::cout << msg.toStdString() << std::endl;
    std::ofstream log("qml_error.log", std::ios::app);
    if (log.is_open()) {
        log << msg.toStdString() << std::endl;
    }
}

Application::Application(int &argc, char **argv) {
    setupLogging();

    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
#ifdef _WIN32
    qputenv("QT_FFMPEG_DECODING_HW_DEVICE_TYPES", "none");
    qputenv("QT_FFMPEG_ENCODING_HW_DEVICE_TYPES", "none");
    qputenv("QT_DISABLE_HW_TEXTURES_CONVERSION", "1");
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");
#endif

    m_app = std::make_unique<QGuiApplication>(argc, argv);
    QGuiApplication::setApplicationName("NeoNect");
    QGuiApplication::setApplicationVersion("1.0");
    QGuiApplication::setQuitOnLastWindowClosed(true);

    parseCommandLine();
    initializeServices();
}

Application::~Application() = default;

void Application::setupLogging() {
    qInstallMessageHandler(customLogHandler);
}

void Application::parseCommandLine() {
    QCommandLineParser parser;
    parser.setApplicationDescription("NeoNect Secure E2EE Desktop Client");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption profileOption(QStringList() << "p" << "profile",
                                     "Set active client profile namespace.",
                                     "profile");
    parser.addOption(profileOption);

    QCommandLineOption mockOption("mock", "Run in standalone mock server mode with interactive virtual echo bots.");
    parser.addOption(mockOption);

    parser.process(*m_app);

    m_profile = parser.value(profileOption);
    m_isMockMode = parser.isSet(mockOption);
}

void Application::initializeServices() {
    qRegisterMetaType<std::vector<NeoNect::Domain::Message>>("std::vector<NeoNect::Domain::Message>");
    m_storage = std::make_shared<Storage::SettingsRepository>(m_profile);
    m_cryptoService = std::make_shared<Crypto::CryptoService>();

    if (m_isMockMode) {
        auto mockTransport = std::make_shared<Testing::MockHttpTransport>(true, false);
        if (!m_profile.isEmpty()) {
            QString profileUser = m_profile.trimmed().toLower();
            mockTransport->seedUser(profileUser, "password123");
        }
        m_transport = mockTransport;
        std::cout << "➔ [MOCK MODE ACTIVATED] Running with embedded multi-client server." << std::endl;
    } else {
        m_transport = std::make_shared<Transport::HttpTransport>();
    }

    auto authService = std::make_shared<Services::AuthService>(m_transport, m_storage);
    auto deviceService = std::make_shared<Services::DeviceService>(m_transport, m_storage);
    m_relayService = std::make_shared<Services::RelayService>(m_transport, m_storage, m_cryptoService);
    auto friendService = std::make_shared<Services::FriendService>(m_transport, m_storage);

    m_networkManager = std::make_unique<NetworkManager>(m_transport, m_storage, m_cryptoService, authService, deviceService, m_relayService, friendService);
    m_cryptoManager = std::make_unique<CryptoManager>(m_cryptoService, m_storage);

    // Phase 3 & 4 Message Storage and Services
    QString dbPath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("messages.db");
    m_messageRepo = std::make_shared<Storage::SqlMessageRepository>(dbPath);
    m_messageService = std::make_unique<Services::MessageService>(m_messageRepo);

    // Wire MessageService dependencies
    QObject::connect(m_networkManager.get(), &NetworkManager::currentUsernameChanged, m_messageService.get(), [this]() {
        m_messageService->setCurrentUserId(m_storage->username());
    });
    m_messageService->setCurrentUserId(m_storage->username()); // Initial set
    
    // RelayService -> MessageService (Incoming)
    QObject::connect(m_relayService.get(), &Services::RelayService::incomingDomainMessagesReceived, m_messageService.get(), &Services::MessageService::handleIncomingMessages);
    QObject::connect(m_relayService.get(), &Services::RelayService::messageTransmissionStatus, m_messageService.get(), [this](const QString &, const QString &messageId, bool success, const QString &errorMessage) {
        m_messageService->handleMessageDeliveryStatus(messageId, success, errorMessage);
    });
    
    // MessageService -> RelayService (Outgoing)
    QObject::connect(m_messageService.get(), &Services::MessageService::transmitMessage, m_relayService.get(), &Services::RelayService::sendDomainMessage);

    m_audioManager = std::make_unique<AudioManager>();
    m_notificationManager = std::make_unique<Core::NotificationManager>();
    m_notificationManager->setupMessageServiceHook(m_messageService.get());
}

bool Application::loadMainUi() {
    m_engine = std::make_unique<QQmlApplicationEngine>();

    registerQmlTypes();

    QObject::connect(m_engine.get(), &QQmlApplicationEngine::objectCreationFailed, m_app.get(),
                     []() {
                         std::cerr << "CRITICAL: QML Object creation failed!" << std::endl;
                         QCoreApplication::exit(-1);
                     },
                     Qt::QueuedConnection);

    m_engine->loadFromModule("NeoNect", "Main");

    if (m_engine->rootObjects().isEmpty()) {
        std::cerr << "CRITICAL: engine.rootObjects() is empty!" << std::endl;
        return false;
    }
    return true;
}

int Application::run() {
    if (!loadMainUi()) {
        return 1;
    }
    return m_app->exec();
}

void Application::registerQmlTypes() {
    m_engine->rootContext()->setContextProperty("ThemeData", ThemeData::instance());
    m_engine->rootContext()->setContextProperty("appProfile", m_profile);

    qmlRegisterSingletonInstance("NeoNect.Core", 1, 0, "NetworkManager", m_networkManager.get());
    qmlRegisterSingletonInstance("NeoNect.Core", 1, 0, "CryptoManager", m_cryptoManager.get());
    qmlRegisterSingletonInstance("NeoNect.Core", 1, 0, "AudioManager", m_audioManager.get());
    qmlRegisterSingletonInstance("NeoNect.Core", 1, 0, "NotificationManager", m_notificationManager.get());
    qmlRegisterSingletonInstance("NeoNect.Core", 1, 0, "MessageService", m_messageService.get());
    qmlRegisterType<ChatMessageModel>("NeoNect.Core", 1, 0, "ChatMessageModel");
}

} // namespace NeoNect

