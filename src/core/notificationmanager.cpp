// src/core/notificationmanager.cpp
#include "notificationmanager.h"
#include "networkmanager.h"
#include "../common/constants.h"
#include <QUuid>
#include <QDateTime>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QQuickWindow>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <dwmapi.h>
#endif

namespace NeoNect {
namespace Core {

NotificationManager* NotificationManager::instance() {
    static NotificationManager _instance;
    return &_instance;
}

NotificationManager::NotificationManager(QObject *parent)
    : QObject(parent) {
    loadSettings();
    setupNetworkManagerHook();

    if (auto screen = QGuiApplication::primaryScreen()) {
        connect(screen, &QScreen::availableGeometryChanged, this, &NotificationManager::screenGeometryChanged);
    }
}

int NotificationManager::screenAvailableWidth() const {
    if (auto screen = QGuiApplication::primaryScreen()) {
        int w = screen->availableGeometry().width();
        if (w > 0) return w;
    }
    return 1920;
}

int NotificationManager::screenAvailableHeight() const {
    if (auto screen = QGuiApplication::primaryScreen()) {
        int h = screen->availableGeometry().height();
        if (h > 0) return h;
    }
    return 1080;
}

int NotificationManager::screenAvailableX() const {
    if (auto screen = QGuiApplication::primaryScreen()) {
        return screen->availableGeometry().x();
    }
    return 0;
}

int NotificationManager::screenAvailableY() const {
    if (auto screen = QGuiApplication::primaryScreen()) {
        return screen->availableGeometry().y();
    }
    return 0;
}

void NotificationManager::loadSettings() {
    QSettings settings;
    settings.beginGroup(Constants::SETTINGS_ROOT_GROUP);
    settings.beginGroup("Notifications");
    m_notificationsEnabled = settings.value("enabled", true).toBool();
    m_soundEnabled = settings.value("sound_enabled", true).toBool();
    m_previewEnabled = settings.value("preview_enabled", true).toBool();
    m_dndEnabled = settings.value("dnd_enabled", false).toBool();
    m_screenCorner = settings.value("screen_corner", "bottom-right").toString();
    settings.endGroup();
    settings.endGroup();
}

void NotificationManager::saveSettings() {
    QSettings settings;
    settings.beginGroup(Constants::SETTINGS_ROOT_GROUP);
    settings.beginGroup("Notifications");
    settings.setValue("enabled", m_notificationsEnabled);
    settings.setValue("sound_enabled", m_soundEnabled);
    settings.setValue("preview_enabled", m_previewEnabled);
    settings.setValue("dnd_enabled", m_dndEnabled);
    settings.setValue("screen_corner", m_screenCorner);
    settings.endGroup();
    settings.endGroup();
}

void NotificationManager::setupNetworkManagerHook() {
    auto nm = NetworkManager::instance();
    if (!nm) return;

    connect(nm, &NetworkManager::incomingRelayMessageReceived, this,
            [this](const QString &fromUsername, const QString &target, const QString &text, qint64 timestamp) {
        Q_UNUSED(target);
        Q_UNUSED(timestamp);
        QString me = NetworkManager::instance()->currentUsername();
        if (!me.isEmpty() && fromUsername.compare(me, Qt::CaseInsensitive) == 0) {
            return; // Ignore self messages
        }
        if (fromUsername.isEmpty() || fromUsername == "Anonymous") {
            return;
        }

        showMessageNotification(fromUsername, text, fromUsername.toLower(), fromUsername.left(1).toUpper(), "text");
    }, Qt::QueuedConnection);

    connect(nm, &NetworkManager::incomingRichMessageReceived, this,
            [this](const QVariantMap &data) {
        QString sender = data.value("sender").toString();
        QString me = NetworkManager::instance()->currentUsername();
        if (!me.isEmpty() && sender.compare(me, Qt::CaseInsensitive) == 0) {
            return;
        }
        if (sender.isEmpty() || sender == "Anonymous") {
            return;
        }

        QString type = data.value("type").toString();
        QString text = data.value("text").toString();
        QString fileName = data.value("fileName").toString();

        QString displayBody = text;
        if (type == "voice") {
            int duration = data.value("duration").toInt();
            displayBody = QString("🎙️ Voice message (%1s)").arg(duration > 0 ? duration : 5);
        } else if (type == "image") {
            displayBody = "📷 Photo";
        } else if (type == "sticker") {
            displayBody = "🎭 Sticker";
        } else if (type == "file") {
            displayBody = fileName.isEmpty() ? "📎 Shared file" : "📎 " + fileName;
        }

        showMessageNotification(sender, displayBody, sender.toLower(), sender.left(1).toUpper(), type);
    }, Qt::QueuedConnection);
}

QVariantList NotificationManager::activeNotifications() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeNotifications;
}

void NotificationManager::showNotification(const QString &title,
                                           const QString &body,
                                           const QString &type,
                                           const QString &channel,
                                           const QString &avatar,
                                           int durationMs) {
    if (!m_notificationsEnabled || m_dndEnabled) {
        return;
    }

    QString notifId = QString("notif_%1_%2").arg(QDateTime::currentMSecsSinceEpoch())
                                           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(6));

    QVariantMap notif;
    notif["id"] = notifId;
    notif["notifId"] = notifId;
    notif["title"] = title.isEmpty() ? "NeoNect" : title;
    notif["body"] = m_previewEnabled ? body : "New message received";
    notif["type"] = type.isEmpty() ? "message" : type;
    notif["channel"] = channel;
    notif["avatar"] = avatar.isEmpty() ? title.left(1).toUpper() : avatar;
    notif["duration"] = durationMs > 0 ? durationMs : 4500;
    notif["timestamp"] = QDateTime::currentSecsSinceEpoch();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeNotifications.prepend(notif);
        while (m_activeNotifications.size() > 30) {
            m_activeNotifications.removeLast();
        }
        m_unreadCount++;
    }

    qDebug() << "--> [NotificationManager] showNotification triggered: title=" << notif["title"].toString() << "body=" << notif["body"].toString();
    emit notificationTriggered(notif);
    emit unreadCountChanged();
    emit activeNotificationsChanged();

    playNotificationSound();
}

void NotificationManager::showMessageNotification(const QString &sender,
                                                 const QString &text,
                                                 const QString &channel,
                                                 const QString &avatar,
                                                 const QString &messageType) {
    Q_UNUSED(messageType);
    QString cleanSender = sender.trimmed();
    if (cleanSender.isEmpty()) cleanSender = "Someone";

    QString targetChannel = channel.isEmpty() ? cleanSender.toLower() : channel.toLower();
    QString avatarChar = avatar.isEmpty() ? cleanSender.left(1).toUpper() : avatar;

    showNotification(cleanSender, text, "message", targetChannel, avatarChar, 5000);
}

void NotificationManager::dismissNotification(const QString &id) {
    if (id.isEmpty()) return;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (int i = 0; i < m_activeNotifications.size(); ++i) {
            if (m_activeNotifications.at(i).toMap().value("notifId").toString() == id ||
                m_activeNotifications.at(i).toMap().value("id").toString() == id) {
                m_activeNotifications.removeAt(i);
                break;
            }
        }
    }

    emit notificationDismissed(id);
    emit activeNotificationsChanged();
}

void NotificationManager::clearAll() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeNotifications.clear();
        m_unreadCount = 0;
    }

    emit notificationsCleared();
    emit unreadCountChanged();
    emit activeNotificationsChanged();
}

void NotificationManager::playNotificationSound() {
    if (!m_soundEnabled || m_dndEnabled) {
        return;
    }

    std::thread([]() {
#ifdef _WIN32
        // Play Telegram-like soft alert chime: try Windows system notification sound or harmonic sequence
        if (!PlaySoundW(L"Notification.Default", NULL, SND_ALIAS | SND_ASYNC | SND_NODEFAULT)) {
            Beep(1046, 50); // C6
            Sleep(25);
            Beep(1318, 70); // E6
        }
#endif
    }).detach();
}

void NotificationManager::setNotificationsEnabled(bool enabled) {
    if (m_notificationsEnabled != enabled) {
        m_notificationsEnabled = enabled;
        saveSettings();
        emit notificationsEnabledChanged();
    }
}

void NotificationManager::setSoundEnabled(bool enabled) {
    if (m_soundEnabled != enabled) {
        m_soundEnabled = enabled;
        saveSettings();
        emit soundEnabledChanged();
    }
}

void NotificationManager::setPreviewEnabled(bool enabled) {
    if (m_previewEnabled != enabled) {
        m_previewEnabled = enabled;
        saveSettings();
        emit previewEnabledChanged();
    }
}

void NotificationManager::setDndEnabled(bool enabled) {
    if (m_dndEnabled != enabled) {
        m_dndEnabled = enabled;
        saveSettings();
        emit dndEnabledChanged();
    }
}

void NotificationManager::setScreenCorner(const QString &corner) {
    if (m_screenCorner != corner) {
        m_screenCorner = corner;
        saveSettings();
        emit screenCornerChanged();
    }
}

void NotificationManager::markChannelAsRead(const QString &channel) {
    Q_UNUSED(channel);
    resetUnreadCount();
}

void NotificationManager::resetUnreadCount() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_unreadCount > 0) {
            m_unreadCount = 0;
            emit unreadCountChanged();
        }
    }
}

void NotificationManager::setupFramelessTransparentWindow(QObject *windowObj) {
    auto window = qobject_cast<QQuickWindow*>(windowObj);
    if (!window) return;

    window->setColor(QColor(Qt::transparent));

#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (hwnd) {
        MARGINS margins = {-1, -1, -1, -1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);

        LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        exStyle |= WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);
    }
#endif
}

} // namespace Core
} // namespace NeoNect
