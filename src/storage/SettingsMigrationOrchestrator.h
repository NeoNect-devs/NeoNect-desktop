// src/storage/SettingsMigrationOrchestrator.h
#pragma once
#include <QString>
#include <QSettings>

namespace NeoNect {
namespace Storage {

class SettingsMigrationOrchestrator {
public:
    /**
     * @brief Migrates settings from global profile scope to isolated user/server scope.
     * @param profile The current profile name.
     * @param serverUrl The authenticated server URL.
     * @param username The authenticated username.
     * @param isolated The isolated QSettings instance (destination).
     * @return true if migration completed or was already completed, false on error.
     */
    static bool migrateSettingsIfNecessary(const QString& profile, const QString& serverUrl, const QString& username, QSettings& isolated);
};

} // namespace Storage
} // namespace NeoNect

