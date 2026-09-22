/**
 * @file themedata.h
 * @brief Dynamic design token system and reactive UI theme manager for NeoNect.
 * @details Implements a centralized, reactive palette of design tokens (OLED black, soft dark,
 * accent gradients, typography sizes) exposed to QML via the Singleton pattern and Qt property system.
 * 
 * @pattern Singleton Pattern / Design Token Architecture
 * @author NeoNect Development Team
 * @version 1.0.0
 */

#pragma once
#include <QColor>
#include <QObject>

/**
 * @struct ThemeTokens
 * @brief Plain Old Data (POD) struct encapsulating the raw visual design tokens.
 * @details Stores color values for surfaces, borders, text contrast levels, status indicators,
 * input fields, and typographic font scales.
 */
struct ThemeTokens {
    QColor windowBackground{"#0e0f12"};        ///< Base window background color.
    QColor panelBackground{"#08080a"};         ///< Secondary content panel surface color.
    QColor headerBackground{"#0d0e11"};        ///< Top navigation header bar surface color.
    QColor sidebarBackground{"#000000"};       ///< Channel/server navigation sidebar background.
    QColor accentColor{"#0A84FF"};             ///< Primary neon accent brand color.
    QColor accentHover{"#0066CC"};             ///< Interactive hover state for accent controls.
    QColor textPrimary{"#f2f3f5"};             ///< High-contrast primary text color.
    QColor textSecondary{"#949ba4"};           ///< Medium-contrast descriptive label text.
    QColor textMuted{"#6d6f78"};               ///< Low-contrast subtle placeholder/timestamp text.

    QColor borderColor{"#18191d"};             ///< Structural dividing line and border color.
    QColor itemHoverBackground{"#141518"};      ///< Background highlight when hovering interactive items.
    QColor itemSelectedBackground{"#1e1f24"};   ///< Background fill for selected/active list delegates.

    QColor scrollBarThumb{"#2b2d31"};          ///< Idle scrollbar thumb handle color.
    QColor scrollBarThumbHover{"#3f4248"};     ///< Active/dragged scrollbar thumb handle color.
    QColor statusOnline{"#23a55a"};            ///< Green status badge for online peers.
    QColor statusOffline{"#80848e"};           ///< Neutral grey status badge for offline peers.

    QColor inputGradientStart{"#0A84FF"};      ///< Accent gradient starting color for input borders.
    QColor inputGradientEnd{"#00B4D8"};        ///< Accent gradient ending color for input borders.
    QColor inputSolidBorder{"#18191d"};        ///< Inactive solid input border color.
    QColor inputBackgroundActive{"#000000"};   ///< Focused text field background color.
    QColor inputBackgroundInactive{"#08080a"}; ///< Unfocused text field background color.
    QColor placeholderColor{"#6d6f78"};        ///< Input placeholder text color.

    int fontSizeNormal{14};                    ///< Default body typography size in points.
    int fontSizeHeader{18};                    ///< Section header typography size in points.
    int fontSizeSmall{11};                     ///< Metadata/badge small typography size in points.
};

/**
 * @class ThemeData
 * @brief Singleton theme engine exposing reactive styling properties to QML.
 * @details Manages active design presets (`OLED Black`, `Soft Charcoal`), notifies QML
 * bindings on theme modification via the `themeChanged()` signal, and provides dynamic
 * runtime color overrides.
 * 
 * @pattern Singleton Pattern
 */
class ThemeData : public QObject {
    Q_OBJECT

    // ─── CORE UI TOKENS ───────────────────────────────────────────────
    /** @brief Base background color for the main application window surface. */
    Q_PROPERTY(QColor windowBackground READ windowBackground WRITE setWindowBackground NOTIFY themeChanged)
    /** @brief Background color for intermediate panels, cards, and container views. */
    Q_PROPERTY(QColor panelBackground READ panelBackground WRITE setPanelBackground NOTIFY themeChanged)
    /** @brief Background color for title bars, headers, and conversation top banners. */
    Q_PROPERTY(QColor headerBackground READ headerBackground WRITE setHeaderBackground NOTIFY themeChanged)
    /** @brief Background color for navigation sidebars and server lists. */
    Q_PROPERTY(QColor sidebarBackground READ sidebarBackground WRITE setSidebarBackground NOTIFY themeChanged)
    /** @brief Primary brand accent color used for buttons, links, and highlights. */
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY themeChanged)
    /** @brief Darkened accent tint applied during mouse hover and press states. */
    Q_PROPERTY(QColor accentHover READ accentHover WRITE setAccentHover NOTIFY themeChanged)
    /** @brief Primary typography color ensuring optimal readability against dark surfaces. */
    Q_PROPERTY(QColor textPrimary READ textPrimary WRITE setTextPrimary NOTIFY themeChanged)
    /** @brief Secondary typography color for subtitles, handles, and metadata. */
    Q_PROPERTY(QColor textSecondary READ textSecondary WRITE setTextSecondary NOTIFY themeChanged)
    /** @brief Muted typography color for timestamps, disabled states, and hints. */
    Q_PROPERTY(QColor textMuted READ textMuted WRITE setTextMuted NOTIFY themeChanged)

    // ─── BORDERS & SEPARATORS ─────────────────────────────────────────
    /** @brief Subtle border color separating adjacent layout panels and containers. */
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY themeChanged)
    /** @brief Hover background highlight for list rows, buttons, and icons. */
    Q_PROPERTY(QColor itemHoverBackground READ itemHoverBackground WRITE setItemHoverBackground NOTIFY themeChanged)
    /** @brief Selection background highlight for active conversation channels. */
    Q_PROPERTY(QColor itemSelectedBackground READ itemSelectedBackground WRITE setItemSelectedBackground NOTIFY themeChanged)

    // ─── SCROLLBAR & STATUS TOKENS ────────────────────────────────────
    /** @brief Normal idle color of virtualized scrollbar thumb handles. */
    Q_PROPERTY(QColor scrollBarThumb READ scrollBarThumb WRITE setScrollBarThumb NOTIFY themeChanged)
    /** @brief Highlighted color of scrollbar thumb handles during interaction. */
    Q_PROPERTY(QColor scrollBarThumbHover READ scrollBarThumbHover WRITE setScrollBarThumbHover NOTIFY themeChanged)
    /** @brief Green indicator color signifying an online/active peer status. */
    Q_PROPERTY(QColor statusOnline READ statusOnline WRITE setStatusOnline NOTIFY themeChanged)
    /** @brief Grey indicator color signifying an offline/disconnected peer status. */
    Q_PROPERTY(QColor statusOffline READ statusOffline WRITE setStatusOffline NOTIFY themeChanged)

    // ─── INPUT TEXTFIELD TOKENS ───────────────────────────────────────
    /** @brief Starting color for focused text input border gradient. */
    Q_PROPERTY(QColor inputGradientStart READ inputGradientStart WRITE setInputGradientStart NOTIFY themeChanged)
    /** @brief Ending color for focused text input border gradient. */
    Q_PROPERTY(QColor inputGradientEnd READ inputGradientEnd WRITE setInputGradientEnd NOTIFY themeChanged)
    /** @brief Solid inactive border color for text input fields. */
    Q_PROPERTY(QColor inputSolidBorder READ inputSolidBorder WRITE setInputSolidBorder NOTIFY themeChanged)
    /** @brief Background color of text input field when focused. */
    Q_PROPERTY(QColor inputBackgroundActive READ inputBackgroundActive WRITE setInputBackgroundActive NOTIFY themeChanged)
    /** @brief Background color of text input field when idle/unfocused. */
    Q_PROPERTY(QColor inputBackgroundInactive READ inputBackgroundInactive WRITE setInputBackgroundInactive NOTIFY themeChanged)
    /** @brief Placeholder text color in empty input fields. */
    Q_PROPERTY(QColor placeholderColor READ placeholderColor WRITE setPlaceholderColor NOTIFY themeChanged)

    // ─── TYPOGRAPHY ───────────────────────────────────────────────────
    /** @brief Standard body text font size in points. */
    Q_PROPERTY(int fontSizeNormal READ fontSizeNormal WRITE setFontSizeNormal NOTIFY themeChanged)
    /** @brief Section header and modal title font size in points. */
    Q_PROPERTY(int fontSizeHeader READ fontSizeHeader WRITE setFontSizeHeader NOTIFY themeChanged)
    /** @brief Timestamp and status badge small font size in points. */
    Q_PROPERTY(int fontSizeSmall READ fontSizeSmall WRITE setFontSizeSmall NOTIFY themeChanged)

public:
    /**
     * @brief Accesses the singleton instance of the theme manager.
     * @return Pointer to global `ThemeData` instance.
     */
    static ThemeData *instance() {
        static ThemeData m_instance;
        return &m_instance;
    }

    ThemeData(const ThemeData &) = delete;
    ThemeData &operator=(const ThemeData &) = delete;
    ThemeData(ThemeData &&) = delete;
    ThemeData &operator=(ThemeData &&) = delete;

    // Getters
    QColor windowBackground() const { return m_tokens.windowBackground; }
    QColor panelBackground() const { return m_tokens.panelBackground; }
    QColor headerBackground() const { return m_tokens.headerBackground; }
    QColor sidebarBackground() const { return m_tokens.sidebarBackground; }
    QColor accentColor() const { return m_tokens.accentColor; }
    QColor accentHover() const { return m_tokens.accentHover; }
    QColor textPrimary() const { return m_tokens.textPrimary; }
    QColor textSecondary() const { return m_tokens.textSecondary; }
    QColor textMuted() const { return m_tokens.textMuted; }

    QColor borderColor() const { return m_tokens.borderColor; }
    QColor itemHoverBackground() const { return m_tokens.itemHoverBackground; }
    QColor itemSelectedBackground() const { return m_tokens.itemSelectedBackground; }

    QColor scrollBarThumb() const { return m_tokens.scrollBarThumb; }
    QColor scrollBarThumbHover() const { return m_tokens.scrollBarThumbHover; }
    QColor statusOnline() const { return m_tokens.statusOnline; }
    QColor statusOffline() const { return m_tokens.statusOffline; }

    QColor inputGradientStart() const { return m_tokens.inputGradientStart; }
    QColor inputGradientEnd() const { return m_tokens.inputGradientEnd; }
    QColor inputSolidBorder() const { return m_tokens.inputSolidBorder; }
    QColor inputBackgroundActive() const { return m_tokens.inputBackgroundActive; }
    QColor inputBackgroundInactive() const { return m_tokens.inputBackgroundInactive; }
    QColor placeholderColor() const { return m_tokens.placeholderColor; }

    int fontSizeNormal() const { return m_tokens.fontSizeNormal; }
    int fontSizeHeader() const { return m_tokens.fontSizeHeader; }
    int fontSizeSmall() const { return m_tokens.fontSizeSmall; }

    // Setters
    void setWindowBackground(const QColor &c) { updateColor(m_tokens.windowBackground, c); }
    void setPanelBackground(const QColor &c) { updateColor(m_tokens.panelBackground, c); }
    void setHeaderBackground(const QColor &c) { updateColor(m_tokens.headerBackground, c); }
    void setSidebarBackground(const QColor &c) { updateColor(m_tokens.sidebarBackground, c); }
    void setAccentColor(const QColor &c) { updateColor(m_tokens.accentColor, c); }
    void setAccentHover(const QColor &c) { updateColor(m_tokens.accentHover, c); }
    void setTextPrimary(const QColor &c) { updateColor(m_tokens.textPrimary, c); }
    void setTextSecondary(const QColor &c) { updateColor(m_tokens.textSecondary, c); }
    void setTextMuted(const QColor &c) { updateColor(m_tokens.textMuted, c); }

    void setBorderColor(const QColor &c) { updateColor(m_tokens.borderColor, c); }
    void setItemHoverBackground(const QColor &c) { updateColor(m_tokens.itemHoverBackground, c); }
    void setItemSelectedBackground(const QColor &c) { updateColor(m_tokens.itemSelectedBackground, c); }

    void setScrollBarThumb(const QColor &c) { updateColor(m_tokens.scrollBarThumb, c); }
    void setScrollBarThumbHover(const QColor &c) { updateColor(m_tokens.scrollBarThumbHover, c); }
    void setStatusOnline(const QColor &c) { updateColor(m_tokens.statusOnline, c); }
    void setStatusOffline(const QColor &c) { updateColor(m_tokens.statusOffline, c); }

    void setInputGradientStart(const QColor &c) { updateColor(m_tokens.inputGradientStart, c); }
    void setInputGradientEnd(const QColor &c) { updateColor(m_tokens.inputGradientEnd, c); }
    void setInputSolidBorder(const QColor &c) { updateColor(m_tokens.inputSolidBorder, c); }
    void setInputBackgroundActive(const QColor &c) { updateColor(m_tokens.inputBackgroundActive, c); }
    void setInputBackgroundInactive(const QColor &c) { updateColor(m_tokens.inputBackgroundInactive, c); }
    void setPlaceholderColor(const QColor &c) { updateColor(m_tokens.placeholderColor, c); }

    void setFontSizeNormal(int s) { updateInt(m_tokens.fontSizeNormal, s); }
    void setFontSizeHeader(int s) { updateInt(m_tokens.fontSizeHeader, s); }
    void setFontSizeSmall(int s) { updateInt(m_tokens.fontSizeSmall, s); }

    /**
     * @brief Loads the high-contrast true black OLED preset theme.
     * @details Optimizes battery usage on OLED panels and provides a modern cyberpunk contrast.
     */
    Q_INVOKABLE void loadOledPreset() {
        m_tokens.windowBackground = QColor("#0e0f12");
        m_tokens.panelBackground = QColor("#08080a");
        m_tokens.headerBackground = QColor("#0d0e11");
        m_tokens.sidebarBackground = QColor("#000000");
        m_tokens.accentColor = QColor("#0A84FF");
        m_tokens.accentHover = QColor("#0066CC");
        m_tokens.textPrimary = QColor("#f2f3f5");
        m_tokens.textSecondary = QColor("#949ba4");
        m_tokens.textMuted = QColor("#6d6f78");

        m_tokens.borderColor = QColor("#18191d");
        m_tokens.itemHoverBackground = QColor("#141518");
        m_tokens.itemSelectedBackground = QColor("#1e1f24");

        m_tokens.scrollBarThumb = QColor("#2b2d31");
        m_tokens.scrollBarThumbHover = QColor("#3f4248");
        m_tokens.statusOnline = QColor("#23a55a");
        m_tokens.statusOffline = QColor("#80848e");

        m_tokens.inputGradientStart = QColor("#0A84FF");
        m_tokens.inputGradientEnd = QColor("#00E5FF");
        m_tokens.inputSolidBorder = QColor("#18191d");
        m_tokens.inputBackgroundActive = QColor("#000000");
        m_tokens.inputBackgroundInactive = QColor("#08080a");
        m_tokens.placeholderColor = QColor("#6d6f78");

        emit themeChanged();
    }

    /**
     * @brief Loads the soft dark charcoal preset theme (Discord/Slack aesthetic).
     * @details Employs warm slate-gray tones to reduce eye strain during extended messaging sessions.
     */
    Q_INVOKABLE void loadSoftDarkPreset() {
        m_tokens.windowBackground = QColor("#1e1f22");
        m_tokens.panelBackground = QColor("#2b2d31");
        m_tokens.headerBackground = QColor("#313338");
        m_tokens.sidebarBackground = QColor("#111214");
        m_tokens.accentColor = QColor("#00E5FF");
        m_tokens.accentHover = QColor("#00B4D8");
        m_tokens.textPrimary = QColor("#dbdee1");
        m_tokens.textSecondary = QColor("#949ba4");
        m_tokens.textMuted = QColor("#6d6f78");

        m_tokens.borderColor = QColor("#1f2023");
        m_tokens.itemHoverBackground = QColor("#35373c");
        m_tokens.itemSelectedBackground = QColor("#404249");

        m_tokens.scrollBarThumb = QColor("#3f4248");
        m_tokens.scrollBarThumbHover = QColor("#4e5058");
        m_tokens.statusOnline = QColor("#23a55a");
        m_tokens.statusOffline = QColor("#80848e");

        m_tokens.inputGradientStart = QColor("#00E5FF");
        m_tokens.inputGradientEnd = QColor("#0A84FF");
        m_tokens.inputSolidBorder = QColor("#383a40");
        m_tokens.inputBackgroundActive = QColor("#1e1f22");
        m_tokens.inputBackgroundInactive = QColor("#2b2d31");
        m_tokens.placeholderColor = QColor("#80848e");

        emit themeChanged();
    }

signals:
    /**
     * @brief Emitted whenever any visual design token or color preset is modified.
     * @details Automatically invalidates and updates all active QML bindings across the UI.
     */
    void themeChanged();

private:
    explicit ThemeData(QObject *parent = nullptr) : QObject(parent) {
        loadOledPreset();
    }

    void updateColor(QColor &target, const QColor &value) {
        if (target != value) {
            target = value;
            emit themeChanged();
        }
    }

    void updateInt(int &target, int value) {
        if (target != value) {
            target = value;
            emit themeChanged();
        }
    }

    ThemeTokens m_tokens;
};