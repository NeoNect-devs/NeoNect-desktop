#include <QStandardPaths>
#include <QDir>
#include <QLockFile>
#include <vector>
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
#include <QQuickWindow>
#include <QIcon>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <iostream>
#include <fstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

static void setupWindows11Window(QQuickWindow *window) {
    if (!window) return;
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) return;

    // 1. Force Windows 11 DWM rounded corners
    DWORD preference = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

    // 2. Extend DWM frame for native shadow
    const MARGINS shadow = { 1, 1, 1, 1 };
    DwmExtendFrameIntoClientArea(hwnd, &shadow);

    // 3. Add WS_MAXIMIZEBOX | WS_THICKFRAME to window style
    // This allows Aero Snap (snap to top to maximize, snap to left/right) and Windows 11 snap layouts
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style |= WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME | WS_SYSMENU;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
}
#endif

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
    m_app->setWindowIcon(QIcon(":/qt/qml/NeoNect/assets/NeoNect/icon.png"));
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

    if (m_profile.isEmpty() && !m_isMockMode) {
        static std::vector<std::unique_ptr<QLockFile>> s_instanceLocks;
        QString lockPath = QDir::temp().filePath("neonect_primary_instance.lock");
        auto primaryLock = std::make_unique<QLockFile>(lockPath);
        primaryLock->setStaleLockTime(10000);
        if (primaryLock->tryLock(50)) {
            s_instanceLocks.push_back(std::move(primaryLock));
        } else {
            bool locked = false;
            for (int i = 2; i <= 10; ++i) {
                QString secLockPath = QDir::temp().filePath(QString("neonect_instance_%1.lock").arg(i));
                auto secLock = std::make_unique<QLockFile>(secLockPath);
                secLock->setStaleLockTime(10000);
                if (secLock->tryLock(50)) {
                    m_profile = QString("client%1").arg(i);
                    s_instanceLocks.push_back(std::move(secLock));
                    locked = true;
                    break;
                }
            }
            if (!locked) {
                m_profile = QString("inst_%1").arg(QCoreApplication::applicationPid());
            }
            std::cout << "➔ Secondary instance detected. Running with isolated profile: "
                      << m_profile.toStdString() << std::endl;
        }
    }
}

void Application::initializeServices() {
    qRegisterMetaType<NeoNect::Domain::Message>("NeoNect::Domain::Message");
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
    QString initialUser = m_networkManager->currentUsername().trimmed().toLower();
    QString dbPath = getUserDatabasePath(initialUser);
    m_messageRepo = std::make_shared<Storage::SqlMessageRepository>(dbPath);
    m_messageService = std::make_unique<Services::MessageService>(m_messageRepo);

    // Wire MessageService dependencies & dynamic account database switching
    QObject::connect(m_networkManager.get(), &NetworkManager::currentUsernameChanged, m_messageService.get(), [this]() {
        QString user = m_networkManager->currentUsername().trimmed().toLower();
        m_messageService->setCurrentUserId(user);
        QString userDbPath = getUserDatabasePath(user);
        m_messageRepo->switchDatabase(userDbPath);
    });
    m_messageService->setCurrentUserId(initialUser); // Initial set
    
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

    // RelayService <-> FriendService
    QObject::connect(m_relayService.get(), &Services::RelayService::incomingFriendPacket,
                     friendService.get(), &Services::FriendService::handleIncomingFriendPacket);
    QObject::connect(friendService.get(), &Services::FriendService::requestSendDomainMessage,
                     m_relayService.get(), &Services::RelayService::sendDomainMessage);
    QObject::connect(m_relayService.get(), &Services::RelayService::messageTransmissionStatus,
                     friendService.get(), &Services::FriendService::handleTransmissionStatus);

    // FriendService -> NotificationManager
    QObject::connect(friendService.get(), &Services::FriendService::friendRequestReceived,
                     m_notificationManager.get(), [this](const QString &sender) {
        m_notificationManager->showNotification(sender, "sent you a friend request!", "friend_request", sender, sender.left(1).toUpper(), 5000);
    });
    QObject::connect(friendService.get(), &Services::FriendService::friendAccepted,
                     m_notificationManager.get(), [this](const QString &sender) {
        m_notificationManager->showNotification(sender, "accepted your friend request!", "friend_accept", sender, sender.left(1).toUpper(), 5000);
    });
    QObject::connect(friendService.get(), &Services::FriendService::friendRejected,
                     m_notificationManager.get(), [this](const QString &sender) {
        m_notificationManager->showNotification(sender, "declined your friend request.", "friend_reject", sender, sender.left(1).toUpper(), 5000);
    });
    QObject::connect(friendService.get(), &Services::FriendService::acceptFriendResult,
                     m_notificationManager.get(), [this](bool success, const QString &, const QString &username) {
        if (success) {
            m_notificationManager->dismissBySender(username, "friend_request");
        }
    });
    QObject::connect(friendService.get(), &Services::FriendService::rejectFriendResult,
                     m_notificationManager.get(), [this](bool success, const QString &, const QString &username) {
        if (success) {
            m_notificationManager->dismissBySender(username, "friend_request");
        }
    });

    // Status & Presence Synchronization (Invisible / DND / Auto-Idle)
    QObject::connect(m_networkManager.get(), &NetworkManager::isInvisibleChanged,
                     m_messageService.get(), &Services::MessageService::setIsInvisible);
    QObject::connect(m_networkManager.get(), &NetworkManager::isDndChanged,
                     m_notificationManager.get(), &Core::NotificationManager::setDndEnabled);
    m_messageService->setIsInvisible(m_networkManager->isInvisible());
    m_notificationManager->setDndEnabled(m_networkManager->isDnd());

    // Global Activity Event Filter for 2-Minute Idle Detection
    class ActivityEventFilter : public QObject {
    public:
        explicit ActivityEventFilter(std::function<void()> onActivity, QObject *parent = nullptr)
            : QObject(parent), m_onActivity(std::move(onActivity)) {}
    protected:
        bool eventFilter(QObject *watched, QEvent *event) override {
            switch (event->type()) {
                case QEvent::MouseMove:
                case QEvent::MouseButtonPress:
                case QEvent::MouseButtonRelease:
                case QEvent::MouseButtonDblClick:
                case QEvent::KeyPress:
                case QEvent::KeyRelease:
                case QEvent::Wheel:
                case QEvent::TouchBegin:
                case QEvent::TouchUpdate:
                case QEvent::TouchEnd:
                    if (m_onActivity) m_onActivity();
                    break;
                default:
                    break;
            }
            return QObject::eventFilter(watched, event);
        }
    private:
        std::function<void()> m_onActivity;
    };

    m_activityFilter = std::make_unique<ActivityEventFilter>([this]() {
        if (m_networkManager) {
            m_networkManager->reportActivity();
        }
    });
    if (m_app) {
        m_app->installEventFilter(m_activityFilter.get());
    }
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

    auto *window = qobject_cast<QQuickWindow*>(m_engine->rootObjects().first());
    if (window) {
#ifdef _WIN32
        setupWindows11Window(window);
#endif
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

QString Application::getUserDatabasePath(const QString &username) const {
    QString cleanUser = username.trimmed().toLower();
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(baseDir);
    if (cleanUser.isEmpty()) {
        QString dbName = m_profile.isEmpty() ? "messages_guest.db" : QString("messages_%1_guest.db").arg(m_profile);
        return QDir(baseDir).filePath(dbName);
    }
    QString dbName = m_profile.isEmpty() ? QString("messages_%1.db").arg(cleanUser) : QString("messages_%1_%2.db").arg(m_profile, cleanUser);
    return QDir(baseDir).filePath(dbName);
}

} // namespace NeoNect

