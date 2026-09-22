# Installation & Platform Requirements {#installation}

This guide provides detailed instructions for installing the **NeoNect Desktop Client** across supported desktop operating systems, including prebuilt binaries, runtime dependencies, and platform configurations.

---

## 1. Supported Platforms & Binaries

NeoNect provides official builds for 64-bit Windows and Linux systems:

| Platform | Distribution Format | Compatibility |
| :--- | :--- | :--- |
| **Windows 11 / 10** | `.zip` (Portable Package) / `.exe` (Installer) | Windows 10 Version 1809+ / Windows 11 (x64) |
| **Linux (Debian/Ubuntu)** | `.tar.gz` (Standalone Archive) / Docker Container | Ubuntu 22.04 LTS+, Debian 12+, Fedora 38+, Arch Linux |
| **Web Browser (Virtual Display)** | Docker Image with noVNC | Any modern HTML5 web browser (Chrome, Firefox, Safari, Edge) |

---

## 2. Windows Installation

### Option A: Portable ZIP Archive (Recommended)
1. Download `NeoNect-vX.Y.Z-windows-x64.zip` from the official [GitHub Releases](https://github.com/NeoNect-devs/NeoNect-desktop/releases) page.
2. Extract the archive contents to your preferred directory (e.g. `C:\Program Files\NeoNect` or `C:\Users\<user>\AppData\Local\Programs\NeoNect`).
3. Run `NeoNectApp.exe`.

### Option B: Windows Installer
1. Download `NeoNect-Setup-vX.Y.Z.exe`.
2. Follow the setup wizard prompts to configure your installation path and create desktop/Start Menu shortcuts.
3. Launch NeoNect from the Start Menu.

### Windows Runtime Dependencies
The portable distribution and installer bundle all required Qt6 dynamic libraries and OpenSSL DLLs (`libcrypto-3-x64.dll`, `libssl-3-x64.dll`). If running on a minimal Windows Server installation, ensure the following are installed:
* [Visual C++ Redistributable (2015–2022 x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe)
* DirectX End-User Runtimes / D3Dcompiler_47.dll

---

## 3. Linux Installation

### Option A: Standalone Archive
1. Download `NeoNect-vX.Y.Z-linux-x64.tar.gz` from [GitHub Releases](https://github.com/NeoNect-devs/NeoNect-desktop/releases).
2. Extract the archive:
   ```bash
   tar -xzf NeoNect-vX.Y.Z-linux-x64.tar.gz -C /opt/neonect
   cd /opt/neonect
   ```
3. Ensure runtime system libraries are installed on Debian/Ubuntu:
   ```bash
   sudo apt-get update
   sudo apt-get install -y \
       libgl1-mesa-dri \
       libglx-mesa0 \
       libxkbcommon0 \
       libxkbcommon-x11-0 \
       libxcb1 \
       libxcb-cursor0 \
       libxcb-icccm4 \
       libxcb-image0 \
       libxcb-keysyms1 \
       libxcb-randr0 \
       libxcb-render0 \
       libxcb-shape0 \
       libxcb-sync1 \
       libxcb-xfixes0 \
       libxcb-xinerama0 \
       libxcb-xkb1 \
       libssl3 \
       libasound2t64 \
       libpulse0
   ```
4. Run the launcher script:
   ```bash
   ./NeoNectApp
   ```

---

## 4. Audio & Media Subsystem Configuration

NeoNect features hardware-level audio DSP, low-latency microphone capture, and waveform visualization:
* **Windows**: Uses native Windows Multimedia (`winmm.dll` waveIn/waveOut) and Media Control Interface (MCI). Zero external drivers required.
* **Linux**: Communicates via ALSA / PulseAudio. Ensure your user account is in the `audio` group:
  ```bash
  sudo usermod -aG audio $USER
  ```

---

## 5. Network Firewall & Connectivity

NeoNect communicates with the relay backend via two standard outbound network ports:
* **HTTPS / REST API**: TCP Port `443` (or `8080` in local dev environments)
* **WSS / Secure WebSocket**: TCP Port `443` / `8080` for bidirectional streaming

No inbound port forwarding or NAT holes are required; all peer connections are established via client-initiated outbound TLS tunnels.

---

## 6. Next Steps

* 🚀 [Getting Started Guide](GETTING_STARTED.md)
* 🛠️ [Compiling & Building from Source](BUILDING.md)
* 🐳 [Docker & Containerized Deployment](DOCKER.md)
