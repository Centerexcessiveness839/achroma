#pragma once
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

class QWebEngineView;
class BrowserTabs;
class Terminal;
class QFileSystemWatcher;

struct CommandMeta
{
    QString name;
    QString description;
    QString argHint;   // e.g. "<url>", "[name]", ""
    QString category;  // navigation | search | terminal | tools | github | web
    QStringList aliases;
};

struct AppConfig
{
    QString bg = "#000000";
    QString fg = "#FFFFFF";
    QString fontFamily = "Source Code Pro";
    int fontSize = 18;
    bool darkMode = false;
    QString qssFile;
};
#include "core/utils.h"

struct HomepageConfig
{
    QString source = "builtin";
    QList<Achroma::QuickLink> quickLinks;
};

struct DevConfig
{
    QMap<QString, QString> runners;
    QString manUrl = "https://man7.org/linux/man-pages/man{section}/{name}.{section}.html";
    QString tldrUrl = "https://tldr.inbrowser.app/pages/common/{name}";
    QMap<QString, QString> docsUrls;
    QMap<QString, QString> fileHandlers;
    QStringList searchDirs = {"~/projects", "~/src"};
    QString editor = "nvim";
};

struct KeyConfig
{
    QString focusTerminal = "Ctrl+T";
    QString toggleTerminal = "Ctrl+J";
    QString focusUrlbar = "Ctrl+Shift+L";
    QString toggleSplit = "Ctrl+O";
    QString newTabFromUrlbar = "Ctrl+Return";
    QString copySelection = "Ctrl+Shift+T";
    QString linkHints = "Ctrl+Shift+F";
    QString showHelp = "Ctrl+Shift+H";
    QString closeHelp = "Escape";
    QString reopenClosedTab = "Ctrl+Shift+U";
    QString nextTab = "Ctrl+Tab";
    QString prevTab = "Ctrl+Shift+Tab";
    QString findInPage = "Ctrl+F";
    QString terminalCopy = "Ctrl+Shift+C";
    QString terminalPaste = "Ctrl+Shift+V";
    QString terminalClear = "Ctrl+Shift+K";
    QString terminalSearch = "Ctrl+Shift+G";
    QString terminalZoomIn = "Ctrl++";
    QString terminalZoomOut = "Ctrl+-";
    QString terminalZoomReset = "Ctrl+0";
    QString resizeUp = "Ctrl+Up";
    QString resizeDown = "Ctrl+Down";
    QString resizeLeft = "Ctrl+Left";
    QString resizeRight = "Ctrl+Right";
    QString fullscreen = "F11";
    QString devTools = "F12";
    QString toggleDarkMode = "Ctrl+Shift+D";
    QString toggleAdBlock = "Ctrl+Shift+A";
    QString viewSource = "Ctrl+U";
    QString printPage = "Ctrl+P";
    QString toggleReader = "Ctrl+Shift+R";
    QString toggleBmbar = "Ctrl+Shift+B";
    QString sendToTerm = "Ctrl+Shift+E";
    QString codeBlock = "Ctrl+Shift+.";
    QString installCmd = "Ctrl+Shift+I";
    QString fuzzyFinder = "Ctrl+Shift+P";
    QString systemBrowser = "Ctrl+Shift+O";
};

class CommandDispatcher : public QObject
{
    Q_OBJECT
public:
    explicit CommandDispatcher(QObject* parent = nullptr);

    void setBrowser(BrowserTabs* browser)
    {
        m_browser = browser;
    }
    void setTerminal(Terminal* terminal)
    {
        m_terminal = terminal;
    }

    void execute(const QString& raw);
    void loadConfig();
    void setAdBlockEnabled(bool on)
    {
        m_adBlockEnabled = on;
    }
    bool adBlockEnabled() const
    {
        return m_adBlockEnabled;
    }

    QJsonArray triggers() const
    {
        return m_mergedTriggers;
    }
    const QList<CommandMeta>& builtinCommands() const
    {
        return m_builtinMeta;
    }
    const AppConfig& appearance() const
    {
        return m_appearance;
    }
    const HomepageConfig& homepage() const
    {
        return m_homepage;
    }
    const DevConfig& devConfig() const
    {
        return m_dev;
    }
    const KeyConfig& keyConfig() const
    {
        return m_keyConfig;
    }
    const QMap<QString, QString>& searchEngines() const
    {
        return m_searchEngines;
    }
    const QMap<QString, QJsonObject>& customCommands() const
    {
        return m_customCommands;
    }

    std::function<void()> showHelpCallback;
    std::function<void()> showHintsCallback;
    std::function<void()> fullscreenCallback;
    std::function<void()> fuzzyCallback;
    std::function<void(bool)> adBlockToggleCallback;

signals:
    void configChanged();

private:
    void setupBuiltins();
    void watchConfig(const QString& path);

    using CommandFn = std::function<void(QWebEngineView*, QString)>;
    QHash<QString, CommandFn> m_dispatch;
    QList<CommandMeta> m_builtinMeta;
    QMap<QString, QJsonObject> m_customCommands;
    QMap<QString, QString> m_searchEngines;
    QJsonArray m_mergedTriggers;
    AppConfig m_appearance;
    HomepageConfig m_homepage;
    DevConfig m_dev;
    KeyConfig m_keyConfig;
    bool m_adBlockEnabled = true;
    QFileSystemWatcher* m_watcher = nullptr;

    BrowserTabs* m_browser = nullptr;
    Terminal* m_terminal = nullptr;
};
