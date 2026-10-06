// src/storage/SettingsMigrationOrchestrator.cpp
#include "SettingsMigrationOrchestrator.h"
#include "../common/constants.h"
#include <QVariant>
#include <QDir>
#include <QDebug>

namespace NeoNect {
namespace Storage {

bool SettingsMigrationOrchestrator::migrateSettingsIfNecessary(const QString& profile, const QString& serverUrl, const QString& username, QSettings& isolated) {
    if (isolated.value("migration_complete", false).toBool()) {
        return true;
    }

    QString groupName = profile.isEmpty() ? Constants::DEFAULT_PROFILE_GROUP : QString("%1_%2").arg(Constants::DEFAULT_PROFILE_GROUP, profile);
    QSettings legacy(Constants::SETTINGS_ROOT_GROUP, groupName);

    QString user = username.trimmed().toLower();
    if (user.isEmpty()) return false;


    auto migrateKey = [&](const QString& baseKey, bool suffixed) {
        QString legacyKey = suffixed ? QString("%1_%2").arg(baseKey, user) : baseKey;
        if (legacy.contains(legacyKey)) {
            if (!isolated.contains(baseKey)) {
                isolated.setValue(baseKey, legacy.value(legacyKey));
            }
        }
    };


    migrateKey(Constants::KEY_DEVICE_ID, true);
    migrateKey(Constants::KEY_PUBLIC_KEY, true);
    migrateKey(Constants::KEY_DISPLAY_NAME, true);
    migrateKey(Constants::KEY_PEER_DISPLAY_NAMES, true);
    migrateKey(Constants::KEY_AVATAR_URL, true);
    migrateKey(Constants::KEY_PEER_AVATARS, true);
    migrateKey(Constants::KEY_OPEN_CONVERSATIONS, true);
    migrateKey(Constants::KEY_FRIENDS, true);
    migrateKey(Constants::KEY_PENDING_REQUESTS, true);
    migrateKey(Constants::KEY_BOOKMARKS, false);

    // Verify and sync
    isolated.setValue("migration_complete", true);
    isolated.sync();

    return isolated.status() == QSettings::NoError;
}

} // namespace Storage
} // namespace NeoNect
