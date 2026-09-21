#!/usr/bin/env bash
set -e

# Configuration defaults
RESOLUTION="${RESOLUTION:-1280x800x24}"
DISPLAY="${DISPLAY:-:99}"
VNC_PORT="${VNC_PORT:-5900}"
NOVNC_PORT="${NOVNC_PORT:-6080}"
PROFILE="${PROFILE:-default}"

echo "============================================"
echo " Starting NeoNect Desktop Container"
echo " Profile:      ${PROFILE}"
echo " Display:      ${DISPLAY}"
echo " Resolution:   ${RESOLUTION}"
echo " noVNC Port:   ${NOVNC_PORT}"
echo "============================================"

# Ensure software rendering for headless OpenGL / Qt Quick
export DISPLAY="${DISPLAY}"
export LIBGL_ALWAYS_SOFTWARE=1
export MESA_LOADER_DRIVER_OVERRIDE=llvmpipe
export QT_QUICK_CONTROLS_STYLE=Basic

# Clean up any stale X11 lock files from container restarts
DISPLAY_NUM="${DISPLAY#:}"
rm -f "/tmp/.X${DISPLAY_NUM}-lock" "/tmp/.X11-unix/X${DISPLAY_NUM}" || true

# 1. Start Xvfb virtual framebuffer
echo "[+] Starting Xvfb on display ${DISPLAY} (${RESOLUTION})..."
Xvfb "${DISPLAY}" -screen 0 "${RESOLUTION}" -ac +extension GLX +render -noreset &
XVFB_PID=$!

# Wait for X server to become available
for i in $(seq 1 30); do
    if xset q -display "${DISPLAY}" >/dev/null 2>&1 || xdpyinfo -display "${DISPLAY}" >/dev/null 2>&1 || [ -e "/tmp/.X11-unix/X${DISPLAY_NUM}" ]; then
        echo "[+] Xvfb is ready on display ${DISPLAY}"
        break
    fi
    sleep 0.2
done

# 2. Start lightweight Fluxbox window manager
echo "[+] Starting fluxbox window manager..."
fluxbox &
FLUXBOX_PID=$!
sleep 0.5

# 3. Start x11vnc server
echo "[+] Starting x11vnc on port ${VNC_PORT}..."
x11vnc -display "${DISPLAY}" -forever -shared -nopw -rfbport "${VNC_PORT}" -bg

# 4. Start noVNC / websockify HTML5 Web viewer
echo "[+] Starting websockify / noVNC on port ${NOVNC_PORT}..."
if [ -d "/usr/share/novnc" ]; then
    websockify --web=/usr/share/novnc/ "${NOVNC_PORT}" "localhost:${VNC_PORT}" &
elif [ -d "/opt/novnc" ]; then
    websockify --web=/opt/novnc/ "${NOVNC_PORT}" "localhost:${VNC_PORT}" &
else
    websockify "${NOVNC_PORT}" "localhost:${VNC_PORT}" &
fi
sleep 0.5

# 5. Launch NeoNect Desktop application
echo "[+] Launching NeoNect Desktop Application (Profile: ${PROFILE})..."
APP_ARGS=("-p" "${PROFILE}")

if [ -n "${MOCK_MODE}" ] && [ "${MOCK_MODE}" = "1" ]; then
    APP_ARGS+=("--mock")
fi

exec /app/NeoNectApp "${APP_ARGS[@]}"
