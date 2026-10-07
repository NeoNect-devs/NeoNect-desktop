#include <QStandardPaths>
#include <QDir>
#include <QLockFile>
#include <vector>
// src/core/application.cpp
#include "application.h"
#include "networkmanager.h"
#include "chatmessagemodel.h"
#include "audiomanager.h"
#include "notificationmanager.h"
#include "versioninfo.h"
#include "version.h"
#include "../themedata.h"
#include "../storage/StorageContext.h"
#include "../storage/settingsrepository.h"
#include "../storage/capabilitiesrepository.h"
#include "../../tests/mocks/mockhttptransport.h"
#include "core/messaging/MessageStorage.h"
#include "../transport/httptransport.h"
#include "../crypto/cryptoservice.h"

// E2EE Production Graph Includes
#include "../storage/e2ee/PlatformSecretStore.h"
#include "../storage/e2ee/MasterKeyProvider.h"
#include "../storage/e2ee/SecureE2EEStore.h"
#include "../crypto/x3dh/X3DH.h"
#include "../crypto/doubleratchet/DoubleRatchet.h"
#include "../crypto/doubleratchet/AEAD.h"
#include "../crypto/OpenSSLBackend.h"
#include "../crypto/XEdDSAAdapter.h"
#include "../crypto/KeyGenerationService.h"
#include "messaging/OfflineQueue.h"
#include "messaging/MessageService.h"
#include "../crypto/session/SessionManager.h"
#include "../crypto/session/SecurePreKeyStoreAdapter.h"
#include "../services/prekeyservice.h"

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
namespace Storage {
class MockSecretStore : public IOSSecretStore {
public:
    ServiceResult<std::monostate> writeSecret(const QString&, const QByteArray&) override { return ServiceResult<std::monostate>::ok(std::monostate{}); }
    ServiceResult<QByteArray> readSecret(const QString&) override { return ServiceResult<QByteArray>::ok(QByteArray(32, 'M')); }
    ServiceResult<std::monostate> deleteSecret(const QString&) override { return ServiceResult<std::monostate>::ok(std::monostate{}); }
};
}
}
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

    if (!QCoreApplication::instance()) {
        m_app = std::make_unique<QGuiApplication>(argc, argv);
        m_app->setWindowIcon(QIcon(":/qt/qml/NeoNect/assets/NeoNect/icon.png"));
        QGuiApplication::setApplicationName(QStringLiteral(NEONECT_APP_NAME));
        QGuiApplication::setOrganizationName(QStringLiteral(NEONECT_ORGANIZATION_NAME));
        QGuiApplication::setOrganizationDomain(QStringLiteral(NEONECT_ORGANIZATION_DOMAIN));
        QGuiApplication::setApplicationVersion(QStringLiteral(NEONECT_VERSION_STRING));
        QGuiApplication::setQuitOnLastWindowClosed(true);
    } else {
        // App instance exists, do not recreate
    }


    // Manual fallback for secondary test instances
    for (int i = 0; i < argc; ++i) {
        QString arg = QString::fromUtf8(argv[i]);
        if (arg == "--mock") m_isMockMode = true;
        if (arg.startsWith("--profile=")) m_profile = arg.mid(10);
    }

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

    if (m_app) parser.process(*m_app); else parser.process(*QCoreApplication::instance());

    if (m_app) {
        m_profile = parser.value(profileOption);
        m_isMockMode = parser.isSet(mockOption);
    }

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
    auto capabilitiesRepo = std::make_shared<Storage::CapabilitiesRepository>();
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

    auto authService = std::make_shared<Services::AuthService>(m_transport, m_storage, capabilitiesRepo);
    auto deviceService = std::make_shared<Services::DeviceService>(m_transport, m_storage);
    m_relayService = std::make_shared<Services::RelayService>(m_transport, m_storage, nullptr);
    auto friendService = std::make_shared<Services::FriendService>(m_transport, m_storage);

    m_networkManager = std::make_unique<NetworkManager>(m_transport, m_storage, authService, deviceService, m_relayService, friendService);

    // Phase 2: StorageContext and E2EE Services Integration
    m_messageService = std::make_unique<Services::MessageService>(std::weak_ptr<Core::Messaging::IMessageStorage>());

    auto backend = std::make_shared<Crypto::OpenSSLBackend>();
    auto xeddsa = std::make_shared<Crypto::XEdDSAAdapter>();
    auto x3dh = std::make_shared<Crypto::X3DH::X3DHImpl>();
    auto ratchet = std::make_shared<Crypto::DoubleRatchet::Engine>(backend);
    auto aead = std::make_shared<Crypto::DoubleRatchet::AEAD>(backend.get());

    auto preKeyAdapter = std::make_shared<Crypto::Session::SecurePreKeyStoreAdapter>(std::weak_ptr<Storage::ISecureE2EEStore>());
    auto preKeyService = std::make_shared<Services::PreKeyService>(m_transport, preKeyAdapter);

    m_offlineQueueService = std::make_shared<Core::Messaging::OfflineQueueService>(
        std::weak_ptr<Core::Messaging::IMessageQueue>(),
        [this](const QString& rUser, const QString& rDev, const QString& msgId, const QByteArray& env) {
            if (m_relayService) {
                m_relayService->sendEncryptedEnvelope(rUser, rDev, msgId, env);
                return true;
            }
            return false;
        }
    );

    m_sessionManager = std::make_shared<Crypto::Session::SessionManager>(
        std::weak_ptr<Storage::ISecureE2EEStore>(), x3dh, ratchet, aead, backend, preKeyAdapter, xeddsa,
        [this](const QString& rUser, const QString& rDev, const QString& msgId, const QByteArray& env) -> bool {
            if (m_offlineQueueService) {
                return m_offlineQueueService->onEnvelopeReady(rUser, rDev, msgId, env);
            }
            return false;
        },
        [this](const QString& sessionId, const QByteArray& plaintext) {
            if (m_coreMessageService) {
                m_coreMessageService->receiveMessage(sessionId, plaintext);
            }
        }
    );
    m_relayService->setEnvelopeHandler(m_sessionManager);

    m_coreMessageService = std::make_shared<Core::Messaging::MessageService>(std::weak_ptr<Core::Messaging::IMessageStorage>(), m_sessionManager, std::weak_ptr<Core::Messaging::IMessageQueue>());

    m_coreMessageService->setOnMessageReceived([this](const Core::Messaging::Message& coreMsg) {
        if (m_messageService) {
            NeoNect::Domain::Message domainMsg;
            domainMsg.id = coreMsg.messageId;
            domainMsg.serverId = coreMsg.serverId;
            domainMsg.conversationId = coreMsg.conversationId;
            domainMsg.senderId = coreMsg.senderId;



            domainMsg.timestamp = coreMsg.timestamp;
            domainMsg.text = coreMsg.plaintext;
            domainMsg.type = coreMsg.type;
            domainMsg.mediaUrl = coreMsg.mediaUrl;
            domainMsg.fileName = coreMsg.fileName;
            domainMsg.fileSize = coreMsg.fileSize;
            domainMsg.duration = coreMsg.duration;
            domainMsg.waveform = coreMsg.waveform;
            domainMsg.mediaWidth = coreMsg.mediaWidth;
            domainMsg.mediaHeight = coreMsg.mediaHeight;
            domainMsg.status = NeoNect::Domain::MessageStatus::Sent;

            m_messageService->handleIncomingMessage(domainMsg);
        }
    });
    m_coreMessageService->setPreKeyClaimRequestCallback([preKeyService](const QString& targetUser, const QString& targetDevice, auto resultCb) {
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = QObject::connect(preKeyService.get(), &Services::PreKeyService::preKeyBundleClaimed,
            [resultCb, targetUser, targetDevice, connection](const QString& retUser, const QString& retDev, std::optional<Crypto::X3DH::BobPreKeyBundle> bundle) {
                if (retUser == targetUser && retDev == targetDevice) {
                    QObject::disconnect(*connection);
                    resultCb(bundle);
                }
            });

        preKeyService->claimPreKeys(targetUser, targetDevice);
    });

    m_coreMessageService->setDeviceResolverCallback([this](const QString& targetUser, std::function<void(std::optional<QString>)> resultCb) {
        if (!m_networkManager || !m_networkManager->deviceService()) {
            resultCb(std::nullopt);
            return;
        }

        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = QObject::connect(m_networkManager->deviceService().get(), &Services::DeviceService::recipientKeysFetched,
            [resultCb, targetUser, connection](const QString& retUser, const QVariantList& devices) {
                if (retUser == targetUser) {
                    QObject::disconnect(*connection);
                    if (!devices.isEmpty()) {
                        QVariantMap deviceMap = devices.first().toMap();
                        resultCb(deviceMap.value("device_id").toString());
                    } else {
                        resultCb(std::nullopt);
                    }
                }
            });

        m_networkManager->deviceService()->fetchRecipientKeys(targetUser);
    });


    m_networkManager->deviceService()->setIdentityKeyProvider([this]() {
        if (auto store = m_secureStore.lock()) {
            auto res = m_secureStore.lock()->getIdentity();
            if (res.success) {
                return QString::fromLatin1(res.data.value().public_key.toBase64());
            }
        }
        return QString();
    });

    QObject::connect(m_networkManager.get(), &NetworkManager::deviceRegistrationResult, preKeyService.get(),
        [this, preKeyService](bool success, const QString&) {
            if (success && m_secureStore.lock()) {
                preKeyService->uploadPreKeys(m_storage->deviceId());
            }
        });

    auto onLoginChanged = [this, backend, xeddsa, preKeyAdapter](bool success, const QString& token) {
        if (success) {
            QString user = m_networkManager->currentUsername().trimmed().toLower();
            QString server = m_storage->serverUrl();
            if (user.isEmpty()) return; // Must have authenticated user

            if (m_audioManager) m_audioManager->setStorageBoundary(server, user);

            m_storageContext = std::make_unique<Storage::StorageContext>(server, user, m_isMockMode, m_profile);

            m_messageStorage = m_storageContext->messageStorage();
            m_messageQueue = m_storageContext->messageQueue();
            m_secureStore = m_storageContext->secureStore();

            m_messageService->setStorage(m_messageStorage);
            m_offlineQueueService->setQueue(m_messageQueue);
            m_coreMessageService->setStorage(m_messageStorage);
            m_coreMessageService->setOfflineQueue(m_messageQueue);
            preKeyAdapter->setStore(m_secureStore);
            m_sessionManager->setStore(m_secureStore);

            m_messageService->setCurrentUserId(user);
            m_messageService->setServerUrl(m_storageContext->serverUrl());

            auto idRes = m_secureStore.lock()->getIdentity();
            if (!idRes.success) {
                Crypto::KeyGenerationService keyGen(backend, xeddsa);
                auto identityKeyPair = keyGen.generateIdentityKeyPair();
                Storage::E2EEIdentity newId;
                newId.identity_id = 1;
                newId.public_key = QByteArray(reinterpret_cast<const char*>(identityKeyPair.publicKey.data.data()), 32);
                newId.private_key = QByteArray(reinterpret_cast<const char*>(identityKeyPair.privateKey.data.data()), 32);
                m_secureStore.lock()->saveIdentity(newId);

                auto spk = keyGen.generateSignedPreKey(identityKeyPair, 1);
                preKeyAdapter->storeSignedPreKey(std::move(spk));

                auto opks = keyGen.generateOneTimePreKeys(100, 1);
                preKeyAdapter->storeOneTimePreKeys(std::move(opks));
            }
        } else {
            // Logout
            if (m_audioManager) m_audioManager->setStorageBoundary("", "");

            m_storageContext.reset();
            m_messageStorage.reset();
            m_messageQueue.reset();
            m_secureStore.reset();

            m_messageService->setStorage(std::weak_ptr<Core::Messaging::IMessageStorage>());
            m_offlineQueueService->setQueue(std::weak_ptr<Core::Messaging::IMessageQueue>());
            m_coreMessageService->setStorage(std::weak_ptr<Core::Messaging::IMessageStorage>());
            m_coreMessageService->setOfflineQueue(std::weak_ptr<Core::Messaging::IMessageQueue>());
            preKeyAdapter->setStore(std::weak_ptr<Storage::ISecureE2EEStore>());
            m_sessionManager->setStore(std::weak_ptr<Storage::ISecureE2EEStore>());
        }
    };

    QObject::connect(m_networkManager.get(), &NetworkManager::loginResult, onLoginChanged);

    // Initial load check
    if (!m_storage->authToken().isEmpty() && !m_storage->username().isEmpty()) {
        onLoginChanged(true, m_storage->authToken());
    }

    // RelayService -> MessageService (Incoming)
    // Legacy plaintext path disabled in A8.3
    QObject::connect(m_relayService.get(), &Services::RelayService::messageTransmissionStatus, m_messageService.get(), [this](const QString &, const QString &messageId, bool success, const QString &errorMessage) {
        m_messageService->handleMessageDeliveryStatus(messageId, success, errorMessage);
        if (m_offlineQueueService) {
            if (success) {
                m_offlineQueueService->handleAck(messageId);
            } else {
                m_offlineQueueService->handleTransportFailure(messageId);
            }
        }
    });

    // MessageService -> RelayService (Outgoing E2EE Adapter)
    QObject::connect(m_messageService.get(), &Services::MessageService::transmitMessage, m_relayService.get(), [this](const NeoNect::Domain::Message& msg) {
        Core::Messaging::Message coreMsg;
        coreMsg.messageId = msg.id;
        coreMsg.conversationId = msg.conversationId;
        coreMsg.senderId = msg.senderId;

        // Extract plain username if it's a DM prefix
        if (msg.conversationId.startsWith("dms:")) {
            coreMsg.receiverId = msg.conversationId.mid(4);
        } else {
            coreMsg.receiverId = msg.conversationId; // fallback
        }

        coreMsg.timestamp = msg.timestamp;
        coreMsg.plaintext = msg.text;
        coreMsg.type = msg.type;
        coreMsg.mediaUrl = msg.mediaUrl;
        coreMsg.fileName = msg.fileName;
        coreMsg.fileSize = msg.fileSize;
        coreMsg.duration = msg.duration;
        coreMsg.waveform = msg.waveform;
        coreMsg.mediaWidth = msg.mediaWidth;
        coreMsg.mediaHeight = msg.mediaHeight;
        coreMsg.state = Core::Messaging::MessageState::CREATED;

        m_coreMessageService->sendMessage(coreMsg);
    });

    // Broadcast presence status changes to active chat peers and friends
    auto broadcastPresence = [this]() {
        if (!m_networkManager->isConnected()) return;
        QString st = m_networkManager->effectiveStatus();
        QSet<QString> targets;
        for (const QString &f : m_networkManager->friends()) {
            QString clean = f.trimmed().toLower();
            if (!clean.isEmpty()) targets.insert(clean);
        }
        for (const auto &conv : m_networkManager->openConversations()) {
            QString name = conv.toMap().value("name").toString().trimmed().toLower();
            if (!name.isEmpty() && name != "saved-messages" && name != "friends") {
                targets.insert(name);
            }
        }
        for (const QString &target : targets) {
            m_messageService->sendPresenceStatus(target, st);
        }
    };

    QObject::connect(m_networkManager.get(), &NetworkManager::effectiveStatusChanged, m_networkManager.get(), broadcastPresence);
    QObject::connect(m_networkManager.get(), &NetworkManager::friendsChanged, m_networkManager.get(), broadcastPresence);
    QObject::connect(m_networkManager.get(), &NetworkManager::displayNameChanged, m_networkManager.get(), broadcastPresence);

    m_audioManager = std::make_unique<AudioManager>();
    if (!m_storage->username().isEmpty() && !m_storage->serverUrl().isEmpty()) {
        m_audioManager->setStorageBoundary(m_storage->serverUrl(), m_storage->username().trimmed().toLower());
    }
    
    QObject::connect(m_messageService.get(), &Services::MessageService::localMediaAbandoned,
                     m_audioManager.get(), &AudioManager::cleanupLocalFile);

    m_notificationManager = std::make_unique<Core::NotificationManager>();
    m_notificationManager->setupMessageServiceHook(m_messageService.get());
    m_versionInfo = std::make_unique<Core::VersionInfo>();

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
        QString dn = m_networkManager->getDisplayName(sender);
        m_notificationManager->showNotification(dn, "sent you a friend request!", "friend_request", sender, dn.left(1).toUpper(), 5000);
    });
    QObject::connect(friendService.get(), &Services::FriendService::friendAccepted,
                     m_notificationManager.get(), [this](const QString &sender) {
        QString dn = m_networkManager->getDisplayName(sender);
        m_notificationManager->showNotification(dn, "accepted your friend request!", "friend_accept", sender, dn.left(1).toUpper(), 5000);
    });
    QObject::connect(friendService.get(), &Services::FriendService::friendRejected,
                     m_notificationManager.get(), [this](const QString &sender) {
        QString dn = m_networkManager->getDisplayName(sender);
        m_notificationManager->showNotification(dn, "declined your friend request.", "friend_reject", sender, dn.left(1).toUpper(), 5000);
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

    QObject::connect(m_messageService.get(), &Services::MessageService::peerAvatarDataReceived, m_networkManager.get(), &NetworkManager::setPeerAvatarData);
    QObject::connect(m_networkManager.get(), &NetworkManager::systemMessageRequested, m_messageService.get(), [this](const QString &convId, const QString &text, const QString &type) {
        m_messageService->sendMessage(convId, text, type, QString());
    });
    qmlRegisterSingletonInstance("NeoNect.Core", 1, 0, "NetworkManager", m_networkManager.get());
    qmlRegisterSingletonInstance("NeoNect.Core", 1, 0, "AudioManager", m_audioManager.get());
    qmlRegisterSingletonInstance("NeoNect.Core", 1, 0, "NotificationManager", m_notificationManager.get());
    qmlRegisterSingletonInstance("NeoNect.Core", 1, 0, "MessageService", m_messageService.get());
    qmlRegisterSingletonInstance("NeoNect.Core", 1, 0, "VersionInfo", m_versionInfo.get());
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

