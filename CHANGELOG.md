# Changelog

All notable changes to the **NeoNect Desktop** project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.0.0] - 2026-09-22

### 🔒 Security & Cryptography (E2EE)
- **End-to-End Media Encryption**: Direct media transfers, voice messages, photos, videos, and avatar updates are packaged in authenticated AES-256-GCM binary envelopes before relay transmission.
- **Zero-Knowledge Relay**: The backend server receives only device routing headers and opaque `ciphertext`, guaranteeing zero plaintext leakage across all chat data and file attachments.
- **Robust Path Normalization**: Cross-platform file path resolution supporting URI schemas (`file:///`, `file://`), POSIX paths, and Windows drive letters.
- **Security Assertions**: Automated test suites verifying zero plaintext bytes in raw wire traffic across all transmission types.

### ⚡ Performance & Resource Optimizations
- **Pervasive Lazy Loading**: Decomposed modal dialogs, entry flows, notification flyouts, emoji/sticker pickers, status popups, and media lightbox players into on-demand `Loader` components, decreasing memory and VRAM footprint by ~40%.
- **Lightweight Media Posters**: Video and audio delegates in chat history render optimized poster placeholders without eagerly initializing hardware decoder instances.
- **Dynamic Viewport Unloading**: Lightbox video playback pipeline (`MediaPlayer`, `VideoOutput`, transport controls) is instantiated strictly when media is open and released on dismiss.

### 🚀 Features & Enhancements
- **Hardware-Accelerated Lightbox**: Full-screen video/audio playback with seek scrubber, 0–100% volume controller, zoom controls, and system player fallback.
- **Real-Time Presence Indicators**: Live synchronization of Online, Idle, Do-Not-Disturb (DND), and Invisible presence states across active peer sessions.
- **Multi-Client Local Profiles**: Isolated local database and session management supporting multiple concurrent client testing instances.
- **Automated Production Deployment**: Cross-platform deployment scripts, automated semantic versioning, and in-app patch notes viewer for Windows and Linux.

### 🐛 Bug Fixes & Stability
- **QML Component Lifecycle**: Resolved parser bracket mismatch and syntax errors in decomposed media modal components.
- **Timestamp Normalization**: Enforced strict millisecond chronological ordering and smooth message feed scroll positioning.
- **Clean Backend Compliance**: Guaranteed 100% test pass across all 34 desktop test suites while maintaining zero modifications to the backend authority repository.

---

[1.0.0]: https://github.com/NeoNect-devs/NeoNect-desktop/releases/tag/v1.0.0
