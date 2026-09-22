# Getting Started with NeoNect Desktop {#getting_started}

Welcome to the **NeoNect Desktop Client** quick start guide. This manual walks you through launching the application, configuring your initial server connection, creating cryptographic identity keys, and managing real-time peer-to-peer secure communication.

---

## 1. System Requirements & Prerequisites

Before running the NeoNect client, verify your environment meets the minimum specifications:

| Requirement | Minimum Specification | Recommended |
| :--- | :--- | :--- |
| **Operating System** | Windows 10 (64-bit) / Ubuntu 22.04 LTS | Windows 11 (64-bit) / Ubuntu 24.04 LTS |
| **Processor (CPU)** | Dual-core 2.0 GHz x86_64 | Quad-core 3.0 GHz x86_64 or higher |
| **System Memory (RAM)** | 2 GB Available RAM | 4 GB+ RAM |
| **Graphics (GPU)** | OpenGL 3.3 / DirectX 11 compatible | Hardware GPU acceleration enabled |
| **Audio Hardware** | Microphone & Speakers (16 kHz PCM support) | Headset / High-fidelity microphone |
| **Network** | Stable Internet / LAN connection | Broadband with WebSocket support |

---

## 2. Launching the Application

### Running on Windows
Double-click `NeoNectApp.exe` from your release package directory, or launch via PowerShell:
```powershell
.\NeoNectApp.exe
```

### Running on Linux
Make the binary executable and launch from your terminal:
```bash
chmod +x NeoNectApp
./NeoNectApp
```

### Command-Line Arguments
NeoNect supports profile isolation and mock simulation modes:
* `-p <name>` or `--profile <name>`: Launches an isolated profile session with separate SQLite storage and configuration vaults (e.g. `./NeoNectApp -p Alice`).
* `--mock`: Runs the client in UI simulation mode for demonstrations without requiring a live backend server.

---

## 3. Account Creation & Security Setup

When you first launch NeoNect, the **Security Vault & Authentication** screen appears:

![Authentication & Security Vault](demos/entry_showcase.png)

1. **Server Configuration**:
   * Click the **Server Settings** icon to define your target NeoNect Backend Server endpoint (default: `http://localhost:8080` for local development or your organization's HTTPS domain).
2. **Account Registration**:
   * Enter your desired **Username** (alphanumeric, 3–32 characters) and a strong **Password**.
   * NeoNect utilizes `PBKDF2-HMAC-SHA-256` with \f$\ge 100,000\f$ iterations and CSPRNG salt to derive your authentication hashes locally.
3. **Session & Token Vault**:
   * Upon successful authentication, your authorization session token and device identity credentials are encrypted at rest with hardware-derived `AES-256-GCM` machine keys before being persisted locally.

---

## 4. Navigating the Interface & Starting Conversations

Once authenticated, you enter the primary application dashboard:

![NeoNect Real-Time Chat & Media](demos/chat_showcase.png)

### A. Managing Friends & Contacts
* **Send Friend Request**: Navigate to the **Friends** tab, click **Add Friend**, and enter your contact's username or account identifier.
* **Accepting Inbound Requests**: Incoming requests appear with instant notification toasts. Click **Accept** to establish mutual cryptographic peering.
* **Live Presence**: User status badges indicate whether contacts are **Online** (green), **Idle** (amber), **Do Not Disturb** (red), or **Offline** (gray).

### B. Messaging & Real-Time Interaction
* **End-to-End Encrypted Text**: Every message is encrypted using `AES-256-GCM` with recipient-specific session keys before transmission over WebSocket streams.
* **Voice Notes & Audio DSP**: Click and hold the microphone icon to record crisp 16 kHz PCM voice memos with live amplitude waveform visualization.
* **Media & File Attachments**: Drag and drop images, stickers, or documents directly into the chat viewport.
* **Delivery & Read Receipts**: Track message progression through status indicators: `Pending` ➔ `Sending` ➔ `Sent` (single check) ➔ `Delivered` ➔ `Read` (double check).

---

## 5. Theme Customization & UI Preferences

NeoNect provides reactive theme switching built directly into the UI:
* **Dark Mode** (Default): Soft contrast dark surfaces optimized for long sessions.
* **OLED Black**: Deep `#000000` pitch-black backgrounds for OLED displays and battery savings.
* **Light Mode**: High-contrast, clean slate palette for brightly lit environments.

To toggle themes, navigate to **Settings** ➔ **Appearance** or click the theme switcher icon in the sidebar.

---

## 6. Next Steps & Detailed Manuals

* 📦 [Installation & Platform Requirements](INSTALLATION.md)
* 🛠️ [Compiling, Testing & Building Documentation](BUILDING.md)
* 🐳 [Docker Deployment & Web Browser Streaming](DOCKER.md)
* 🌐 [Documentation Ecosystem & Static Hosting](DOCUMENTATION.md)
