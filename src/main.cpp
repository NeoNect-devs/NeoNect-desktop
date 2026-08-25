// src/main.cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include "core/networkmanager.h"
#include "core/cryptomanager.h"
#include "core/chatmessagemodel.h"
#include "themedata.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("Avila");
    QGuiApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Avila Desktop Client");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption profileOption(QStringList() << "p" << "profile", "Profile identifier for running multiple instances side-by-side", "profile");
    parser.addOption(profileOption);
    parser.process(app);

    QString profile = parser.value(profileOption);
    if (!profile.isEmpty()) {
        CryptoManager::instance()->setProfile(profile);
        NetworkManager::instance()->setProfile(profile);
    }

    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("ThemeData", ThemeData::instance());
    engine.rootContext()->setContextProperty("appProfile", profile);

    qmlRegisterSingletonInstance("Avila.Core", 1, 0, "NetworkManager", NetworkManager::instance());
    qmlRegisterSingletonInstance("Avila.Core", 1, 0, "CryptoManager", CryptoManager::instance());
    qmlRegisterType<ChatMessageModel>("Avila.Core", 1, 0, "ChatMessageModel");

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection
                     );

    engine.loadFromModule("Avila", "Main");

    return app.exec();
}

