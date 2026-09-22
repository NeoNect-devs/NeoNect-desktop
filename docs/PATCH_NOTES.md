# Patch Notes & Highlights {#patch_notes}

This document outlines the major patch highlights, security hardening updates, and architectural enhancements for all production releases of **NeoNect Desktop**.

---

## Version v1.0.0 (2026-09-22)

| Property | Specification |
| :--- | :--- |
| **Version** | `v1.0.0` |
| **Release Date** | 2026-09-22 |
| **Commit Hash** | `30a54e5` |
| **Platform Targets** | Windows x86_64 (MSVC 2022), Linux x86_64 (GCC 11+) |
| **Qt Framework** | Qt 6.5+ LTS / 6.8+ |
| **Cryptography Engine** | OpenSSL 3.x / 4.x (AES-256-GCM + PBKDF2) |

### Key Release Highlights

* ✔️ ensure end-to-end encryption for all media transmissions (`017baea`)
* ✔️ implement automated semantic versioning, patch notes engine, and cross-platform production packaging (`0219f30`)
* ✔️ lazy load fullscreen video and image components in MediaLightboxModal (`333038a`)
* ✔️ fix Windows test runner & windeployqt packaging, optimize scripts (`2ca5e2d`)
* ✔️ implement pervasive UI lazy loading, reducing memory usage by approx. 40% (`53a36b3`)
* ✔️ prevent windows.h min/max macro collision on MSVC with NOMINMAX (`30a54e5`)
* ✔️ make Windows post-build DLL copy dynamic and remove hardcoded paths (`a565360`)
* ✔️ resolve missing closing brace syntax error in modern-graph-viewer.js (`4c54975`)
* ✔️ resolve namespace duplication and add descriptions for all namespaces (`2d25222`)
* ✔️ register friendship via POST /api/v1/friends and prevent false 401 session resets on 403 forbidden relay errors (`f2e7ba1`)
* ✔️ include required protocol_version in relay payload and fix unit test setup (`c68a6f3`)
* ✔️ synchronize friend retrieval with backend api and fix settings logout flow (`ce44ccf`)

### Security Advisories & Hardening

- **Cryptographic Nonces**: Guaranteed 96-bit CSPRNG unique nonces for every single `AES-256-GCM` payload.
- **Memory Safety**: Strict RAII encapsulation for all OpenSSL EVP contexts.
- **Local Storage**: Hardware-derived machine-key encrypted SQLite credential vault.

---

### Historical Patch Notes Archive

* 📜 For full commit-level tracking, refer to the **@subpage changelog "Comprehensive Changelog"**.
