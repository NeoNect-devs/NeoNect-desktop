// src/main.cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext> // ➔ REQUIRED FOR CONTEXT PROPERTIES
#include "core/networkmanager.h"
#include "core/cryptomanager.h"
#include "core/chatmessagemodel.h"
#include "themedata.h" // ➔ 1. INCLUDE YOUR THEME HEADER

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    // ➔ 2. ACCESS OR INSTANTIATE YOUR THEME
    // If ThemeData is a singleton type like NetworkManager, use ThemeData::instance()
    // Otherwise, create an instance pointer here:
    // ThemeData *themeData = new ThemeData(&app);

    // ➔ 3. EXPOSE IT GLOBALLY TO QML
    // This makes the "ThemeData" variable available everywhere in QML instantly
    engine.rootContext()->setContextProperty("ThemeData", ThemeData::instance());
    // (Swap out 'ThemeData::instance()' with 'themeData' if it is not a singleton)

    // Register other backends...
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
