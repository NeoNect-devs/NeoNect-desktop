# =====================================================================
# Stage 1: Build NeoNect Desktop Application with Qt 6.7.3
# =====================================================================
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    libssl-dev \
    libgl1-mesa-dev \
    libgl1 \
    libglib2.0-0 \
    libglib2.0-dev \
    libxkbcommon-dev \
    libxkbcommon-x11-0 \
    libfontconfig1 \
    libfontconfig1-dev \
    libdbus-1-3 \
    libdbus-1-dev \
    libx11-xcb1 \
    libxcb-cursor0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-randr0 \
    libxcb-render-util0 \
    libxcb-shape0 \
    libxcb-shm0 \
    libxcb-sync1 \
    libxcb-xfixes0 \
    libxcb-xinerama0 \
    libxcb-xkb1 \
    python3 \
    python3-pip \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Install official Qt 6.7.3 binaries using aqtinstall
RUN pip3 install --break-system-packages aqtinstall && \
    aqt install-qt linux desktop 6.7.3 linux_gcc_64 -O /opt/qt -m qtmultimedia qtshadertools

ENV QT_DIR="/opt/qt/6.7.3/gcc_64"
ENV PATH="${QT_DIR}/bin:${PATH}"
ENV CMAKE_PREFIX_PATH="${QT_DIR}"
ENV LD_LIBRARY_PATH="${QT_DIR}/lib:${LD_LIBRARY_PATH}"
ENV QT_PLUGIN_PATH="${QT_DIR}/plugins"
ENV QML2_IMPORT_PATH="${QT_DIR}/qml"

WORKDIR /src
COPY . .

RUN cmake -B /build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/opt/qt/6.7.3/gcc_64 \
    && cmake --build /build --target NeoNectApp

# =====================================================================
# Stage 2: Runtime Environment with Virtual Display & noVNC
# =====================================================================
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Install system runtime dependencies for Qt6 GUI, X11, OpenGL, and noVNC
RUN apt-get update && apt-get install -y --no-install-recommends \
    libopengl0 \
    libgl1 \
    libglx-mesa0 \
    libgl1-mesa-dri \
    mesa-utils \
    libegl1 \
    libglib2.0-0 \
    libfontconfig1 \
    libdbus-1-3 \
    libxkbcommon-x11-0 \
    libxkbcommon0 \
    libxcb1 \
    libxcb-cursor0 \
    libxcb-glx0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-randr0 \
    libxcb-render0 \
    libxcb-render-util0 \
    libxcb-shape0 \
    libxcb-shm0 \
    libxcb-sync1 \
    libxcb-xfixes0 \
    libxcb-xinerama0 \
    libxcb-xinput0 \
    libxcb-xkb1 \
    libx11-xcb1 \
    libxrender1 \
    libxi6 \
    libsm6 \
    libice6 \
    libssl3 \
    libbrotli1 \
    libasound2t64 \
    libpulse0 \
    libgstreamer1.0-0 \
    libgstreamer-plugins-base1.0-0 \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-libav \
    gstreamer1.0-gl \
    gstreamer1.0-alsa \
    x11-utils \
    x11-xserver-utils \
    procps \
    xvfb \
    fluxbox \
    x11vnc \
    novnc \
    websockify \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Copy the complete Qt 6.7.3 installation from the builder stage
COPY --from=builder /opt/qt /opt/qt

ENV QT_DIR="/opt/qt/6.7.3/gcc_64"
ENV PATH="${QT_DIR}/bin:${PATH}"
ENV LD_LIBRARY_PATH="${QT_DIR}/lib:${LD_LIBRARY_PATH}"
ENV QT_PLUGIN_PATH="${QT_DIR}/plugins"
ENV QML2_IMPORT_PATH="${QT_DIR}/qml"

# Fix novnc index.html for instant direct browser access without landing page
RUN ln -s /usr/share/novnc/vnc.html /usr/share/novnc/index.html || true

WORKDIR /app
COPY --from=builder /build/NeoNectApp /app/NeoNectApp
COPY docker/entrypoint.sh /app/entrypoint.sh

RUN chmod +x /app/entrypoint.sh /app/NeoNectApp

EXPOSE 6080 5900

ENTRYPOINT ["/app/entrypoint.sh"]
