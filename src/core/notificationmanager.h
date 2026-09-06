// src/core/notificationmanager.h
#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QSettings>
#include <memory>
#include <mutex>

namespace NeoNect {
namespace Services { class MessageService; }
namespace Core {

class NotificationManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled NOTIFY notificationsEnabledChanged)
    Q_PROPERTY(bool soundEnabled READ soundEnabled WRITE setSoundEnabled NOTIFY soundEnabledChanged)
    Q_PROPERTY(bool previewEnabled READ previewEnabled WRITE setPreviewEnabled NOTIFY previewEnabledChanged)
    Q_PROPERTY(bool dndEnabled READ dndEnabled WRITE setDndEnabled NOTIFY dndEnabledChanged)
    Q_PROPERTY(QString screenCorner READ screenCorner WRITE setScreenCorner NOTIFY screenCornerChanged)
    Q_PROPERTY(int screenAvailableWidth READ screenAvailableWidth NOTIFY screenGeometryChanged)
    Q_PROPERTY(int screenAvailableHeight READ screenAvailableHeight NOTIFY screenGeometryChanged)
    Q_PROPERTY(int screenAvailableX READ screenAvailableX NOTIFY screenGeometryChanged)
    Q_PROPERTY(int screenAvailableY READ screenAvailableY NOTIFY screenGeometryChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)
    Q_PROPERTY(QVariantList activeNotifications READ activeNotifications NOTIFY activeNotificationsChanged)

public:
    explicit NotificationManager(QObject *parent = nullptr);
    ~NotificationManager() override = default;

    bool notificationsEnabled() const { return m_notificationsEnabled; }
    bool soundEnabled() const { return m_soundEnabled; }
    bool previewEnabled() const { return m_previewEnabled; }
    bool dndEnabled() const { return m_dndEnabled; }
    QString screenCorner() const { return m_screenCorner; }
    int screenAvailableWidth() const;
    int screenAvailableHeight() const;
    int screenAvailableX() const;
    int screenAvailableY() const;
    int unreadCount() const { return m_unreadCount; }
    QVariantList activeNotifications() const;

    Q_INVOKABLE void showNotification(const QString &title,
                                     const QString &body,
                                     const QString &type = "message",
                                     const QString &channel = "",
                                     const QString &avatar = "",
                                     int durationMs = 4500);

    Q_INVOKABLE void showMessageNotification(const QString &sender,
                                            const QString &text,
                                            const QString &channel = "",
                                            const QString &avatar = "",
                                            const QString &messageType = "text");

    Q_INVOKABLE void dismissNotification(const QString &id);
    Q_INVOKABLE void clearAll();
    Q_INVOKABLE void playNotificationSound();

    Q_INVOKABLE void setNotificationsEnabled(bool enabled);
    Q_INVOKABLE void setSoundEnabled(bool enabled);
    Q_INVOKABLE void setPreviewEnabled(bool enabled);
    Q_INVOKABLE void setDndEnabled(bool enabled);
    Q_INVOKABLE void setScreenCorner(const QString &corner);
    Q_INVOKABLE void markChannelAsRead(const QString &channel);
    Q_INVOKABLE void resetUnreadCount();
    Q_INVOKABLE void setupFramelessTransparentWindow(QObject *window);

signals:
    void notificationTriggered(const QVariantMap &notification);
    void notificationDismissed(const QString &id);
    void notificationsCleared();
    void notificationsEnabledChanged();
    void soundEnabledChanged();
    void previewEnabledChanged();
    void dndEnabledChanged();
    void screenCornerChanged();
    void screenGeometryChanged();
    void unreadCountChanged();
    void activeNotificationsChanged();

private:
    void loadSettings();
    void setEnabled(bool enabled);
public:
    void setupMessageServiceHook(Services::MessageService* ms);
private:
    void saveSettings();

    mutable std::mutex m_mutex;
    bool m_notificationsEnabled{true};
    bool m_soundEnabled{true};
    bool m_previewEnabled{true};
    bool m_dndEnabled{false};
    QString m_screenCorner{"bottom-right"};
    int m_unreadCount{0};
    QVariantList m_activeNotifications;
};

} // namespace Core
} // namespace NeoNect
