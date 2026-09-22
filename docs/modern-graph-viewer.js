/**
 * @file modern-graph-viewer.js
 * @brief Modern Interactive Pan/Zoom/Fullscreen Graph Viewer with Dual Theme (Light & Dark) support.
 * @author NeoNect Development Team
 * @date 2026
 */

(function () {
    let currentScale = 1.0;
    let translateX = 0;
    let translateY = 0;
    let isDragging = false;
    let startX = 0;
    let startY = 0;
    let initialPinchDistance = 0;
    let initialPinchScale = 1.0;
    let rawMermaidCode = "";

    const DEFAULT_DIAGRAM = `flowchart TB
    subgraph L1["Layer 1: Presentation Tier (Declarative QML & QtQuick)"]
        qml_ui["<b>QML User Interface</b><br/>Main.qml • ChatView.qml • FriendsHomePanel.qml<br/>WindowTitleBar.qml • MessageDelegate.qml • MediaLightboxModal"]
    end

    subgraph L2["Layer 2: Core Presentation Facades & ViewModels"]
        net_mgr["<b>NetworkManager</b><br/>Master UI Facade & Inter-Service Coordinator"]
        crypto_mgr["<b>CryptoManager</b><br/>Credential Provisioning & PBKDF2"]
        audio_mgr["<b>AudioManager</b><br/>Audio Streaming & Waveform DSP"]
        model_chat["<b>ChatMessageModel</b><br/>QAbstractListModel • Bubble Clustering"]
        notif_mgr["<b>NotificationManager</b><br/>Desktop Toasts & Unread Badges"]
    end

    subgraph L3["Layer 3: Composition Root & Application Controller"]
        app_ctrl["<b>NeoNect::Application</b><br/>Bootstrap • CLI Parsing • Dependency Injection Container • Qt Event Loop"]
    end

    subgraph L4["Layer 4: Business Domain Services"]
        auth_svc["<b>AuthService</b><br/>Registration • Login • Token Refresh • Session Cleanup"]
        device_svc["<b>DeviceService</b><br/>Device Fingerprinting • Hardware Binding • Device List"]
        friend_svc["<b>FriendService</b><br/>Friend Requests • Accept/Decline • Dynamic State Machine"]
        msg_svc["<b>MessageService</b><br/>Local SQLite Persistence • Delivery Invariants"]
        relay_svc["<b>RelayService</b><br/>E2EE Envelopes • WebSocket Stream • HTTP Polling Fallback"]
    end

    subgraph L5["Layer 5: Core Domain Entities & Value Objects"]
        domain_msg["<b>Message Entity</b><br/>UUID • Timestamps • Invariants • Sender/Recipient"]
        domain_status["<b>FriendStatus</b><br/>State Enum Transitions"]
    end

    subgraph L6["Layer 6: Persistence & Repository Tier"]
        settings_repo["<b>SettingsRepository</b><br/>Machine-Key AES-256-GCM Encrypted Token Storage"]
        sql_repo["<b>SqlMessageRepository</b><br/>SQLite WAL Mode • Background Worker Thread • O(1) Indexing"]
    end

    subgraph L7["Layer 7: Cryptography & Network Infrastructure"]
        crypto_svc["<b>CryptoService (OpenSSL)</b><br/>AES-256-GCM • PBKDF2 • CSPRNG • RAII"]
        http_trans["<b>HttpTransport</b><br/>QNetworkAccessManager • TLS REST API"]
        ws_client["<b>WebSocketClient</b><br/>RFC 6455 Client • SSL Frame Engine"]
    end

    qml_ui -->|Q_PROPERTY / Q_INVOKABLE| net_mgr
    qml_ui --> crypto_mgr
    qml_ui --> audio_mgr
    qml_ui --> model_chat
    qml_ui --> notif_mgr

    app_ctrl -.->|instantiates & wires| net_mgr
    app_ctrl -.-> crypto_mgr
    app_ctrl -.-> audio_mgr
    app_ctrl -.-> notif_mgr
    app_ctrl -.-> auth_svc
    app_ctrl -.-> settings_repo
    app_ctrl -.-> sql_repo

    net_mgr -->|delegates| auth_svc
    net_mgr --> device_svc
    net_mgr --> friend_svc
    net_mgr --> relay_svc

    model_chat -->|observes| msg_svc
    msg_svc -->|async I/O| sql_repo
    msg_svc -->|transmits domain msg| relay_svc
    friend_svc -->|p2p signaling| relay_svc

    relay_svc -->|encrypt / decrypt| crypto_svc
    relay_svc -->|REST poll| http_trans
    relay_svc -->|push stream| ws_client

    auth_svc --> http_trans
    device_svc --> http_trans
    friend_svc --> http_trans

    auth_svc -->|persists token| settings_repo
    crypto_mgr --> crypto_svc
    crypto_mgr --> settings_repo

    msg_svc -->|operates on| domain_msg
    domain_msg --> domain_status`;

    function isDarkMode() {
        return document.documentElement.classList.contains("dark-mode");
    }

    function initMermaid(themeOverride) {
        if (typeof mermaid === "undefined") return;

        const isDark = themeOverride !== undefined ? themeOverride : isDarkMode();

        mermaid.initialize({
            startOnLoad: false,
            securityLevel: "loose",
            theme: isDark ? "dark" : "default",
            themeVariables: isDark ? {
                darkMode: true,
                background: "#161b22",
                primaryColor: "#1e293b",
                primaryTextColor: "#f1f5f9",
                primaryBorderColor: "#38bdf8",
                lineColor: "#94a3b8",
                secondaryColor: "#0f172a",
                tertiaryColor: "#1e293b",
                clusterBkg: "#161b22",
                clusterBorder: "#38bdf8",
                titleColor: "#38bdf8",
                nodeTextColor: "#f1f5f9"
            } : {
                darkMode: false,
                background: "#ffffff",
                primaryColor: "#f8fafc",
                primaryTextColor: "#0f172a",
                primaryBorderColor: "#2563eb",
                lineColor: "#64748b",
                secondaryColor: "#f1f5f9",
                tertiaryColor: "#e2e8f0",
                clusterBkg: "#f8fafc",
                clusterBorder: "#2563eb",
                titleColor: "#1d4ed8",
                nodeTextColor: "#0f172a"
            },
            flowchart: {
                useMaxWidth: false,
                htmlLabels: true,
                curve: "basis",
                padding: 16
            }
        });
    }

    async function renderMermaidGraph(isThemeSwitch = false) {
        const container = document.getElementById("architectureMermaid");
        if (!container || typeof mermaid === "undefined") return;

        if (!rawMermaidCode) {
            const raw = container.getAttribute("data-code") || container.textContent.trim();
            rawMermaidCode = (raw && raw.length > 50 && !raw.includes("&lt;")) ? raw : DEFAULT_DIAGRAM;
            container.setAttribute("data-code", rawMermaidCode);
        }

        initMermaid();

        try {
            const uniqueId = "architectureSvg_" + Math.random().toString(36).substring(2, 9);
            const { svg } = await mermaid.render(uniqueId, rawMermaidCode);
            container.innerHTML = svg;
            if (!isThemeSwitch) {
                resetView();
            } else {
                updateTransform();
            }
        } catch (err) {
            console.warn("Mermaid.render error, trying mermaid.run:", err);
            container.removeAttribute("data-processed");
            container.innerHTML = rawMermaidCode;
            try {
                await mermaid.run({ nodes: [container] });
                if (!isThemeSwitch) resetView(); else updateTransform();
            } catch (fallbackErr) {
                console.error("Mermaid fallback render error:", fallbackErr);
            }
        }
    }

    function updateTransform() {
        const target = document.querySelector("#architectureMermaid svg") || document.getElementById("architectureMermaid");
        if (target) {
            target.style.transform = `translate(${translateX}px, ${translateY}px) scale(${currentScale})`;
            target.style.transformOrigin = "center center";
            target.style.transition = isDragging ? "none" : "transform 0.15s cubic-bezier(0.2, 0, 0, 1)";
        }
    }

    function resetView() {
        currentScale = 1.0;
        translateX = 0;
        translateY = 0;
        updateTransform();
    }

    function zoom(deltaFactor) {
        const newScale = currentScale * deltaFactor;
        if (newScale >= 0.35 && newScale <= 4.0) {
            currentScale = newScale;
            updateTransform();
        }
    }

    function setupPanZoom() {
        const viewport = document.getElementById("graphViewport");
        if (!viewport) return;

        // Toolbar buttons
        const btnIn = document.getElementById("btnZoomIn");
        const btnOut = document.getElementById("btnZoomOut");
        const btnReset = document.getElementById("btnReset");
        const btnFullscreen = document.getElementById("btnFullscreen");
        const card = document.querySelector(".modern-graph-card");

        if (btnIn) btnIn.onclick = () => zoom(1.25);
        if (btnOut) btnOut.onclick = () => zoom(0.8);
        if (btnReset) btnReset.onclick = () => resetView();
        if (btnFullscreen && card) {
            btnFullscreen.onclick = () => {
                if (!document.fullscreenElement) {
                    if (card.requestFullscreen) {
                        card.requestFullscreen().catch(() => {});
                    } else if (card.webkitRequestFullscreen) {
                        card.webkitRequestFullscreen();
                    }
                    btnFullscreen.textContent = "✖ Exit Fullscreen";
                } else {
                    if (document.exitFullscreen) {
                        document.exitFullscreen().catch(() => {});
                    } else if (document.webkitExitFullscreen) {
                        document.webkitExitFullscreen();
                    }
                    btnFullscreen.textContent = "⛶ Fullscreen";
                }
            };
        [btnIn, btnOut, btnReset, btnFullscreen].forEach((btn) => {
            if (btn) {
                btn.addEventListener("keydown", (e) => {
                    if (e.key === "Enter" || e.key === " ") {
                        e.preventDefault();
                        btn.click();
                    }
                });
            }
        });

        // Viewport keyboard shortcuts
        viewport.setAttribute("tabindex", "0");
        viewport.addEventListener("keydown", (e) => {
            if (e.key === "+" || e.key === "=") {
                e.preventDefault();
                zoom(1.25);
            } else if (e.key === "-" || e.key === "_") {
                e.preventDefault();
                zoom(0.8);
            } else if (e.key === "0") {
                e.preventDefault();
                resetView();
            } else if (e.key === "f" || e.key === "F") {
                e.preventDefault();
                if (btnFullscreen) btnFullscreen.click();
            }
        });

        // Mouse Wheel Zoom
        viewport.addEventListener("wheel", (e) => {
            e.preventDefault();
            const delta = e.deltaY > 0 ? 0.9 : 1.1;
            zoom(delta);
        }, { passive: false });

        // Mouse Drag to Pan
        viewport.addEventListener("mousedown", (e) => {
            if (e.button !== 0) return;
            isDragging = true;
            startX = e.clientX - translateX;
            startY = e.clientY - translateY;
            viewport.style.cursor = "grabbing";
        });

        window.addEventListener("mousemove", (e) => {
            if (!isDragging) return;
            translateX = e.clientX - startX;
            translateY = e.clientY - startY;
            updateTransform();
        });

        window.addEventListener("mouseup", () => {
            if (isDragging) {
                isDragging = false;
                if (viewport) viewport.style.cursor = "grab";
                updateTransform();
            }
        });

        // Touch Pinch & Pan on Mobile
        viewport.addEventListener("touchstart", (e) => {
            if (e.touches.length === 1) {
                isDragging = true;
                startX = e.touches[0].clientX - translateX;
                startY = e.touches[0].clientY - translateY;
            } else if (e.touches.length === 2) {
                isDragging = false;
                initialPinchDistance = Math.hypot(
                    e.touches[0].clientX - e.touches[1].clientX,
                    e.touches[0].clientY - e.touches[1].clientY
                );
                initialPinchScale = currentScale;
            }
        }, { passive: true });

        viewport.addEventListener("touchmove", (e) => {
            if (e.touches.length === 1 && isDragging) {
                translateX = e.touches[0].clientX - startX;
                translateY = e.touches[0].clientY - startY;
                updateTransform();
            } else if (e.touches.length === 2 && initialPinchDistance > 0) {
                const dist = Math.hypot(
                    e.touches[0].clientX - e.touches[1].clientX,
                    e.touches[0].clientY - e.touches[1].clientY
                );
                const factor = dist / initialPinchDistance;
                const newScale = Math.min(Math.max(initialPinchScale * factor, 0.35), 4.0);
                currentScale = newScale;
                updateTransform();
            }
        }, { passive: true });

        viewport.addEventListener("touchend", () => {
            isDragging = false;
            initialPinchDistance = 0;
            updateTransform();
        });
    }

    function observeThemeChanges() {
        const observer = new MutationObserver((mutations) => {
            for (const mutation of mutations) {
                if (mutation.attributeName === "class") {
                    renderMermaidGraph(true);
                }
            }
        });
        observer.observe(document.documentElement, { attributes: true, attributeFilter: ["class"] });
    }

    // Bootstrap when DOM is ready
    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", () => {
            renderMermaidGraph(false);
            setupPanZoom();
            observeThemeChanges();
        });
    } else {
        renderMermaidGraph(false);
        setupPanZoom();
        observeThemeChanges();
    }
})();
