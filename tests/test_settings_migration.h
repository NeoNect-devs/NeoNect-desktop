#pragma once
#include <QObject>

class TestSettingsMigration : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testUnauthenticatedState();
    void testLegacySettingsDetectionAndMigration();
    void testMigrationIsIdempotent();
    void testExistingDestinationNotOverwritten();
    void testFailedMigrationDoesNotDeleteSource();
    void testUserIsolation();
    void testServerIsolation();
    void testE2EEStorageUntouched();
    void testGlobalFallbackSafety();
    void testCompleteMigrationCoverage();
};

