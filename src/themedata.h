#pragma once
#include <QColor>
#include <QObject>


class ThemeData : public QObject {
  Q_OBJECT

  // ─── CORE UI TOKENS ───────────────────────────────────────────────
  Q_PROPERTY(QColor windowBackground READ windowBackground WRITE
                 setWindowBackground NOTIFY themeChanged)
  Q_PROPERTY(QColor panelBackground READ panelBackground WRITE
                 setPanelBackground NOTIFY themeChanged)
  Q_PROPERTY(QColor headerBackground READ headerBackground WRITE
                 setHeaderBackground NOTIFY themeChanged)
  Q_PROPERTY(QColor sidebarBackground READ sidebarBackground WRITE
                 setSidebarBackground NOTIFY themeChanged)
  Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY
                 themeChanged)
  Q_PROPERTY(QColor accentHover READ accentHover WRITE setAccentHover NOTIFY
                 themeChanged)
  Q_PROPERTY(QColor textPrimary READ textPrimary WRITE setTextPrimary NOTIFY
                 themeChanged)
  Q_PROPERTY(QColor textSecondary READ textSecondary WRITE setTextSecondary
                 NOTIFY themeChanged)
  Q_PROPERTY(
      QColor textMuted READ textMuted WRITE setTextMuted NOTIFY themeChanged)

  // ─── BORDERS & SEPARATORS ─────────────────────────────────────────
  Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY
                 themeChanged)
  Q_PROPERTY(QColor itemHoverBackground READ itemHoverBackground WRITE
                 setItemHoverBackground NOTIFY themeChanged)
  Q_PROPERTY(QColor itemSelectedBackground READ itemSelectedBackground WRITE
                 setItemSelectedBackground NOTIFY themeChanged)

  // ─── SCROLLBAR & STATUS TOKENS ────────────────────────────────────
  Q_PROPERTY(QColor scrollBarThumb READ scrollBarThumb WRITE setScrollBarThumb
                 NOTIFY themeChanged)
  Q_PROPERTY(QColor scrollBarThumbHover READ scrollBarThumbHover WRITE
                 setScrollBarThumbHover NOTIFY themeChanged)
  Q_PROPERTY(QColor statusOnline READ statusOnline WRITE setStatusOnline NOTIFY
                 themeChanged)
  Q_PROPERTY(QColor statusOffline READ statusOffline WRITE setStatusOffline
                 NOTIFY themeChanged)

  // ─── INPUT TEXTFIELD TOKENS ───────────────────────────────────────
  Q_PROPERTY(QColor inputGradientStart READ inputGradientStart WRITE
                 setInputGradientStart NOTIFY themeChanged)
  Q_PROPERTY(QColor inputGradientEnd READ inputGradientEnd WRITE
                 setInputGradientEnd NOTIFY themeChanged)
  Q_PROPERTY(QColor inputSolidBorder READ inputSolidBorder WRITE
                 setInputSolidBorder NOTIFY themeChanged)
  Q_PROPERTY(QColor inputBackgroundActive READ inputBackgroundActive WRITE
                 setInputBackgroundActive NOTIFY themeChanged)
  Q_PROPERTY(QColor inputBackgroundInactive READ inputBackgroundInactive WRITE
                 setInputBackgroundInactive NOTIFY themeChanged)
  Q_PROPERTY(QColor placeholderColor READ placeholderColor WRITE
                 setPlaceholderColor NOTIFY themeChanged)

  // ─── TYPOGRAPHY ───────────────────────────────────────────────────
  Q_PROPERTY(int fontSizeNormal READ fontSizeNormal WRITE setFontSizeNormal
                 NOTIFY themeChanged)
  Q_PROPERTY(int fontSizeHeader READ fontSizeHeader WRITE setFontSizeHeader
                 NOTIFY themeChanged)
  Q_PROPERTY(int fontSizeSmall READ fontSizeSmall WRITE setFontSizeSmall NOTIFY
                 themeChanged)

public:
  static ThemeData *instance() {
    static ThemeData m_instance;
    return &m_instance;
  }

  // Getters
  QColor windowBackground() const { return m_windowBackground; }
  QColor panelBackground() const { return m_panelBackground; }
  QColor headerBackground() const { return m_headerBackground; }
  QColor sidebarBackground() const { return m_sidebarBackground; }
  QColor accentColor() const { return m_accentColor; }
  QColor accentHover() const { return m_accentHover; }
  QColor textPrimary() const { return m_textPrimary; }
  QColor textSecondary() const { return m_textSecondary; }
  QColor textMuted() const { return m_textMuted; }

  QColor borderColor() const { return m_borderColor; }
  QColor itemHoverBackground() const { return m_itemHoverBackground; }
  QColor itemSelectedBackground() const { return m_itemSelectedBackground; }

  QColor scrollBarThumb() const { return m_scrollBarThumb; }
  QColor scrollBarThumbHover() const { return m_scrollBarThumbHover; }
  QColor statusOnline() const { return m_statusOnline; }
  QColor statusOffline() const { return m_statusOffline; }

  QColor inputGradientStart() const { return m_inputGradientStart; }
  QColor inputGradientEnd() const { return m_inputGradientEnd; }
  QColor inputSolidBorder() const { return m_inputSolidBorder; }
  QColor inputBackgroundActive() const { return m_inputBackgroundActive; }
  QColor inputBackgroundInactive() const { return m_inputBackgroundInactive; }
  QColor placeholderColor() const { return m_placeholderColor; }

  int fontSizeNormal() const { return m_fontSizeNormal; }
  int fontSizeHeader() const { return m_fontSizeHeader; }
  int fontSizeSmall() const { return m_fontSizeSmall; }

  // Setters
  void setWindowBackground(const QColor &c) {
    if (m_windowBackground != c) {
      m_windowBackground = c;
      emit themeChanged();
    }
  }
  void setPanelBackground(const QColor &c) {
    if (m_panelBackground != c) {
      m_panelBackground = c;
      emit themeChanged();
    }
  }
  void setHeaderBackground(const QColor &c) {
    if (m_headerBackground != c) {
      m_headerBackground = c;
      emit themeChanged();
    }
  }
  void setSidebarBackground(const QColor &c) {
    if (m_sidebarBackground != c) {
      m_sidebarBackground = c;
      emit themeChanged();
    }
  }
  void setAccentColor(const QColor &c) {
    if (m_accentColor != c) {
      m_accentColor = c;
      emit themeChanged();
    }
  }
  void setAccentHover(const QColor &c) {
    if (m_accentHover != c) {
      m_accentHover = c;
      emit themeChanged();
    }
  }
  void setTextPrimary(const QColor &c) {
    if (m_textPrimary != c) {
      m_textPrimary = c;
      emit themeChanged();
    }
  }
  void setTextSecondary(const QColor &c) {
    if (m_textSecondary != c) {
      m_textSecondary = c;
      emit themeChanged();
    }
  }
  void setTextMuted(const QColor &c) {
    if (m_textMuted != c) {
      m_textMuted = c;
      emit themeChanged();
    }
  }

  void setBorderColor(const QColor &c) {
    if (m_borderColor != c) {
      m_borderColor = c;
      emit themeChanged();
    }
  }
  void setItemHoverBackground(const QColor &c) {
    if (m_itemHoverBackground != c) {
      m_itemHoverBackground = c;
      emit themeChanged();
    }
  }
  void setItemSelectedBackground(const QColor &c) {
    if (m_itemSelectedBackground != c) {
      m_itemSelectedBackground = c;
      emit themeChanged();
    }
  }

  void setScrollBarThumb(const QColor &c) {
    if (m_scrollBarThumb != c) {
      m_scrollBarThumb = c;
      emit themeChanged();
    }
  }
  void setScrollBarThumbHover(const QColor &c) {
    if (m_scrollBarThumbHover != c) {
      m_scrollBarThumbHover = c;
      emit themeChanged();
    }
  }
  void setStatusOnline(const QColor &c) {
    if (m_statusOnline != c) {
      m_statusOnline = c;
      emit themeChanged();
    }
  }
  void setStatusOffline(const QColor &c) {
    if (m_statusOffline != c) {
      m_statusOffline = c;
      emit themeChanged();
    }
  }

  void setInputGradientStart(const QColor &c) {
    if (m_inputGradientStart != c) {
      m_inputGradientStart = c;
      emit themeChanged();
    }
  }
  void setInputGradientEnd(const QColor &c) {
    if (m_inputGradientEnd != c) {
      m_inputGradientEnd = c;
      emit themeChanged();
    }
  }
  void setInputSolidBorder(const QColor &c) {
    if (m_inputSolidBorder != c) {
      m_inputSolidBorder = c;
      emit themeChanged();
    }
  }
  void setInputBackgroundActive(const QColor &c) {
    if (m_inputBackgroundActive != c) {
      m_inputBackgroundActive = c;
      emit themeChanged();
    }
  }
  void setInputBackgroundInactive(const QColor &c) {
    if (m_inputBackgroundInactive != c) {
      m_inputBackgroundInactive = c;
      emit themeChanged();
    }
  }
  void setPlaceholderColor(const QColor &c) {
    if (m_placeholderColor != c) {
      m_placeholderColor = c;
      emit themeChanged();
    }
  }

  void setFontSizeNormal(int s) {
    if (m_fontSizeNormal != s) {
      m_fontSizeNormal = s;
      emit themeChanged();
    }
  }
  void setFontSizeHeader(int s) {
    if (m_fontSizeHeader != s) {
      m_fontSizeHeader = s;
      emit themeChanged();
    }
  }
  void setFontSizeSmall(int s) {
    if (m_fontSizeSmall != s) {
      m_fontSizeSmall = s;
      emit themeChanged();
    }
  }

  // ─── OLED BLACK PRESET ────────────────────────────────────────────
  Q_INVOKABLE void loadOledPreset() {
    m_windowBackground = QColor("#0e0f12"); // Content background elevation
    m_panelBackground = QColor("#08080a");  // Channel list panel elevation
    m_headerBackground = QColor("#0d0e11"); // Top titlebar elevation
    m_sidebarBackground =
        QColor("#000000"); // Pitch Black for navigation sidebar & brand button
    m_accentColor = QColor("#5865f2");
    m_accentHover = QColor("#4752c4");
    m_textPrimary = QColor("#f2f3f5");
    m_textSecondary = QColor("#949ba4");
    m_textMuted = QColor("#6d6f78");

    m_borderColor = QColor("#18191d");
    m_itemHoverBackground = QColor("#141518");
    m_itemSelectedBackground = QColor("#1e1f24");

    m_scrollBarThumb = QColor("#2b2d31");
    m_scrollBarThumbHover = QColor("#3f4248");
    m_statusOnline = QColor("#23a55a");
    m_statusOffline = QColor("#80848e");

    m_inputGradientStart = QColor("#5865f2");
    m_inputGradientEnd = QColor("#7289da");
    m_inputSolidBorder = QColor("#18191d");
    m_inputBackgroundActive = QColor("#000000");
    m_inputBackgroundInactive = QColor("#08080a");
    m_placeholderColor = QColor("#6d6f78");

    emit themeChanged();
  }

  // ─── SOFT CHARCOAL PRESET ─────────────────────────────────────────
  Q_INVOKABLE void loadSoftDarkPreset() {
    m_windowBackground = QColor("#1e1f22");
    m_panelBackground = QColor("#2b2d31");
    m_headerBackground = QColor("#313338");
    m_sidebarBackground = QColor("#111214"); // Darkest element in soft preset
    m_accentColor = QColor("#5865f2");
    m_accentHover = QColor("#4752c4");
    m_textPrimary = QColor("#dbdee1");
    m_textSecondary = QColor("#949ba4");
    m_textMuted = QColor("#6d6f78");

    m_borderColor = QColor("#1f2023");
    m_itemHoverBackground = QColor("#35373c");
    m_itemSelectedBackground = QColor("#404249");

    m_scrollBarThumb = QColor("#3f4248");
    m_scrollBarThumbHover = QColor("#4e5058");
    m_statusOnline = QColor("#23a55a");
    m_statusOffline = QColor("#80848e");

    m_inputGradientStart = QColor("#5865f2");
    m_inputGradientEnd = QColor("#7289da");
    m_inputSolidBorder = QColor("#383a40");
    m_inputBackgroundActive = QColor("#1e1f22");
    m_inputBackgroundInactive = QColor("#2b2d31");
    m_placeholderColor = QColor("#80848e");

    emit themeChanged();
  }

signals:
  void themeChanged();

private:
  explicit ThemeData(QObject *parent = nullptr) : QObject(parent) {
    loadOledPreset(); // Default initialized to OLED Black
  }

  ThemeData(const ThemeData &) = delete;
  ThemeData &operator=(const ThemeData &) = delete;

  QColor m_windowBackground;
  QColor m_panelBackground;
  QColor m_headerBackground;
  QColor m_sidebarBackground;
  QColor m_accentColor;
  QColor m_accentHover;
  QColor m_textPrimary;
  QColor m_textSecondary;
  QColor m_textMuted;

  QColor m_borderColor;
  QColor m_itemHoverBackground;
  QColor m_itemSelectedBackground;

  QColor m_scrollBarThumb;
  QColor m_scrollBarThumbHover;
  QColor m_statusOnline;
  QColor m_statusOffline;

  QColor m_inputGradientStart;
  QColor m_inputGradientEnd;
  QColor m_inputSolidBorder;
  QColor m_inputBackgroundActive;
  QColor m_inputBackgroundInactive;
  QColor m_placeholderColor;

  int m_fontSizeNormal = 14;
  int m_fontSizeHeader = 18;
  int m_fontSizeSmall = 11;
};