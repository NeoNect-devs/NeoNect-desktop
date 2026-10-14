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
#include "../transport/EnvelopeRouter.h"

#include <QQmlContext>
#include <QQuickWindow>
#include <QIcon>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QEventLoop>
#include <QTimer>
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
    Q_UNUSED(context);
#if defined(NDEBUG)
    if (type == QtDebugMsg) {
        return;
    }
#endif
    std::cout << msg.toStdString() << std::endl;
    if (type != QtDebugMsg) {
        std::ofstream log("qml_error.log", std::ios::app);
        if (log.is_open()) {
            log << msg.toStdString() << std::endl;
        }
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
    m_relayService = std::make_shared<Services::RelayService>(m_transport, m_storage, capabilitiesRepo, nullptr);
    auto friendService = std::make_shared<Services::FriendService>(m_transport, m_storage);

    m_networkManager = std::make_unique<NetworkManager>(m_transport, m_storage, authService, deviceService, m_relayService, friendService, capabilitiesRepo);

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
    m_sessionManager->setSenderVerifier([this](const QString& claimedUser, const QString& senderDev, const QByteArray& senderIdentityKey) -> bool {
        if (!m_networkManager || !m_networkManager->deviceService()) return false;
        if (m_isMockMode && senderDev.compare(claimedUser + ":default_device", Qt::CaseInsensitive) == 0) {
            return true;
        }
        QPointer<NetworkManager> netGuard(m_networkManager.get());
        std::weak_ptr<Storage::ISecureE2EEStore> weakStore = m_secureStore;
        const bool isMockMode = m_isMockMode;
        bool done = false;
        bool verified = false;
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = QObject::connect(m_networkManager->deviceService().get(), &Services::DeviceService::recipientKeysFetched,
            [&done, &verified, claimedUser, senderDev, senderIdentityKey, conn, netGuard, isMockMode](const QString& retUser, const QVariantList& devices) {
                if (!netGuard || retUser.compare(claimedUser, Qt::CaseInsensitive) != 0) return;
                QObject::disconnect(*conn);
                done = true;
                for (const QVariant& v : devices) {
                    QVariantMap d = v.toMap();
                    if (d.value("device_id").toString() == senderDev) {
                        QString pubKeyStr = d.value("public_key").toString();
                        if (isMockMode && pubKeyStr == "mock_key") {
                            verified = true;
                            break;
                        }
                        QByteArray pubKey = QByteArray::fromBase64(pubKeyStr.toLatin1());
                        if (!pubKey.isEmpty() && pubKey == senderIdentityKey) {
                            verified = true;
                        }
                        break;
                    }
                }
            });
        m_networkManager->deviceService()->fetchRecipientKeys(claimedUser);
        if (!done && netGuard && netGuard->deviceService()) {
            QEventLoop loop;
            QTimer timer;
            timer.setSingleShot(true);
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            auto loopConn = QObject::connect(netGuard->deviceService().get(), &Services::DeviceService::recipientKeysFetched, &loop,
                [&loop, claimedUser](const QString& retUser, const QVariantList&) {
                    if (retUser.compare(claimedUser, Qt::CaseInsensitive) == 0) {
                        loop.quit();
                    }
                });
            auto logoutConn = QObject::connect(netGuard.data(), &NetworkManager::loginResult, &loop,
                [&loop](bool success, const QString&) {
                    if (!success) {
                        loop.quit();
                    }
                });
            timer.start(3000);
            loop.exec();
            QObject::disconnect(loopConn);
            QObject::disconnect(logoutConn);
        }
        QObject::disconnect(*conn);
        if (!netGuard || weakStore.expired()) {
            return false;
        }
        return verified;
    });
    m_fileTransferManager = std::make_shared<Services::FileTransferManager>(
        std::weak_ptr<Storage::ISecureE2EEStore>(),
        backend,
        capabilitiesRepo,
        m_storage,
        [this](const QString& rUser, const QString& rDev, const QString& msgId, const QByteArray& env) {
            if (m_relayService) {
                m_relayService->sendEncryptedEnvelope(rUser, rDev, msgId, env);
                return true;
            }
            return false;
        },
        [this](const QJsonObject& obj, const QString& ru, const QString& rd) {
            Core::Messaging::Message coreMsg;
            coreMsg.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            coreMsg.conversationId = "dms:" + ru;
            coreMsg.senderId = m_storage->username();
            coreMsg.receiverId = ru;
            coreMsg.receiverDeviceId = rd;
            coreMsg.timestamp = QDateTime::currentMSecsSinceEpoch();
            coreMsg.plaintext = QJsonDocument(obj).toJson(QJsonDocument::Compact);
            coreMsg.type = obj.value("type").toString("file_start");
            coreMsg.state = Core::Messaging::MessageState::CREATED;
            if (m_coreMessageService) m_coreMessageService->sendMessage(coreMsg);
        }
    );
    m_messageService->setFileTransferManager(m_fileTransferManager);

    auto envelopeRouter = std::make_shared<Transport::EnvelopeRouter>();
    envelopeRouter->setSessionManagerHandler(m_sessionManager);
    envelopeRouter->setFileTransferHandler(m_fileTransferManager);
    m_relayService->setEnvelopeHandler(envelopeRouter);

    m_secureImageProvider = new Services::SecureImageProvider(std::weak_ptr<Storage::ISecureE2EEStore>(), backend, m_storage);

    m_coreMessageService = std::make_shared<Core::Messaging::MessageService>(std::weak_ptr<Core::Messaging::IMessageStorage>(), m_sessionManager, std::weak_ptr<Core::Messaging::IMessageQueue>());

    m_coreMessageService->setOnDeliveryStateChanged([this](const QString& msgId, Core::Messaging::MessageState state, const QString& reason) {
        if (state == Core::Messaging::MessageState::FAILED && m_messageService) {
            m_messageService->handleMessageDeliveryStatus(msgId, false, reason);
        }
    });

    m_coreMessageService->setOnMessageReceived([this](const Core::Messaging::Message& coreMsg) {
        if (!coreMsg.senderDisplayName.isEmpty() && m_networkManager) {
            m_networkManager->setPeerDisplayName(coreMsg.senderId, coreMsg.senderDisplayName);
        }
        if (coreMsg.type == "file_start" || coreMsg.type == "file_ack" || coreMsg.type == "file_nack" || coreMsg.type == "file_cancel") {
            if (m_fileTransferManager && !coreMsg.senderDeviceId.isEmpty()) {
                QJsonParseError err;
                QJsonDocument doc = QJsonDocument::fromJson(coreMsg.plaintext.toUtf8(), &err);
                if (err.error == QJsonParseError::NoError && doc.isObject()) {
                    m_fileTransferManager->handleControlMessage(coreMsg.senderId, coreMsg.senderDeviceId, doc.object());
                }
            }
            return;
        }
        if (m_messageService) {
            NeoNect::Domain::Message domainMsg;
            domainMsg.id = coreMsg.messageId;
            domainMsg.serverId = coreMsg.serverId;
            domainMsg.conversationId = coreMsg.conversationId;
            if (domainMsg.conversationId.startsWith("dms:", Qt::CaseInsensitive) &&
                domainMsg.conversationId.compare("dms:saved-messages", Qt::CaseInsensitive) != 0 &&
                !coreMsg.senderId.trimmed().isEmpty()) {
                domainMsg.conversationId = "dms:" + coreMsg.senderId.trimmed().toLower();
            }
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
            domainMsg.errorText = coreMsg.errorText;
            domainMsg.status = NeoNect::Domain::MessageStatus::Sent;

            if (m_relayService) {
                emit m_relayService->incomingDomainMessagesReceived({domainMsg});
            }
            m_messageService->handleIncomingMessage(domainMsg);
        }
    });
    m_coreMessageService->setPreKeyClaimRequestCallback([preKeyService](const QString& targetUser, const QString& targetDevice, auto resultCb) {
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = QObject::connect(preKeyService.get(), &Services::PreKeyService::preKeyBundleClaimed,
            [resultCb, targetUser, targetDevice, connection](const QString& retUser, const QString& retDev, std::optional<Crypto::X3DH::BobPreKeyBundle> bundle) {
                if (retUser.compare(targetUser.trimmed(), Qt::CaseInsensitive) == 0 && retDev == targetDevice) {
                    QObject::disconnect(*connection);
                    resultCb(bundle);
                }
            });

        preKeyService->claimPreKeys(targetUser, targetDevice);
    });

    auto deviceResolver = [this](const QString& targetUser, std::function<void(std::optional<QString>)> resultCb) {
        if (!m_networkManager || !m_networkManager->deviceService()) {
            resultCb(std::nullopt);
            return;
        }

        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = QObject::connect(m_networkManager->deviceService().get(), &Services::DeviceService::recipientKeysFetched,
            [resultCb, targetUser, connection](const QString& retUser, const QVariantList& devices) {
                if (retUser.compare(targetUser.trimmed(), Qt::CaseInsensitive) == 0) {
                    QObject::disconnect(*connection);
                    for (const QVariant& v : devices) {
                        QVariantMap deviceMap = v.toMap();
                        if (!deviceMap.value("revoked_at").toString().isEmpty()) {
                            continue;
                        }
                        QString devId = deviceMap.value("device_id").toString().trimmed();
                        if (!devId.isEmpty()) {
                            resultCb(devId);
                            return;
                        }
                    }
                    resultCb(std::nullopt);
                }
            });

        m_networkManager->deviceService()->fetchRecipientKeys(targetUser);
    };

    auto multiDeviceResolver = [this](const QString& targetUser, std::function<void(QStringList)> resultCb) {
        if (!m_networkManager || !m_networkManager->deviceService()) {
            resultCb({});
            return;
        }

        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = QObject::connect(m_networkManager->deviceService().get(), &Services::DeviceService::recipientKeysFetched,
            [resultCb, targetUser, connection](const QString& retUser, const QVariantList& devices) {
                if (retUser.compare(targetUser.trimmed(), Qt::CaseInsensitive) == 0) {
                    QObject::disconnect(*connection);
                    QStringList deviceIds;
                    for (const QVariant& v : devices) {
                        QVariantMap deviceMap = v.toMap();
                        if (!deviceMap.value("revoked_at").toString().isEmpty()) {
                            continue;
                        }
                        QString devId = deviceMap.value("device_id").toString().trimmed();
                        if (!devId.isEmpty() && !deviceIds.contains(devId)) {
                            deviceIds.append(devId);
                        }
                    }
                    resultCb(deviceIds);
                }
            });

        m_networkManager->deviceService()->fetchRecipientKeys(targetUser);
    };

    m_coreMessageService->setDeviceResolverCallback(deviceResolver);
    m_coreMessageService->setMultiDeviceResolverCallback(multiDeviceResolver);
    m_messageService->setDeviceResolverCallback(deviceResolver);
    m_messageService->setMultiDeviceResolverCallback(multiDeviceResolver);

    m_networkManager->deviceService()->setIdentityKeyProvider([this]() {
        if (auto store = m_secureStore.lock()) {
            auto res = store->getIdentity();
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
            if (success && m_offlineQueueService) {
                m_offlineQueueService->resume();
            }
        });

    QObject::connect(m_relayService.get(), &Services::RelayService::serverConnected, m_relayService.get(),
        [this]() {
            if (m_offlineQueueService) {
                m_offlineQueueService->resume();
            }
        });

    auto teardownUserServices = [this, preKeyAdapter]() {
        if (m_audioManager) m_audioManager->setStorageBoundary("", "");

        m_storageContext.reset();
        m_messageStorage.reset();
        m_messageQueue.reset();
        m_secureStore.reset();

        m_messageService->setStorage(std::weak_ptr<Core::Messaging::IMessageStorage>());
        m_messageService->setCurrentUserId(QString());
        m_messageService->setServerUrl(QString());
        m_offlineQueueService->setQueue(std::weak_ptr<Core::Messaging::IMessageQueue>());
        m_coreMessageService->setStorage(std::weak_ptr<Core::Messaging::IMessageStorage>());
        m_coreMessageService->setOfflineQueue(std::weak_ptr<Core::Messaging::IMessageQueue>());
        preKeyAdapter->setStore(std::weak_ptr<Storage::ISecureE2EEStore>());
        m_sessionManager->setStore(std::weak_ptr<Storage::ISecureE2EEStore>());
        if (m_fileTransferManager) m_fileTransferManager->setStore(std::weak_ptr<Storage::ISecureE2EEStore>());
        if (m_secureImageProvider) m_secureImageProvider->setStore(std::weak_ptr<Storage::ISecureE2EEStore>());
    };

    auto initUserServices = [this, backend, xeddsa, preKeyAdapter, teardownUserServices](QString& errorOut) -> bool {
        QString user = m_networkManager->currentUsername().trimmed().toLower();
        QString server = m_storage->serverUrl();
        if (user.isEmpty()) {
            errorOut = QStringLiteral("Authenticated username is missing.");
            return false;
        }

        if (m_storageContext && m_storageContext->isE2EEReady() &&
            m_storageContext->serverUrl() == server && m_storageContext->username() == user) {
            return true;
        }

        if (m_audioManager) m_audioManager->setStorageBoundary(server, user);

        m_storageContext = std::make_unique<Storage::StorageContext>(server, user, m_isMockMode, m_profile);
        if (!m_storageContext->isE2EEReady()) {
            errorOut = QStringLiteral("E2EE storage could not be unlocked (%1). Verify your OS keyring/secret-service environment.")
                           .arg(m_storageContext->e2eeError());
            qCritical() << "[Application] CRITICAL:" << errorOut;
            teardownUserServices();
            return false;
        }

        m_secureStore = m_storageContext->secureStore();
        preKeyAdapter->setStore(m_secureStore);

        auto idCheck = ensureE2EEIdentity(m_secureStore.lock(), backend, xeddsa, preKeyAdapter);
        if (!idCheck.success) {
            errorOut = idCheck.message;
            qCritical() << "[Application] CRITICAL: E2EE identity initialization failed:" << errorOut;
            teardownUserServices();
            return false;
        }

        m_messageStorage = m_storageContext->messageStorage();
        m_messageQueue = m_storageContext->messageQueue();

        m_messageService->setStorage(m_messageStorage);
        m_offlineQueueService->setQueue(m_messageQueue);
        m_coreMessageService->setStorage(m_messageStorage);
        m_coreMessageService->setOfflineQueue(m_messageQueue);
        m_sessionManager->setStore(m_secureStore);
        if (m_fileTransferManager) {
            m_fileTransferManager->setStore(m_secureStore);
            m_fileTransferManager->setIsImageTransferCb([this](const QString& transferId) -> bool {
                auto lockedStorage = m_messageStorage.lock();
                if (!lockedStorage) return false;
                auto msgOpt = lockedStorage->getMessage(transferId);
                if (msgOpt) return msgOpt->type == "image";
                return false;
            });
        }
        if (m_secureImageProvider) m_secureImageProvider->setStore(m_secureStore);

        m_messageService->setCurrentUserId(user);
        m_messageService->setServerUrl(m_storageContext->serverUrl());
        return true;
    };

    m_networkManager->setSessionValidator(initUserServices);

    auto onLoginChanged = [this, initUserServices, teardownUserServices](bool success, const QString& token) {
        Q_UNUSED(token);
        if (success) {
            QString initError;
            if (!initUserServices(initError)) {
                if (m_networkManager) {
                    m_networkManager->abortSessionWithError(initError);
                }
            }
        } else {
            teardownUserServices();
        }
    };

    auto announcedPresencePeers = std::make_shared<QSet<QString>>();

    QObject::connect(m_networkManager.get(), &NetworkManager::loginResult, [onLoginChanged, announcedPresencePeers](bool success, const QString &tokenOrError) {
        if (!success) {
            announcedPresencePeers->clear();
        }
        onLoginChanged(success, tokenOrError);
    });

    if (!m_storage->authToken().isEmpty() && !m_storage->username().isEmpty()) {
        onLoginChanged(true, m_storage->authToken());
        if (!m_isMockMode && !m_networkManager->token().isEmpty()) {
            m_networkManager->resumeSavedSession();
        }
    }

    QObject::connect(m_relayService.get(), &Services::RelayService::messageTransmissionStatus, m_messageService.get(), [this](const QString &, const QString &messageId, bool success, const QString &errorMessage) {
        if (!messageId.contains('#')) {
            if (auto s = m_messageStorage.lock()) {
                s->updateMessageState(messageId, success ? Core::Messaging::MessageState::SENT : Core::Messaging::MessageState::FAILED);
            }
            m_messageService->handleMessageDeliveryStatus(messageId, success, errorMessage);
        } else if (success) {
            const QString baseMessageId = messageId.section('#', 0, 0);
            if (auto s = m_messageStorage.lock()) {
                s->updateMessageState(baseMessageId, Core::Messaging::MessageState::SENT);
            }
            m_messageService->handleMessageDeliveryStatus(baseMessageId, true, QString());
        }
        if (m_offlineQueueService) {
            if (success) {
                m_offlineQueueService->handleAck(messageId);
            } else {
                m_offlineQueueService->handleTransportFailure(messageId);
            }
        }
    });

    QObject::connect(m_messageService.get(), &Services::MessageService::transmitMessage, m_relayService.get(), [this](const NeoNect::Domain::Message& msg) {
        Core::Messaging::Message coreMsg;
        coreMsg.messageId = msg.id;
        coreMsg.conversationId = msg.conversationId;
        coreMsg.senderId = msg.senderId;
        if (m_storage && !m_storage->displayName().trimmed().isEmpty()) {
            coreMsg.senderDisplayName = m_storage->displayName().trimmed();
        }

        if (msg.conversationId.startsWith("dms:")) {
            coreMsg.receiverId = msg.conversationId.mid(4);
        } else {
            coreMsg.receiverId = msg.conversationId;
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
        coreMsg.errorText = msg.errorText;
        coreMsg.state = Core::Messaging::MessageState::CREATED;

        m_coreMessageService->sendMessage(coreMsg);
    });

    auto broadcastPresence = [this, announcedPresencePeers](bool forceAll) {
        if (!m_networkManager->isConnected()) {
            announcedPresencePeers->clear();
            return;
        }
        if (forceAll) {
            announcedPresencePeers->clear();
        }
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
            if (!announcedPresencePeers->contains(target)) {
                announcedPresencePeers->insert(target);
                m_messageService->sendPresenceStatus(target, st);
            }
        }
    };

    QObject::connect(m_networkManager.get(), &NetworkManager::effectiveStatusChanged, m_networkManager.get(), [broadcastPresence]() {
        broadcastPresence(true);
    });
    QObject::connect(m_networkManager.get(), &NetworkManager::friendsChanged, m_networkManager.get(), [broadcastPresence]() {
        broadcastPresence(false);
    });
    QObject::connect(m_networkManager.get(), &NetworkManager::displayNameChanged, m_networkManager.get(), [broadcastPresence]() {
        broadcastPresence(true);
    });

    QObject::connect(m_messageService.get(), &Services::MessageService::peerAvatarDataReceived, m_networkManager.get(), &NetworkManager::setPeerAvatarData);
    QObject::connect(m_networkManager.get(), &NetworkManager::systemMessageRequested, m_messageService.get(), [this](const QString &convId, const QString &text, const QString &type) {
        m_messageService->sendMessage(convId, text, type, QString());
    });

    m_audioManager = std::make_unique<AudioManager>();
    m_audioManager->setStorageBoundary(m_storage->serverUrl(), m_storage->username().trimmed().toLower());
    
    QObject::connect(m_messageService.get(), &Services::MessageService::localMediaAbandoned,
                     m_audioManager.get(), &AudioManager::cleanupLocalFile);

    m_notificationManager = std::make_unique<Core::NotificationManager>();
    m_notificationManager->setupMessageServiceHook(m_messageService.get());
    m_versionInfo = std::make_unique<Core::VersionInfo>();

    // RelayService <-> FriendService
    QObject::connect(m_relayService.get(), &Services::RelayService::incomingFriendPacket,
                     friendService.get(), &Services::FriendService::handleIncomingFriendPacket);

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

ServiceResult<std::monostate> Application::ensureE2EEIdentity(
    const std::shared_ptr<Storage::ISecureE2EEStore> &store,
    const std::shared_ptr<Crypto::ICryptoBackend> &backend,
    const std::shared_ptr<Crypto::IXEdDSA> &xeddsa,
    const std::shared_ptr<Crypto::Session::SecurePreKeyStoreAdapter> &preKeyAdapter)
{
    if (!store || !store->isOpen()) {
        qCritical() << "[Application] CRITICAL: E2EE secure store is not open; refusing to initialize or overwrite identity.";
        return ServiceResult<std::monostate>::fail("E2EE secure storage could not be opened or unlocked.");
    }

    auto idRes = store->getIdentity();
    if (idRes.success) {
        if (!idRes.data.has_value() || idRes.data->public_key.size() != 32 || idRes.data->private_key.size() != 32) {
            qCritical() << "[Application] CRITICAL: Stored E2EE identity has invalid key lengths; refusing to overwrite.";
            return ServiceResult<std::monostate>::fail("Stored E2EE identity is invalid or corrupted.");
        }
        if (m_storage) {
            m_storage->setPublicKey(QString::fromLatin1(idRes.data->public_key.toBase64()));
        }
        return ServiceResult<std::monostate>::ok({});
    }

    if (idRes.message != "Identity not found") {
        qCritical() << "[Application] CRITICAL: E2EE identity lookup failed (" << idRes.message
                    << "); refusing to generate replacement identity.";
        return ServiceResult<std::monostate>::fail(QString("E2EE identity lookup failed: %1").arg(idRes.message));
    }

    if (!backend || !xeddsa) {
        return ServiceResult<std::monostate>::fail("Cryptographic backend unavailable for E2EE identity generation.");
    }

    Crypto::KeyGenerationService keyGen(backend, xeddsa);
    auto identityKeyPair = keyGen.generateIdentityKeyPair();
    Storage::E2EEIdentity newId;
    newId.identity_id = 1;
    newId.public_key = QByteArray(reinterpret_cast<const char*>(identityKeyPair.publicKey.data.data()), 32);
    newId.private_key = QByteArray(reinterpret_cast<const char*>(identityKeyPair.privateKey.data.data()), 32);

    auto saveRes = store->saveIdentity(newId);
    if (!saveRes.success) {
        qCritical() << "[Application] CRITICAL: Failed to save generated E2EE identity (" << saveRes.message
                    << "); refusing to update public_key setting.";
        return ServiceResult<std::monostate>::fail(QString("Failed to persist E2EE identity: %1").arg(saveRes.message));
    }

    if (preKeyAdapter) {
        auto spk = keyGen.generateSignedPreKey(identityKeyPair, 1);
        preKeyAdapter->storeSignedPreKey(std::move(spk));
        auto opks = keyGen.generateOneTimePreKeys(100, 1);
        preKeyAdapter->storeOneTimePreKeys(std::move(opks));
    }

    if (m_storage) {
        m_storage->setPublicKey(QString::fromLatin1(newId.public_key.toBase64()));
    }
    return ServiceResult<std::monostate>::ok({});
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
    qmlRegisterSingletonInstance("NeoNect.Core", 1, 0, "AudioManager", m_audioManager.get());
    qmlRegisterSingletonInstance("NeoNect.Core", 1, 0, "NotificationManager", m_notificationManager.get());
    qmlRegisterSingletonInstance("NeoNect.Core", 1, 0, "MessageService", m_messageService.get());
    qmlRegisterSingletonInstance("NeoNect.Core", 1, 0, "VersionInfo", m_versionInfo.get());
    if (m_secureImageProvider) {
        m_engine->addImageProvider("secure", m_secureImageProvider);
    }
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

