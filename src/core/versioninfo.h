/**
 * @file versioninfo.h
 * @brief Application versioning, build telemetry, and changelog metadata provider for QML.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Exposes immutable build metadata, semantic version components (major, minor, patch, build number),
 * Git repository commit hashes, compilation timestamps, platform architecture, and parsed patch
 * notes to the QML presentation tier.
 *
 * @par Design Patterns:
 * - <b>Information Expert Pattern</b>: Centralizes application identity and release metadata.
 * - <b>Immutable Data Transfer Object (DTO)</b>: Exposes read-only `CONSTANT` Q_PROPERTY values to QML.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace NeoNect {
namespace Core {

/**
 * @class VersionInfo
 * @brief Exposes application version, Git commit telemetry, and changelog data to QML.
 */
class VersionInfo : public QObject {
    Q_OBJECT

    /** @brief Human-readable application name (e.g. `"NeoNect"`). */
    Q_PROPERTY(QString appName READ appName CONSTANT)
    /** @brief Semantic version string (e.g. `"1.2.0"`). */
    Q_PROPERTY(QString version READ version CONSTANT)
    /** @brief Full extended version string with commit hash (e.g. `"1.2.0-a1b2c3d"`). */
    Q_PROPERTY(QString fullVersion READ fullVersion CONSTANT)
    /** @brief Semantic major version number. */
    Q_PROPERTY(int majorVersion READ majorVersion CONSTANT)
    /** @brief Semantic minor version number. */
    Q_PROPERTY(int minorVersion READ minorVersion CONSTANT)
    /** @brief Semantic patch version number. */
    Q_PROPERTY(int patchVersion READ patchVersion CONSTANT)
    /** @brief Sequential CI build number. */
    Q_PROPERTY(int buildNumber READ buildNumber CONSTANT)
    /** @brief Git short commit SHA-1 hash. */
    Q_PROPERTY(QString gitHash READ gitHash CONSTANT)
    /** @brief Git branch name at compile time. */
    Q_PROPERTY(QString gitBranch READ gitBranch CONSTANT)
    /** @brief ISO 8601 build timestamp. */
    Q_PROPERTY(QString buildTimestamp READ buildTimestamp CONSTANT)
    /** @brief Target operating system platform and architecture (e.g. `"Windows x86_64"`). */
    Q_PROPERTY(QString platform READ platform CONSTANT)
    /** @brief Legal copyright string. */
    Q_PROPERTY(QString copyright READ copyright CONSTANT)
    /** @brief Official project URL or documentation website. */
    Q_PROPERTY(QString website READ website CONSTANT)
    /** @brief List of parsed structured patch note objects for UI rendering. */
    Q_PROPERTY(QVariantList patchNotes READ patchNotes CONSTANT)

public:
    /**
     * @brief Constructs the version metadata controller and loads embedded release notes.
     * @param parent Optional parent QObject for Qt tree ownership.
     */
    explicit VersionInfo(QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~VersionInfo() override = default;

    /** @brief Returns the application brand name. */
    QString appName() const;
    /** @brief Returns semantic version string. */
    QString version() const;
    /** @brief Returns full extended version string. */
    QString fullVersion() const;
    /** @brief Returns major version integer. */
    int majorVersion() const;
    /** @brief Returns minor version integer. */
    int minorVersion() const;
    /** @brief Returns patch version integer. */
    int patchVersion() const;
    /** @brief Returns CI build number. */
    int buildNumber() const;
    /** @brief Returns Git commit hash. */
    QString gitHash() const;
    /** @brief Returns Git branch name. */
    QString gitBranch() const;
    /** @brief Returns ISO timestamp of compilation. */
    QString buildTimestamp() const;
    /** @brief Returns target OS and CPU architecture. */
    QString platform() const;
    /** @brief Returns copyright notice. */
    QString copyright() const;
    /** @brief Returns official project website. */
    QString website() const;
    /** @brief Returns structured patch notes array. */
    QVariantList patchNotes() const;

    /**
     * @brief Formats the latest release notes into rich markdown text for display.
     * @return Markdown formatted release summary string.
     */
    Q_INVOKABLE QString formattedReleaseNotes() const;

private:
    /** @brief Parses embedded or resource-bundled patch notes JSON. */
    void loadPatchNotes();
    /** @brief Cached patch notes variant list. */
    QVariantList m_patchNotes;
};

} // namespace Core
} // namespace NeoNect
