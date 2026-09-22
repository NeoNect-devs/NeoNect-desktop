# NeoNect Desktop Client Architecture & Engineering Specification {#mainpage}

![NeoNect Logo](icon.png)

Welcome to the internal engineering documentation of the **NeoNect Desktop Client**. This system is an enterprise-grade, end-to-end encrypted (E2EE) real-time communication platform engineered with modern C++17, Qt 6.5+, OpenSSL 3.x/4.x, and SQLite.

---

## 1. System Overview & Core Philosophy

The NeoNect Desktop Client is engineered under strict zero-trust principles:
1. **End-to-End Encryption (E2EE)**: All peer-to-peer payloads (direct text messages, presence broadcasts, audio recordings, images, media transfers) are encrypted using authenticated symmetric cryptography (`AES-256-GCM`) before transmission. The central server acts solely as a blind relay.
2. **Account & Key Isolation**: Every user account maintains an independent, isolated database and localized encryption keys. Stored credentials and tokens are encrypted at rest with hardware-derived AES-GCM machine keys.
3. **Decoupled Layered Architecture**: Clear separation of concerns between declarative QML presentation, stateful facade managers, domain services, infrastructure repositories, and low-level cryptographic primitives.

---

## 2. Architectural Layers

The client is decomposed into seven primary architectural layers:

```
+-----------------------------------------------------------------------+
|                       PRESENTATION LAYER (QML)                        |
|   Main.qml, ChatView.qml, FriendsHomePanel.qml, MediaLightboxModal    |
+-----------------------------------------------------------------------+
                                  |
                                  v
+-----------------------------------------------------------------------+
|                    FACADE & ADAPTER LAYER (Core)                      |
|      NetworkManager, CryptoManager, AudioManager, ChatMessageModel     |
+-----------------------------------------------------------------------+
                                  |
                                  v
+-----------------------------------------------------------------------+
|                         APPLICATION CONTROLLER                        |
|                   NeoNect::Application (Composition Root)             |
+-----------------------------------------------------------------------+
                                  |
            +---------------------+---------------------+
            |                                           |
            v                                           v
+-----------------------+                   +-----------------------+
|    SERVICES LAYER     |                   |     STORAGE LAYER     |
| - AuthService         |                   | - SettingsRepository  |
| - DeviceService       |                   | - SqlMessageRepository|
| - FriendService       |                   +-----------------------+
| - MessageService      |                               |
| - RelayService        |                               v
+-----------------------+                   +-----------------------+
            |                               |   CRYPTOGRAPHY LAYER  |
            v                               | - CryptoService       |
+-----------------------+                   | - OpenSSL RAII        |
|    TRANSPORT LAYER    |                   +-----------------------+
| - HttpTransport       |
| - WebSocketClient     |
+-----------------------+
```

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
| **Active Object** | `WebSocketClient` | Manages an autonomous background state machine handling heartbeat ping/pong and exponential backoff reconnects. |
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

## 5. Threading, Concurrency & Data Flow

- **Main Thread (GUI / QML Event Loop)**: Runs UI rendering, QML animations, user input events, and high-level facade dispatches.
- **Network I/O Thread (`QNetworkAccessManager` & Qt WebSockets)**: Non-blocking asynchronous event processing on internal Qt socket threads. Responses trigger asynchronous callbacks via Qt signal queue.
- **Worker & Storage Concurrency**: Relational database operations utilize SQLite transactions. Repositories and crypto services employ mutex locks (`std::mutex`, `std::recursive_mutex`) to guarantee thread safety across multi-threaded tests and background workers.

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
