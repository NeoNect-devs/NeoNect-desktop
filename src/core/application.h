// src/core/application.h
#pragma once
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QString>
#include <memory>

namespace Avila {

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
    QString m_profile;
};

} // namespace Avila
