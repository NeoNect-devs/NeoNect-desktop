#include "versioninfo.h"
#include "version.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

namespace NeoNect {
namespace Core {

VersionInfo::VersionInfo(QObject *parent)
    : QObject(parent)
{
    loadPatchNotes();
}

QString VersionInfo::appName() const {
    return QStringLiteral(NEONECT_APP_NAME);
}

QString VersionInfo::version() const {
    return QStringLiteral(NEONECT_VERSION_STRING);
}

QString VersionInfo::fullVersion() const {
    return QStringLiteral(NEONECT_VERSION_FULL);
}

int VersionInfo::majorVersion() const {
    return NEONECT_VERSION_MAJOR;
}

int VersionInfo::minorVersion() const {
    return NEONECT_VERSION_MINOR;
}

int VersionInfo::patchVersion() const {
    return NEONECT_VERSION_PATCH;
}

int VersionInfo::buildNumber() const {
    return NEONECT_BUILD_NUMBER;
}

QString VersionInfo::gitHash() const {
    return QStringLiteral(NEONECT_GIT_HASH);
}

QString VersionInfo::gitBranch() const {
    return QStringLiteral(NEONECT_GIT_BRANCH);
}

QString VersionInfo::buildTimestamp() const {
    return QStringLiteral(NEONECT_BUILD_TIMESTAMP);
}

QString VersionInfo::platform() const {
    return QStringLiteral(NEONECT_PLATFORM);
}

QString VersionInfo::copyright() const {
    return QStringLiteral(NEONECT_COPYRIGHT);
}

QString VersionInfo::website() const {
    return QStringLiteral(NEONECT_WEBSITE);
}

QVariantList VersionInfo::patchNotes() const {
    return m_patchNotes;
}

void VersionInfo::loadPatchNotes() {
    m_patchNotes.clear();

    QFile file(":/qt/qml/NeoNect/assets/patch_notes.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // Fallback default patch notes if JSON asset is not yet bundled
        QVariantMap defaultRelease;
        defaultRelease["version"] = version();
        defaultRelease["date"] = buildTimestamp().left(10);
        defaultRelease["title"] = "Production Release";
        QVariantList notes;
        notes.append("End-to-End Encryption (AES-256-GCM) across all messages, audio notes, media files, and avatars.");
        notes.append("Pervasive UI lazy loading reducing client RAM/VRAM footprint by ~40%.");
        notes.append("Real-time presence state synchronization (Online, Idle, DND, Invisible).");
        notes.append("Hardware-accelerated media lightbox with zoom, seek, and multi-format playback.");
        defaultRelease["highlights"] = notes;
        m_patchNotes.append(defaultRelease);
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isArray()) {
        m_patchNotes = doc.array().toVariantList();
    } else if (doc.isObject() && doc.object().contains("releases")) {
        m_patchNotes = doc.object().value("releases").toArray().toVariantList();
    }
}

QString VersionInfo::formattedReleaseNotes() const {
    QString out;
    out += QString("### NeoNect v%1 (%2)\n").arg(version(), gitHash());
    out += QString("**Build Date**: %1 | **Platform**: %2\n\n").arg(buildTimestamp(), platform());

    for (const auto &relVal : m_patchNotes) {
        QVariantMap rel = relVal.toMap();
        out += QString("#### Version %1 (%2)\n").arg(rel.value("version").toString(), rel.value("date").toString());
        QVariantList highlights = rel.value("highlights").toList();
        for (const auto &item : highlights) {
            out += QString("- %1\n").arg(item.toString());
        }
        out += "\n";
    }
    return out;
}

} // namespace Core
} // namespace NeoNect
