#pragma once
#include <QDir>
#include <QList>
#include <QMap>
#include <QString>

class QWebEngineProfile;

namespace Achroma
{

namespace Theme
{

constexpr const char* bg = "#080808";
constexpr const char* bg1 = "#0d0d0d";
constexpr const char* bg2 = "#111111";
constexpr const char* border = "#1e1e1e";
constexpr const char* border2 = "#333333";
constexpr const char* fg = "#aaaaaa";
constexpr const char* fgBright = "#ffffff";
constexpr const char* fgDim = "#555555";
constexpr const char* fgHint = "#383838";
constexpr const char* accent = "#8fb6ff";

inline QString scrollBar()
{
    return "QScrollBar:vertical { width: 4px; background: transparent; margin: 0; border: none; }"
           "QScrollBar::handle:vertical { background: #2a2a2a; border-radius: 2px; min-height: 20px; }"
           "QScrollBar::handle:vertical:hover { background: #3a3a3a; }"
           "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
           "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }";
}

inline QString monoLabel(const char* color = "#aaaaaa", int px = 11)
{
    return QString("color: %1; font-family: monospace; font-size: %2px; border: none;").arg(color).arg(px);
}

inline QString inputField()
{
    return "QLineEdit { background: #111111; color: #ffffff; border: 1px solid #333333; "
           "font-family: monospace; font-size: 14px; padding: 6px 8px; }";
}

inline QString listView()
{
    return "QListWidget { background: #0d0d0d; color: #aaaaaa; border: none; "
           "font-family: monospace; font-size: 12px; outline: none; }"
           "QListWidget::item { padding: 4px 8px; }"
           "QListWidget::item:selected { background: #1e1e1e; color: #ffffff; }" +
           scrollBar();
}

inline QString panelFrame()
{
    return "QFrame { background-color: #0d0d0d; border: 1px solid #333333; border-radius: 6px; }";
}

inline QString actionButton(const char* textColor = "#777777")
{
    return QString(
               "QPushButton { color: %1; background: transparent; border: 1px solid #252525; "
               "border-radius: 3px; padding: 4px 12px; min-width: 60px; "
               "font-family: monospace; font-size: 11px; }"
               "QPushButton:hover { background: #101010; border-color: #3a3a3a; color: #dddddd; }"
    )
        .arg(textColor);
}

}  // namespace Theme

struct QuickLink
{
    QString name;
    QString url;
    QString icon;
};

QString configDir();
QString dataDir();
QString formatUrl(const QString& input);
QString searchUrl(const QString& engine, const QString& query);
QString duckDuckGoUrl(const QString& query);
QString urlEncode(const QString& text);
QString stripTerminalControls(QString text);
QString homePageHtml(const QMap<QString, QString>& bookmarks = {}, const QList<QuickLink>& quickLinks = {});
QString markdownPageHtml(const QString& title, const QString& markdown);
QString linkHintsScript();
QString keyScrollScript();
QString codeBlockScript();
QString installCmdScript();

extern const QMap<QString, QString> builtinSearchEngines;

QWebEngineProfile* mainProfile();

}  // namespace Achroma
