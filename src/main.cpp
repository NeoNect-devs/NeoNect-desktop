// src/main.cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include <iostream>
#include <fstream>
#include "core/networkmanager.h"
#include "core/cryptomanager.h"
#include "core/chatmessagemodel.h"
#include "themedata.h"

void customLogHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    Q_UNUSED(context);
    std::cout << msg.toStdString() << std::endl;
    std::ofstream log("qml_error.log", std::ios::app);
    log << msg.toStdString() << std::endl;
}

int main(int argc, char *argv[]) {
    qInstallMessageHandler(customLogHandler);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("Avila");
    QGuiApplication::setApplicationVersion("1.0");
    QGuiApplication::setQuitOnLastWindowClosed(true);


    QString profile;
    QStringList args = app.arguments();
    for (int i = 0; i < args.size(); ++i) {
        if ((args[i] == "--profile" || args[i] == "-p") && i + 1 < args.size()) {
            profile = args[i + 1];
            break;
        } else if (args[i].startsWith("--profile=")) {
            profile = args[i].mid(10);
            break;
        } else if (args[i].startsWith("-p=")) {
            profile = args[i].mid(3);
            break;
        }
    }

    CryptoManager::instance()->setProfile(profile);
    NetworkManager::instance()->setProfile(profile);


    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("ThemeData", ThemeData::instance());
    engine.rootContext()->setContextProperty("appProfile", profile);

    qmlRegisterSingletonInstance("Avila.Core", 1, 0, "NetworkManager", NetworkManager::instance());
    qmlRegisterSingletonInstance("Avila.Core", 1, 0, "CryptoManager", CryptoManager::instance());
    qmlRegisterType<ChatMessageModel>("Avila.Core", 1, 0, "ChatMessageModel");

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() {
                         std::cout << "CRITICAL: QML Object creation failed!" << std::endl;
                         QCoreApplication::exit(-1);
                     },
                     Qt::QueuedConnection
                     );

    engine.loadFromModule("Avila", "Main");

    if (engine.rootObjects().isEmpty()) {
        std::cout << "CRITICAL: engine.rootObjects() is empty!" << std::endl;
        return 1;
    }

    return app.exec();
}


