# NeoNect Desktop Client Architecture & Engineering Specification {#mainpage}

Welcome to the internal engineering documentation of the **NeoNect Desktop Client**. This system is an enterprise-grade, end-to-end encrypted (E2EE) real-time communication platform engineered with modern C++17, Qt 6.5+, OpenSSL 3.x/4.x, and SQLite.

---

## Documentation Manuals & Guides

* 🚀 **@subpage getting_started "Getting Started Guide"**: Account registration, session vault setup, and initial communication workflows.
* 📦 **@subpage installation "Installation & Platform Setup"**: Binary distributions, system dependencies, and audio driver configurations.
* 🛠️ **@subpage building_guide "Compiling, Testing & Docs Guide"**: CMake build commands, automated test suites execution, and benchmarking.
* 🐳 **@subpage docker_guide "Docker Deployment & Web Streaming"**: Containerized deployment with virtual display rendering (`Xvfb`) and HTML5 `noVNC` streaming.
* 🌐 **@subpage documentation_guide "Documentation Ecosystem"**: Doxygen architecture, interactive graph viewer, and automated GitHub Pages static hosting.

---

## Application Showcase

| Security Vault & Authentication | Real-Time E2EE Chat & Waveforms |
| :---: | :---: |
| ![Security Vault](demos/entry_showcase.png) | ![Real-Time Chat View](demos/chat_showcase.png) |

---

## 1. System Overview & Core Philosophy

The NeoNect Desktop Client is engineered under strict zero-trust principles:
1. **End-to-End Encryption (E2EE)**: All peer-to-peer payloads (direct text messages, presence broadcasts, audio recordings, images, media transfers) are encrypted using authenticated symmetric cryptography (`AES-256-GCM`) before transmission. The central server acts solely as a blind relay.
2. **Account & Key Isolation**: Every user account maintains an independent, isolated database and localized encryption keys. Stored credentials and tokens are encrypted at rest with hardware-derived AES-GCM machine keys.
3. **Decoupled Layered Architecture**: Clear separation of concerns between declarative QML presentation, stateful facade managers, domain services, infrastructure repositories, and low-level cryptographic primitives.

---

## 2. Architectural Layers

The client is decomposed into seven primary architectural layers, visualized in the graph below:

<div class="modern-graph-card">
    <div class="graph-toolbar">
        <div class="graph-title">
            <span class="graph-icon">🏛️</span>
            <span>Interactive Architectural Topology</span>
        </div>
        <div class="graph-controls">
            <span id="btnZoomIn" class="graph-btn" role="button" tabindex="0" title="Zoom In">➕ Zoom In</span>
            <span id="btnZoomOut" class="graph-btn" role="button" tabindex="0" title="Zoom Out">➖ Zoom Out</span>
            <span id="btnReset" class="graph-btn" role="button" tabindex="0" title="Reset View">↺ Reset</span>
            <span id="btnFullscreen" class="graph-btn" role="button" tabindex="0" title="Toggle Fullscreen">⛶ Fullscreen</span>
        </div>
    </div>
    <div id="graphViewport" class="graph-viewport">
        <div id="architectureMermaid" class="mermaid"></div>
    </div>
    <div class="graph-footer">
        <span class="graph-tip">💡 <b>Interactive Topology:</b> Click &amp; drag to pan • Mouse wheel to zoom • Touch pinch/pan on mobile • Toggle light/dark theme anytime.</span>
    </div>
</div>

### Layer Responsibilities
- **Presentation Layer (QML / QtQuick)**: Purely declarative, reactive UI written in QML, adhering to Discord/Telegram-inspired high-productivity aesthetics with fluid animations, virtualized list views, and responsive layouting.
- **Facade Layer (`src/core`)**: Mediates between Qt Quick QML context and C++ domain services. Exposes thread-safe Q_PROPERTY bindings, Q_INVOKABLE methods, and Qt signals.
- **Application Controller (`NeoNect::Application`)**: The single composition root responsible for dependency injection, platform window initialization (Windows 11 DWM snap/corners), CLI parsing, and dynamic user switching.
- **Services Layer (`src/services`)**: Encapsulates business logic, including authentication flows, device provisioning and pruning, friend request state transitions, relay transmission failover, and message delivery receipts.
- **Domain Layer (`src/domain`)**: Immutable entities and value objects (such as `NeoNect::Domain::Message`) encapsulating domain invariants.
- **Storage Layer (`src/storage`)**: Abstract repository pattern implementations for encrypted user preferences (`SettingsRepository`) and relational SQLite message storage (`SqlMessageRepository`).
- **Transport Layer (`src/transport`)**: Abstractions over HTTP/REST (`HttpTransport`) and full-duplex WebSocket streaming (`WebSocketClient`).
- **Cryptography Layer (`src/crypto`)**: OpenSSL-backed cryptographic service implementing authenticated encryption, hashing, CSPRNG, and RAII handle safety.

---

## 3. Comprehensive Design Patterns Catalog

NeoNect systematically implements classical Gang of Four (GoF) and enterprise software architecture patterns:

| Design Pattern | Component / Class | Architectural Purpose |
| :--- | :--- | :--- |
| **Application Controller** | `NeoNect::Application` | Coordinates system bootstrap, lifecycle, argument parsing, and window subsystem initialization. |
| **Composition Root / Dependency Injection** | `NeoNect::Application::initializeServices()` | Explicitly instantiates and wires together repositories, crypto engines, transports, and services without global singletons. |
| **Facade Pattern** | `NetworkManager`, `CryptoManager`, `AudioManager` | Presents a unified, high-level API to QML, masking the internal orchestration of multiple backend services. |
| **Repository Pattern** | `ISettingsRepository`, `IMessageRepository`, `SettingsRepository`, `SqlMessageRepository` | Decouples business logic from persistence mechanisms (QSettings vs. SQLite), allowing clean mocking in unit tests. |
| **Decorator / Encrypted Storage** | `SettingsRepository` | Transparently encrypts sensitive values (tokens, keys) with machine-unique keys prior to persistence. |
| **Strategy Pattern** | `ICryptoService`, `CryptoService` | Encapsulates cryptographic algorithm implementations behind an interface, enabling algorithm rotation or mock injection. |
| **Interface Segregation (ISP)** | `IHttpTransport`, `ISettingsRepository`, `IMessageRepository`, `ICryptoService` | Pure virtual interfaces ensure modules only depend on methods they strictly require. |
| **Adapter Pattern** | `HttpTransport`, `AudioManager` | Adapts low-level Qt APIs (`QNetworkAccessManager`, `QMediaRecorder`, `QAudioSink`) to clean domain contracts. |
| **Observer / Pub-Sub** | `RelayService`, `NotificationManager`, Qt Signals & Slots | Decouples event producers (incoming relay packets, network disconnects) from event consumers. |
| **Mediator Pattern** | `NetworkManager`, `Application` | Mediates cross-service coordination (e.g. `RelayService` forwarding friend packets to `FriendService`). |
| **Circuit Breaker / Fallback** | `RelayService` | Implements automated failover between primary WebSocket real-time relay and HTTP long-polling fallback. |
| **Active Object** | `WebSocketClient`, `SqlMessageRepository` | Decouples method invocation from background execution via dedicated `QThread` and event loops. |
| **Resource Acquisition Is Initialization (RAII)** | `OpenSSL_RAII.h` (`EvpCipherCtxPtr`, `BioPtr`, etc.) | Automatically cleans up C-style OpenSSL heap allocations via custom `std::unique_ptr` deleters, eliminating leaks. |
| **Model-View Architecture (MVC)** | `ChatMessageModel` | Custom `QAbstractListModel` providing O(1) virtualized viewport list data to QML ListView with reverse pagination. |
| **Thread-Safe Monitor** | `SettingsRepository`, `CryptoService`, `HttpTransport` | Uses `std::mutex` and `std::lock_guard` to synchronize concurrent multi-threaded access. |

---

## 4. End-to-End Encryption (E2EE) & Security Model

### Cryptographic Primitives
- **Symmetric Cipher**: `AES-256-GCM` (Galois/Counter Mode)
  - Key Length: 256 bits (32 bytes)
  - Initialization Vector (IV / Nonce): 96 bits (12 bytes) generated via CSPRNG (`RAND_bytes`)
  - Authentication Tag: 128 bits (16 bytes) verifying ciphertext authenticity and integrity
- **Key Derivation & Hashing**:
  - `SHA-256` for integrity hashes, message fingerprinting, and account hash identifiers.
- **Machine-Bound Storage Key**:
  - Derived from `QSysInfo::machineUniqueId()` mixed with application salt via SHA-256, protecting tokens stored in local registries/settings from external process tampering.

### E2EE Message Transmission Flow
1. **Serialization**: Domain message is serialized to JSON including text, media metadata, avatar payloads, or waveform vectors.
2. **Encryption**: `CryptoService::encryptAesGcm` encrypts plaintext bytes with recipient's shared symmetric secret or prekey.
3. **Envelope Packaging**: Ciphertext, IV, and auth tag are combined into a binary envelope and base64 encoded.
4. **Relay Transport**: Sent via `POST /api/v1/relay/send` with `protocol_version: 1` or `2`. The server routes the envelope blindly to the recipient's registered device mailbox without possessing decryption keys.
5. **Decryption & Verification**: Recipient pulls envelope via WebSocket or `GET /api/v1/relay/poll`, validates tag integrity, and decrypts to plaintext.

---

## 5. Technical Constraints, Cryptographic Invariants & Operational Bounds

This section provides explicit specifications for all mathematical bounds, preconditions, postconditions, and concurrency rules across the system.

### 5.1 Cryptographic Invariants & Operational Bounds

| Parameter | Specification / Constraint | Rationale / Standard |
| :--- | :--- | :--- |
| **Symmetric Algorithm** | `AES-256-GCM` (`EVP_aes_256_gcm()`) | NIST SP 800-38D authenticated symmetric encryption. |
| **Symmetric Key Size** | Exactly 32 bytes (256 bits) | Cryptographic security floor; rejected if size != 32. |
| **IV / Nonce Length** | Exactly 12 bytes (96 bits) | Optimal for Galois Field 128-bit authentication without GHASH pre-processing. |
| **Zero Nonce-Reuse Invariant** | Must NEVER encrypt two messages with same (Key, IV) | GCM nonce reuse catastrophically compromises the GHASH authentication key (H). |
| **Authentication Tag** | Exactly 16 bytes (128 bits) | Verifies ciphertext integrity and AAD authenticity. Tag mismatches immediately abort with zeroed output. |
| **AAD Bounds** | Bounded to <= 65,536 bytes (64 KB) | Prevents denial-of-service via oversized unencrypted envelope headers. |
| **PBKDF2 Derivation** | HMAC-SHA-256, >= 100,000 iterations | NIST SP 800-132 password-based key derivation guidelines against ASIC/GPU cracking. |
| **Salt Length** | >= 16 bytes CSPRNG-generated | Precludes rainbow table and precomputation attacks. |
| **Sensitive Memory Hygiene** | `OPENSSL_cleanse()` on deallocation | Guarantees plaintext keys and credentials are not left in paging files or memory core dumps. |

### 5.2 Concurrency & Threading Constraints

The client enforces a multi-threaded architecture with strict thread-affinity rules:

```
┌──────────────────────────────────────────────────────────────────┐
│                   GUI Thread (Main Qt Event Loop)                │
│  - QML SceneGraph rendering, animations, user input events       │
│  - Presentation Facades (NetworkManager, NotificationManager)    │
│  - QAbstractListModel UI synchronization (ChatMessageModel)      │
└────────────────┬────────────────────────────────┬────────────────┘
                 │ (QMetaObject::invokeMethod)    │ (Qt Signals & Slots)
                 ▼                                ▼
┌────────────────────────────────┐  ┌───────────────────────────────┐
│     Storage Worker Thread      │  │      Audio Capture Thread     │
│  - Dedicated QThread           │  │  - Low-latency waveIn worker  │
│  - SQLite queries & commits    │  │  - 16 kHz PCM ring buffer     │
│  - Zero GUI thread disk blocks │  │  - Thread-safe amplitude DSP  │
└────────────────────────────────┘  └───────────────────────────────┘
```

1. **Main GUI Thread Exclusivity**:
   - `QQmlApplicationEngine`, visual QQuick items, and scene graph allocations must exist exclusively on the main process thread.
   - Any background worker returning data to UI controls must marshal calls via Qt signals or `QPointer<QObject>` lifetime guards.
2. **Active Object SQLite Concurrency**:
   - `SqlMessageRepository` delegates all query compilation and execution to `SqlMessageRepositoryWorker` on a separate `QThread`.
   - SQLite connections are initialized with `PRAGMA journal_mode = WAL;` and `PRAGMA synchronous = NORMAL;`, allowing simultaneous readers and single-writer concurrency without lock escalation timeouts.
3. **Audio Capture & Playback Isolation**:
   - Audio recording (`waveIn`) runs on an independent `std::thread`, communicating via atomic flags (`m_recordThreadRunning`) and mutexes.
   - Playback (`waveOut` / MCI) streams PCM audio chunks through dedicated worker loops, preventing UI stutter during high-framerate interactions.
4. **Thread-Safe Monitors**:
   - `SettingsRepository` guards configuration caching via `std::mutex`.
   - `CryptoService` reader-writer lock allows concurrent multi-threaded encryptions via `std::shared_lock` and exclusive key mutations via `std::unique_lock`.

### 5.3 Network & Transport Protocol Constraints

1. **WebSocket Protocol (RFC 6455)**:
   - **Client Masking**: All client-to-server frames MUST have the `MASK` bit set (`1`) and provide a cryptographically uniform 4-byte masking key (`QRandomGenerator::global()->generate()`).
   - **Inbound Validation**: Unsolicited masked frames from the relay server violate RFC 6455 Section 5.1 and trigger immediate connection termination.
   - **Frame Bounds**: Text payload size is capped at 64 KB (65,536 bytes) per frame. Control frames (Ping/Pong/Close) are bounded to <= 125 bytes.
   - **Reconnection Backoff**: Reconnect interval begins at 1,000 ms, doubles exponentially (2^n * 1000 ms), and is strictly capped at 30,000 ms (30 seconds).
2. **HTTP / REST Transport**:
   - Enforces TLS 1.3 / 1.2 in production environments. Cleartext HTTP is restricted to `localhost` and `127.0.0.1` development profiles.
   - Timeouts: Connection timeout 10 seconds; request completion timeout 30 seconds.
   - Request Body Limits: Outgoing JSON request bodies are capped at 10 MB (10,485,760 bytes).

### 5.4 Data Storage & Entity Constraints

1. **Message Entities (`Domain::Message`)**:
   - `id`: Non-empty canonical UUID v4 string (36 characters).
   - `timestamp`: Monotonically increasing Unix epoch milliseconds (t > 0).
   - `status`: Lifecycle transitions follow strict sequence: `Pending -> Sending -> Sent -> Delivered -> Read` (or `Failed`).
2. **Pagination & Query Limits**:
   - Message fetch pagination size: limit in range [1, 1000].
   - Batch insert operations are transactionally committed up to 5,000 messages per `BEGIN IMMEDIATE ... COMMIT` block.
3. **Profile Isolation**:
   - User profile paths are isolated under `%LOCALAPPDATA%/NeoNect/<profile>/`.
   - Profile names are alphanumeric strings restricted to [1, 64] characters without path traversal characters (`/`, `\`, `..`).

---

## 6. Directory Structure

```
NeoNect-desktop/
├── assets/                  # High-resolution icons, SVG graphics, stickers, and sound effects
│   ├── NeoNect/             # Official branding logo and application icon
│   └── icons/               # Action icons (feather / material SVG)
├── cmake/                   # Custom CMake modules (Version.cmake, packaging scripts)
├── docs/                    # Architecture guides, Doxygen specifications, and diagrams
├── qml/                     # Declarative QtQuick UI components, views, dialogs, and themes
├── src/
│   ├── common/              # System constants, common types, OpenSSL RAII wrappers
│   ├── core/                # Application bootstrap, facades (NetworkManager, CryptoManager), models
│   ├── crypto/              # OpenSSL 3.x/4.x cryptographic service implementation
│   ├── domain/              # Pure domain entities (Message, Attachment)
│   ├── services/            # Business services (Auth, Device, Friend, Message, Relay)
│   ├── storage/             # Repositories (Encrypted Settings, SQLite Message Database)
│   └── transport/           # Network transports (HTTP/REST, WebSockets)
├── tests/                   # Automated unit tests, integration suites, and mocks
└── Doxyfile                 # Doxygen configuration
```
