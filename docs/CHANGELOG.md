# Changelog & Release Notes {#changelog}

All notable changes to the **NeoNect Desktop Client** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- Group chat support with MLS (Messaging Layer Security) E2EE tree protocol.
- WebRTC peer-to-peer live voice and video calling.
- Hardware key token (YubiKey / PKCS#11) hardware vault integration.

---

## [v1.0.0] - 2026-09-22

### 🚀 Major Production Release

This marks the official production release of the **NeoNect Desktop Client**, delivering an enterprise-grade, end-to-end encrypted (E2EE) real-time communication platform.

#### Added
- **Authentication & Security Vault**:
  - `PBKDF2-HMAC-SHA-256` key derivation with $\ge 100,000$ iterations and CSPRNG salt generation.
  - Hardware-derived `AES-256-GCM` machine key encryption for token and credential persistence at rest.
  - Multi-account profile management with isolated SQLite database partitions.
- **Real-Time Messaging & Duplex Transport**:
  - Asynchronous dual-transport engine: HTTP REST for idempotent resource management and WebSocket for zero-latency duplex streaming.
  - Automatic exponential-backoff reconnection with frame sequencing and heartbeat keep-alive pings.
  - Instant direct messages, delivery status, and seen receipts (`readAt` timestamps).
- **End-to-End Encrypted Media & Voice**:
  - Live audio recording with interactive waveform visualization.
  - Media attachment encryption with SHA-256 integrity checksums and explicit user acceptance workflows.
- **User Presence & Social Graph**:
  - Live presence state machine: `Online`, `Idle`, `Do Not Disturb (DND)`, and `Invisible/Offline`.
  - Cryptographic friend request negotiation with mutual peer public key verification.
  - Open conversations sidebar sorted by last activity with real-time unread badges.
  - User profile customization: Custom avatars with client-side scaling/caching and display name synchronization.
- **Docker & Containerized Streaming**:
  - Multi-stage Docker image with headless `Xvfb` virtual display (`1280x800x24`), `Fluxbox` window manager, `x11vnc`, and `noVNC` web streaming on port 6080 (`http://localhost:6080`).
  - Docker Compose topology for running isolated dual-client peer networks locally.
- **Documentation Engine & GitHub Pages**:
  - Doxygen 1.13 API documentation suite with `doxygen-awesome-css` theme.
  - Interactive Mermaid SVG architecture topology viewer with mouse-wheel zoom, drag-to-pan, and 2-finger touch controls.
  - Automated GitHub Actions deployment pipeline for GitHub Pages static hosting on tagged version releases.

#### Changed
- Migrated all UI components to Qt 6.5+ QtQuick Controls with custom modern responsive dark and light themes.
- Standardized project licensing under the **Apache License 2.0**.

#### Security & Compliance
- Strict zero-nonce-reuse guarantees for all `AES-256-GCM` operations using 96-bit CSPRNG nonces.
- OpenSSL 3.x/4.x RAII wrappers (`EVP_CIPHER_CTX_free`, `EVP_PKEY_free`) eliminating cryptographic memory leaks.
- SQLite WAL (Write-Ahead Logging) mode with `NORMAL` synchronous mode and 5000ms busy timeouts for robust crash resistance.

---

## [v0.9.0] - 2026-09-15

### 🧪 Pre-Release & Protocol Hardening

#### Added
- Full test suite integration covering Cryptography, Storage, Models, Network Services, and UI Facades.
- Stress testing harnesses (`NeoNectStressTests`) simulating concurrent socket reconnects and rapid-fire messaging.
- High-throughput microbenchmarks (`NeoNectBenchmarks`) measuring PBKDF2 hashing latency and SQLite batch write throughput.

#### Fixed
- Fixed session token persistence bug where old authentication tokens were not purged during account switching.
- Resolved memory leak in websocket frame decoding during high-volume payload bursts.
- Fixed UI layout clipping on high-DPI displays under Windows scaling (125% - 200%).

---

## [v0.5.0] - 2026-08-01

### 🛠️ Alpha Core & Architectural Prototype

#### Added
- Initial C++17 project scaffolding with CMake build system.
- Basic QML UI mockup with Discord/Telegram-inspired layout.
- Prototype OpenSSL 3.x crypto wrapper for AES-256-GCM and SHA-256.
- SQLite message storage prototype.
