#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace NeoNect {
namespace Core {

class VersionInfo : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString appName READ appName CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString fullVersion READ fullVersion CONSTANT)
    Q_PROPERTY(int majorVersion READ majorVersion CONSTANT)
    Q_PROPERTY(int minorVersion READ minorVersion CONSTANT)
    Q_PROPERTY(int patchVersion READ patchVersion CONSTANT)
    Q_PROPERTY(int buildNumber READ buildNumber CONSTANT)
    Q_PROPERTY(QString gitHash READ gitHash CONSTANT)
    Q_PROPERTY(QString gitBranch READ gitBranch CONSTANT)
    Q_PROPERTY(QString buildTimestamp READ buildTimestamp CONSTANT)
    Q_PROPERTY(QString platform READ platform CONSTANT)
    Q_PROPERTY(QString copyright READ copyright CONSTANT)
    Q_PROPERTY(QString website READ website CONSTANT)
    Q_PROPERTY(QVariantList patchNotes READ patchNotes CONSTANT)

public:
    explicit VersionInfo(QObject *parent = nullptr);
    ~VersionInfo() override = default;

    QString appName() const;
    QString version() const;
    QString fullVersion() const;
    int majorVersion() const;
    int minorVersion() const;
    int patchVersion() const;
    int buildNumber() const;
    QString gitHash() const;
    QString gitBranch() const;
    QString buildTimestamp() const;
    QString platform() const;
    QString copyright() const;
    QString website() const;
    QVariantList patchNotes() const;

    Q_INVOKABLE QString formattedReleaseNotes() const;

private:
    void loadPatchNotes();
    QVariantList m_patchNotes;
};

} // namespace Core
} // namespace NeoNect
