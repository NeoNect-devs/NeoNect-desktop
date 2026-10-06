#pragma once
#include <QString>

namespace NeoNect {
namespace Storage {

/**
 * @brief Pure, deterministic, stateless path and identity resolver.
 * @details Generates cross-platform file paths and safe hash-based keys for the
 * NeoNect storage hierarchy, preventing filesystem injection and isolation collisions.
 */
class StoragePathResolver {
public:
    /**
     * @brief Resolves the canonical NeoNect application data root.
     * @return Absolute path to the app data location.
     */
    static QString appRoot();

    // =========================================================================
    // Canonical Normalization
    // =========================================================================
    static QString canonicalServerUrl(const QString& rawUrl);
    static QString canonicalUsername(const QString& rawUsername);
    static QString canonicalConversationId(const QString& rawConversationId);

    // =========================================================================
    // Identity Key Generation
    // =========================================================================
    static QString serverKey(const QString& serverUrl);
    static QString accountKey(const QString& username);
    static QString chatKey(const QString& conversationId);

    // =========================================================================
    // Hierarchy Roots
    // =========================================================================
    static QString serverRoot(const QString& serverUrl);
    static QString accountRoot(const QString& serverUrl, const QString& username);
    
    // =========================================================================
    // Account Directories
    // =========================================================================
    static QString accountDirectory(const QString& serverUrl, const QString& username);
    static QString e2eeDirectory(const QString& serverUrl, const QString& username);
    static QString settingsDirectory(const QString& serverUrl, const QString& username);
    static QString cacheDirectory(const QString& serverUrl, const QString& username);
    static QString chatsDirectory(const QString& serverUrl, const QString& username);
    static QString chatDirectory(const QString& serverUrl, const QString& username, const QString& conversationId);
    static QString chatAttachmentsDirectory(const QString& serverUrl, const QString& username, const QString& conversationId);

    // =========================================================================
    // File Paths
    // =========================================================================
    static QString messageDbPath(const QString& serverUrl, const QString& username);
    static QString e2eeDbPath(const QString& serverUrl, const QString& username);
    static QString accountMetadataPath(const QString& serverUrl, const QString& username);
    static QString chatMetadataPath(const QString& serverUrl, const QString& username, const QString& conversationId);
};

} // namespace Storage
} // namespace NeoNect
