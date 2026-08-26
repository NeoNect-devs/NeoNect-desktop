// src/themedata.h
#pragma once
#include <QColor>
#include <QObject>

struct ThemeTokens {
    QColor windowBackground{"#0e0f12"};
    QColor panelBackground{"#08080a"};
    QColor headerBackground{"#0d0e11"};
    QColor sidebarBackground{"#000000"};
    QColor accentColor{"#0A84FF"};
    QColor accentHover{"#0066CC"};
    QColor textPrimary{"#f2f3f5"};
    QColor textSecondary{"#949ba4"};
    QColor textMuted{"#6d6f78"};

    QColor borderColor{"#18191d"};
    QColor itemHoverBackground{"#141518"};
    QColor itemSelectedBackground{"#1e1f24"};

    QColor scrollBarThumb{"#2b2d31"};
    QColor scrollBarThumbHover{"#3f4248"};
    QColor statusOnline{"#23a55a"};
    QColor statusOffline{"#80848e"};

    QColor inputGradientStart{"#0A84FF"};
    QColor inputGradientEnd{"#00B4D8"};
    QColor inputSolidBorder{"#18191d"};
    QColor inputBackgroundActive{"#000000"};
    QColor inputBackgroundInactive{"#08080a"};
    QColor placeholderColor{"#6d6f78"};

    int fontSizeNormal{14};
    int fontSizeHeader{18};
    int fontSizeSmall{11};
};

class ThemeData : public QObject {
    Q_OBJECT

    // ─── CORE UI TOKENS ───────────────────────────────────────────────
    Q_PROPERTY(QColor windowBackground READ windowBackground WRITE setWindowBackground NOTIFY themeChanged)
    Q_PROPERTY(QColor panelBackground READ panelBackground WRITE setPanelBackground NOTIFY themeChanged)
    Q_PROPERTY(QColor headerBackground READ headerBackground WRITE setHeaderBackground NOTIFY themeChanged)
    Q_PROPERTY(QColor sidebarBackground READ sidebarBackground WRITE setSidebarBackground NOTIFY themeChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY themeChanged)
    Q_PROPERTY(QColor accentHover READ accentHover WRITE setAccentHover NOTIFY themeChanged)
    Q_PROPERTY(QColor textPrimary READ textPrimary WRITE setTextPrimary NOTIFY themeChanged)
    Q_PROPERTY(QColor textSecondary READ textSecondary WRITE setTextSecondary NOTIFY themeChanged)
    Q_PROPERTY(QColor textMuted READ textMuted WRITE setTextMuted NOTIFY themeChanged)

    // ─── BORDERS & SEPARATORS ─────────────────────────────────────────
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY themeChanged)
    Q_PROPERTY(QColor itemHoverBackground READ itemHoverBackground WRITE setItemHoverBackground NOTIFY themeChanged)
    Q_PROPERTY(QColor itemSelectedBackground READ itemSelectedBackground WRITE setItemSelectedBackground NOTIFY themeChanged)

    // ─── SCROLLBAR & STATUS TOKENS ────────────────────────────────────
    Q_PROPERTY(QColor scrollBarThumb READ scrollBarThumb WRITE setScrollBarThumb NOTIFY themeChanged)
    Q_PROPERTY(QColor scrollBarThumbHover READ scrollBarThumbHover WRITE setScrollBarThumbHover NOTIFY themeChanged)
    Q_PROPERTY(QColor statusOnline READ statusOnline WRITE setStatusOnline NOTIFY themeChanged)
    Q_PROPERTY(QColor statusOffline READ statusOffline WRITE setStatusOffline NOTIFY themeChanged)

    // ─── INPUT TEXTFIELD TOKENS ───────────────────────────────────────
    Q_PROPERTY(QColor inputGradientStart READ inputGradientStart WRITE setInputGradientStart NOTIFY themeChanged)
    Q_PROPERTY(QColor inputGradientEnd READ inputGradientEnd WRITE setInputGradientEnd NOTIFY themeChanged)
    Q_PROPERTY(QColor inputSolidBorder READ inputSolidBorder WRITE setInputSolidBorder NOTIFY themeChanged)
    Q_PROPERTY(QColor inputBackgroundActive READ inputBackgroundActive WRITE setInputBackgroundActive NOTIFY themeChanged)
    Q_PROPERTY(QColor inputBackgroundInactive READ inputBackgroundInactive WRITE setInputBackgroundInactive NOTIFY themeChanged)
    Q_PROPERTY(QColor placeholderColor READ placeholderColor WRITE setPlaceholderColor NOTIFY themeChanged)

    // ─── TYPOGRAPHY ───────────────────────────────────────────────────
    Q_PROPERTY(int fontSizeNormal READ fontSizeNormal WRITE setFontSizeNormal NOTIFY themeChanged)
    Q_PROPERTY(int fontSizeHeader READ fontSizeHeader WRITE setFontSizeHeader NOTIFY themeChanged)
    Q_PROPERTY(int fontSizeSmall READ fontSizeSmall WRITE setFontSizeSmall NOTIFY themeChanged)

public:
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

    // ─── OLED BLACK PRESET ────────────────────────────────────────────
    Q_INVOKABLE void loadOledPreset() {
        m_tokens.windowBackground = QColor("#0e0f12");
        m_tokens.panelBackground = QColor("#08080a");
        m_tokens.headerBackground = QColor("#0d0e11");
        m_tokens.sidebarBackground = QColor("#000000");
        m_tokens.accentColor = QColor("#5865f2");
        m_tokens.accentHover = QColor("#4752c4");
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

        m_tokens.inputGradientStart = QColor("#5865f2");
        m_tokens.inputGradientEnd = QColor("#7289da");
        m_tokens.inputSolidBorder = QColor("#18191d");
        m_tokens.inputBackgroundActive = QColor("#000000");
        m_tokens.inputBackgroundInactive = QColor("#08080a");
        m_tokens.placeholderColor = QColor("#6d6f78");

        emit themeChanged();
    }

    // ─── SOFT CHARCOAL PRESET ─────────────────────────────────────────
    Q_INVOKABLE void loadSoftDarkPreset() {
        m_tokens.windowBackground = QColor("#1e1f22");
        m_tokens.panelBackground = QColor("#2b2d31");
        m_tokens.headerBackground = QColor("#313338");
        m_tokens.sidebarBackground = QColor("#111214");
        m_tokens.accentColor = QColor("#5865f2");
        m_tokens.accentHover = QColor("#4752c4");
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

        m_tokens.inputGradientStart = QColor("#5865f2");
        m_tokens.inputGradientEnd = QColor("#7289da");
        m_tokens.inputSolidBorder = QColor("#383a40");
        m_tokens.inputBackgroundActive = QColor("#1e1f22");
        m_tokens.inputBackgroundInactive = QColor("#2b2d31");
        m_tokens.placeholderColor = QColor("#80848e");

        emit themeChanged();
    }

signals:
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