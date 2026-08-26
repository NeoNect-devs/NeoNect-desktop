// src/core/application.cpp
#include "application.h"
#include "networkmanager.h"
#include "cryptomanager.h"
#include "chatmessagemodel.h"
#include "../themedata.h"
#include "../../tests/mocks/mockhttptransport.h"

#include <QQmlContext>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <iostream>
#include <fstream>

namespace Avila {

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

    m_app = std::make_unique<QGuiApplication>(argc, argv);
    QGuiApplication::setApplicationName("Avila");
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
    parser.setApplicationDescription("Avila Secure E2EE Desktop Client");
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
    CryptoManager::instance()->setProfile(m_profile);
    if (m_isMockMode) {
        auto mockTransport = std::make_shared<Testing::MockHttpTransport>(true);
        NetworkManager::instance()->initializeCustom(mockTransport);
        std::cout << "➔ [MOCK MODE ACTIVATED] Running with embedded in-memory server & virtual echo bots." << std::endl;
    }
    NetworkManager::instance()->setProfile(m_profile);
}

void Application::registerQmlTypes() {
    m_engine->rootContext()->setContextProperty("ThemeData", ThemeData::instance());
    m_engine->rootContext()->setContextProperty("appProfile", m_profile);

    qmlRegisterSingletonInstance("Avila.Core", 1, 0, "NetworkManager", NetworkManager::instance());
    qmlRegisterSingletonInstance("Avila.Core", 1, 0, "CryptoManager", CryptoManager::instance());
    qmlRegisterType<ChatMessageModel>("Avila.Core", 1, 0, "ChatMessageModel");
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

    m_engine->loadFromModule("Avila", "Main");

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

} // namespace Avila
