// src/core/notificationmanager.cpp
#include "notificationmanager.h"
#include "../services/messageservice.h"
#include "../common/constants.h"
#include <QUuid>
#include <QDateTime>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QQuickWindow>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <dwmapi.h>
#endif

namespace NeoNect {
namespace Core {

NotificationManager::NotificationManager(QObject *parent)
    : QObject(parent) {
    loadSettings();

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

void NotificationManager::setupMessageServiceHook(Services::MessageService* ms) {
    if (!ms) return;
    connect(ms, &Services::MessageService::messageAdded, this,
            [this](const QString &conversationId, const QVariantMap &data) {
        if (data.value("fromMe").toBool()) return;
        
        QString sender = data.value("senderId").toString();
        if (sender.isEmpty() || sender == "Anonymous") return;
        
        QString text = data.value("text").toString();
        QString type = data.value("type", "text").toString();
        QString avatar = sender.left(1).toUpper();
        
        QString targetChannel = conversationId;
        if (targetChannel.startsWith("dms:")) {
            targetChannel = targetChannel.mid(4);
        }
        
        if (type == "media_request") {
            QString fileName = data.value("fileName").toString();
            qint64 fileSize = data.value("fileSize").toLongLong();
            QString mediaCategory = data.value("errorText").toString();
            if (mediaCategory.isEmpty()) mediaCategory = "media";
            QString sizeStr;
            if (fileSize < 1024) {
                sizeStr = QString("%1 B").arg(fileSize);
            } else if (fileSize < 1024 * 1024) {
                sizeStr = QString("%1 KB").arg(QString::number(fileSize / 1024.0, 'f', 1));
            } else if (fileSize < 1024LL * 1024 * 1024) {
                sizeStr = QString("%1 MB").arg(QString::number(fileSize / (1024.0 * 1024.0), 'f', 1));
            } else {
                sizeStr = QString("%1 GB").arg(QString::number(fileSize / (1024.0 * 1024.0 * 1024.0), 'f', 2));
            }
            QString reqId = data.value("id").toString();
            QString body = QString("wants to send %1: %2 (%3)").arg(mediaCategory, fileName, sizeStr);
            showNotification(sender, body, "media_request", targetChannel, avatar, 10000, reqId);
            return;
        }

        showMessageNotification(sender, text, targetChannel, avatar, type);
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
                                           int durationMs,
                                           const QString &requestId) {
    if (!m_notificationsEnabled) {
        return;
    }

    QString notifId = QString("notif_%1_%2").arg(QDateTime::currentMSecsSinceEpoch())
                                           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(6));

    QVariantMap notif;
    notif["id"] = notifId;
    notif["notifId"] = notifId;
    notif["requestId"] = requestId.isEmpty() ? notifId : requestId;
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

    emit unreadCountChanged();
    emit activeNotificationsChanged();

    // In DND mode, suppress floating screen toast pill and sound, keeping it in notification center
    if (!m_dndEnabled) {
        qDebug() << "--> [NotificationManager] showNotification triggered: title=" << notif["title"].toString() << "body=" << notif["body"].toString();
        emit notificationTriggered(notif);
        playNotificationSound();
    }
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

    bool unreadChanged = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (int i = 0; i < m_activeNotifications.size(); ++i) {
            if (m_activeNotifications.at(i).toMap().value("notifId").toString() == id ||
                m_activeNotifications.at(i).toMap().value("id").toString() == id) {
                m_activeNotifications.removeAt(i);
                break;
            }
        }
        if (m_unreadCount > m_activeNotifications.size()) {
            m_unreadCount = m_activeNotifications.size();
            unreadChanged = true;
        }
    }

    emit notificationDismissed(id);
    if (unreadChanged) {
        emit unreadCountChanged();
    }
    emit activeNotificationsChanged();
}

void NotificationManager::dismissBySender(const QString &sender, const QString &type) {
    if (sender.isEmpty()) return;

    bool unreadChanged = false;
    QStringList dismissedIds;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (int i = m_activeNotifications.size() - 1; i >= 0; --i) {
            auto map = m_activeNotifications.at(i).toMap();
            QString s = map.value("channel").toString();
            if (s.isEmpty()) s = map.value("title").toString();
            QString t = map.value("type").toString();
            if (s.compare(sender, Qt::CaseInsensitive) == 0 && (type.isEmpty() || t == type)) {
                dismissedIds.append(map.value("id").toString());
                m_activeNotifications.removeAt(i);
            }
        }
        if (m_unreadCount > m_activeNotifications.size()) {
            m_unreadCount = m_activeNotifications.size();
            unreadChanged = true;
        }
    }

    for (const QString &id : dismissedIds) {
        emit notificationDismissed(id);
    }
    if (unreadChanged) {
        emit unreadCountChanged();
    }
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
    if (channel.isEmpty()) {
        resetUnreadCount();
        return;
    }

    QString cleanTarget = channel.trimmed();
    if (cleanTarget.startsWith("dms:")) cleanTarget = cleanTarget.mid(4);
    if (cleanTarget.startsWith("dm-")) cleanTarget = cleanTarget.mid(3);
    if (cleanTarget.startsWith("#")) cleanTarget = cleanTarget.mid(1);

    bool unreadChanged = false;
    QStringList dismissedIds;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (int i = m_activeNotifications.size() - 1; i >= 0; --i) {
            auto map = m_activeNotifications.at(i).toMap();
            QString ch = map.value("channel").toString();
            if (ch.startsWith("dms:")) ch = ch.mid(4);
            if (ch.startsWith("dm-")) ch = ch.mid(3);
            if (ch.startsWith("#")) ch = ch.mid(1);
            QString title = map.value("title").toString();

            if (ch.compare(cleanTarget, Qt::CaseInsensitive) == 0 ||
                title.compare(cleanTarget, Qt::CaseInsensitive) == 0 ||
                channel.compare(map.value("channel").toString(), Qt::CaseInsensitive) == 0) {
                dismissedIds.append(map.value("id").toString());
                m_activeNotifications.removeAt(i);
            }
        }
        if (m_unreadCount > m_activeNotifications.size()) {
            m_unreadCount = m_activeNotifications.size();
            unreadChanged = true;
        }
    }

    for (const QString &id : dismissedIds) {
        emit notificationDismissed(id);
    }
    if (unreadChanged) {
        emit unreadCountChanged();
    }
    emit activeNotificationsChanged();
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
