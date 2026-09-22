# Docker Deployment & Web Streaming {#docker_guide}

The **NeoNect Desktop Client** can be executed inside a headless containerized environment with virtual display rendering (`Xvfb`) and streamed directly to any standard HTML5 web browser via **noVNC** and **websockify**.

---

## 1. Architecture of the Containerized Client

The [`Dockerfile`](../Dockerfile) uses a multi-stage build design:
1. **Stage 1 (Builder)**: Ubuntu 24.04 with official Qt 6.7.3 (`aqtinstall`), CMake, Ninja, and OpenSSL. Compiles `NeoNectApp` as an optimized release binary.
2. **Stage 2 (Runtime)**: Minimal runtime environment with Mesa llvmpipe software OpenGL, Xvfb virtual framebuffer, Fluxbox window manager, x11vnc, and noVNC web streaming engine.

```
┌─────────────────────────────────────────────────────────────┐
│                       Host Machine                          │
│               (Access via Web Browser / Port 6080)          │
└──────────────────────────────┬──────────────────────────────┘
                               │ HTTP / WebSocket (Port 6080)
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                 Docker Container Environment                │
│                                                             │
│   ┌───────────────┐     ┌───────────────┐    ┌──────────┐   │
│   │ noVNC / HTML5 │ ◄── │  websockify   │ ◄─ │  x11vnc  │   │
│   └───────────────┘     └───────────────┘    └────▲─────┘   │
│                                                   │         │
│   ┌───────────────────────────────────────────────┴─────┐   │
│   │               Xvfb Virtual Framebuffer (:99)        │   │
│   │   ┌─────────────────┐       ┌───────────────────┐   │   │
│   │   │ Fluxbox Manager │       │  NeoNectApp (QML) │   │   │
│   │   └─────────────────┘       └───────────────────┘   │   │
│   └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Quick Start: Build & Run with Docker

### A. Building the Docker Image
```bash
docker build -t neonect-desktop:latest .
```

### B. Running the Container
Launch the container and publish port `6080` (noVNC web interface):
```bash
docker run -d \
    --name neonect-client \
    -p 6080:6080 \
    -e RESOLUTION="1440x900x24" \
    -e PROFILE="ContainerUser" \
    neonect-desktop:latest
```

### C. Accessing via Web Browser
Open your browser and navigate to:
```text
http://localhost:6080/
```
You will immediately see the full NeoNect Desktop Qt Quick interface running with hardware-level responsiveness directly inside your browser!

---

## 3. Environment Variables & Configuration

The container entrypoint script (`docker/entrypoint.sh`) supports dynamic configuration through environment variables:

| Variable | Default | Description |
| :--- | :--- | :--- |
| `RESOLUTION` | `1280x800x24` | Virtual display resolution and color depth (e.g. `1920x1080x24`, `1440x900x24`). |
| `DISPLAY` | `:99` | X11 virtual display number. |
| `VNC_PORT` | `5900` | Port for direct RFB / VNC client connections. |
| `NOVNC_PORT` | `6080` | Port for the noVNC HTML5 WebSocket web stream. |
| `PROFILE` | `default` | Profile name for user database and key isolation. |
| `MOCK_MODE` | `0` | Set to `1` to run in UI simulation mode without server connection. |

---

## 4. Persistent Storage & Data Volumes

To preserve message history, cryptographic keys, and encrypted settings across container restarts, mount a persistent host directory or named Docker volume to `/root/.local/share/NeoNect`:

```bash
docker run -d \
    --name neonect-client \
    -p 6080:6080 \
    -v neonect_storage:/root/.local/share/NeoNect \
    neonect-desktop:latest
```

---

## 5. Docker Compose Configuration

Create a `docker-compose.yml` file for unified multi-client testing or orchestration:

```yaml
version: '3.8'

services:
  neonect-alice:
    build: .
    container_name: neonect-alice
    ports:
      - "6081:6080"
    environment:
      - RESOLUTION=1366x768x24
      - PROFILE=Alice
    volumes:
      - alice_data:/root/.local/share/NeoNect

  neonect-bob:
    build: .
    container_name: neonect-bob
    ports:
      - "6082:6080"
    environment:
      - RESOLUTION=1366x768x24
      - PROFILE=Bob
    volumes:
      - bob_data:/root/.local/share/NeoNect

volumes:
  alice_data:
  bob_data:
```

Start the multi-client network:
```bash
docker compose up -d
```
* Access **Alice** at: `http://localhost:6081`
* Access **Bob** at: `http://localhost:6082`

---

## 6. Next Steps

* 🛠️ [Building from Source & Running Tests](BUILDING.md)
* 🌐 [Documentation Ecosystem & GitHub Pages Deployment](DOCUMENTATION.md)
* 🚀 [Getting Started Guide](GETTING_STARTED.md)
