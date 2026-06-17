#include "commands.h"
#include "core/utils.h"
#include "net/cookieimport.h"
#include "net/headerdump.h"
#include "term/terminal.h"
#include "ui/browser.h"
#include <QColor>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLineEdit>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QWebEngineCookieStore>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>

CommandDispatcher::CommandDispatcher(QObject* parent) : QObject(parent)
{
    m_searchEngines = Achroma::builtinSearchEngines;
    setupBuiltins();
    loadConfig();
}

void CommandDispatcher::execute(const QString& raw)
{
    QStringList parts = raw.split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return;
    QString cmd = parts[0];
    QString arg = parts.mid(1).join(' ');
    QWebEngineView* v = m_browser ? m_browser->currentView() : nullptr;

    if (auto it = m_dispatch.constFind(cmd); it != m_dispatch.constEnd())
    {
        (*it)(v, arg);
        return;
    }
    if (m_customCommands.contains(cmd) && !arg.isEmpty())
    {
        QJsonObject c = m_customCommands[cmd];
        QString action = c["action"].toString();
        if (action == "search")
        {
            QString engine = c["engine"].toString("https://duckduckgo.com/?q={{arg}}");
            if (m_browser)
                m_browser->navigateOrNewTab(QUrl(Achroma::searchUrl(engine, arg)));
        }
        else if (action == "open")
        {
            if (m_browser)
                m_browser->navigateOrNewTab(QUrl(Achroma::formatUrl(arg)));
        }
        else if (action == "tab")
        {
            if (m_browser)
                m_browser->addNewTab(QUrl(Achroma::formatUrl(arg)));
        }
        else if (action == "js")
        {
            if (v)
            {
                QString escaped = arg;
                escaped.replace("\\", "\\\\").replace("'", "\\'").replace("\"", "\\\"");
                v->page()->runJavaScript(c["code"].toString().replace("{{arg}}", escaped));
            }
        }
        return;
    }
    if (m_searchEngines.contains(cmd) && !arg.isEmpty())
    {
        if (m_browser)
            m_browser->navigateOrNewTab(QUrl(Achroma::searchUrl(m_searchEngines[cmd], arg)));
    }
}

void CommandDispatcher::setupBuiltins()
{
    m_dispatch["help"] = [this](QWebEngineView*, const QString&)
    {
        if (showHelpCallback)
            showHelpCallback();
    };

    m_dispatch["fullscreen"] = [this](QWebEngineView*, const QString&)
    {
        if (fullscreenCallback)
            fullscreenCallback();
    };

    m_dispatch["fuzzy"] = m_dispatch["finder"] = [this](QWebEngineView*, const QString&)
    {
        if (fuzzyCallback)
            fuzzyCallback();
    };

    auto doOpen = [this](QWebEngineView* v, const QString& arg)
    {
        if (arg.isEmpty() || !m_browser)
            return;
        if (arg.startsWith("bookmark:"))
        {
            QSettings s("Achroma", "Achroma");
            s.beginGroup("bookmarks");
            QString url = s.value(arg.mid(9)).toString();
            s.endGroup();
            if (!url.isEmpty())
                m_browser->navigateOrNewTab(QUrl(url));
        }
        else
        {
            m_browser->navigateOrNewTab(QUrl(Achroma::formatUrl(arg)));
        }
    };
    m_dispatch["open"] = m_dispatch["o"] = doOpen;

    m_dispatch["tab"] = m_dispatch["t"] = [this](QWebEngineView*, const QString& arg)
    {
        if (!arg.isEmpty() && m_browser)
            m_browser->addNewTab(QUrl(Achroma::formatUrl(arg)));
    };

    m_dispatch["search"] = m_dispatch["s"] = [this](QWebEngineView*, const QString& arg)
    {
        if (!arg.isEmpty() && m_browser)
            m_browser->navigateOrNewTab(QUrl(Achroma::duckDuckGoUrl(arg)));
    };

    m_dispatch["goto"] = [this](QWebEngineView*, const QString& arg)
    {
        bool ok;
        int idx = arg.toInt(&ok);
        if (ok && m_browser)
            m_browser->switchToTab(idx);
    };

    m_dispatch["g"] = [this](QWebEngineView*, const QString& arg)
    {
        bool ok;
        int idx = arg.toInt(&ok);
        if (ok && m_browser)
        {
            m_browser->switchToTab(idx);
        }
        else if (!arg.isEmpty() && m_browser)
        {
            m_browser->navigateOrNewTab(
                QUrl(Achroma::searchUrl(m_searchEngines.value("g", "https://google.com/search?q="), arg))
            );
        }
    };

    m_dispatch["back"] = m_dispatch["b"] = [](QWebEngineView* v, const QString&)
    {
        if (v)
            v->back();
    };

    m_dispatch["forward"] = m_dispatch["f"] = [](QWebEngineView* v, const QString&)
    {
        if (v)
            v->forward();
    };

    m_dispatch["reload"] = [this](QWebEngineView*, const QString&)
    {
        loadConfig();
        if (m_terminal)
            m_terminal->sendText("# config reloaded\n");
    };

    m_dispatch["r"] = [](QWebEngineView* v, const QString&)
    {
        if (v)
            v->reload();
    };

    m_dispatch["home"] = [this](QWebEngineView*, const QString&)
    {
        if (m_browser)
            m_browser->switchToHomeTab();
    };

    m_dispatch["close"] = m_dispatch["c"] = [this](QWebEngineView*, const QString&)
    {
        if (m_browser)
            m_browser->closeCurrentTab();
    };

    m_dispatch["undo"] = m_dispatch["u"] = [this](QWebEngineView*, const QString&)
    {
        if (m_browser)
            m_browser->reopenLastTab();
    };

    m_dispatch["duplicate"] = m_dispatch["dup"] = [this](QWebEngineView*, const QString&)
    {
        if (m_browser)
            m_browser->duplicateTab();
    };

    m_dispatch["incognito"] = m_dispatch["incog"] = [this](QWebEngineView*, const QString& arg)
    {
        if (m_browser)
            m_browser->addIncognitoTab(QUrl(arg.isEmpty() ? QString() : Achroma::formatUrl(arg)));
    };

    m_dispatch["next"] = m_dispatch["n"] = [this](QWebEngineView*, const QString&)
    {
        if (m_browser)
            m_browser->nextTab();
    };

    m_dispatch["prev"] = m_dispatch["p"] = [this](QWebEngineView*, const QString&)
    {
        if (m_browser)
            m_browser->prevTab();
    };

    m_dispatch["tabs"] = [this](QWebEngineView*, const QString&)
    {
        if (!m_browser || !m_terminal)
            return;
        QStringList titles;
        for (int i = 0; i < m_browser->tabCount(); i++)
            titles += QString::number(i) + ": " + m_browser->tabText(i);
        m_terminal->sendText("# " + titles.join(", ") + "\n");
    };

    m_dispatch["url"] = [this](QWebEngineView* v, const QString&)
    {
        if (!v || !m_browser || !m_terminal)
            return;
        m_browser->setDetectedUrl(v->url().toString());
        m_terminal->sendText("# " + m_browser->detectedUrl() + "\n");
    };

    m_dispatch["focus"] = [this](QWebEngineView*, const QString&)
    {
        if (m_browser)
        {
            m_browser->urlBar()->setFocus();
            m_browser->urlBar()->selectAll();
        }
    };

    m_dispatch["lasturl"] = [this](QWebEngineView* v, const QString&)
    {
        if (!m_browser)
            return;
        if (!m_browser->detectedUrl().isEmpty() && v)
            v->setUrl(QUrl(m_browser->detectedUrl()));
    };

    auto doBookmark = [this](QWebEngineView* v, const QString& arg)
    {
        if (arg.isEmpty() || !v)
            return;
        QSettings s("Achroma", "Achroma");
        s.beginGroup("bookmarks");
        s.setValue(arg, v->url().toString());
        s.endGroup();
        if (m_browser)
            m_browser->refreshBookmarkBar();
        if (m_terminal)
            m_terminal->sendText("# bookmarked: " + arg + "\n");
    };
    m_dispatch["bookmark"] = m_dispatch["bm"] = doBookmark;

    m_dispatch["bookmarks"] = [this](QWebEngineView*, const QString&)
    {
        if (!m_terminal)
            return;
        QSettings s("Achroma", "Achroma");
        s.beginGroup("bookmarks");
        QStringList lines;
        for (const QString& key : s.childKeys())
            lines += key + ": " + s.value(key).toString();
        s.endGroup();
        m_terminal->sendText("# " + lines.join(", ") + "\n");
    };

    m_dispatch["hint"] = [this](QWebEngineView*, const QString&)
    {
        if (showHintsCallback)
            showHintsCallback();
    };

    m_dispatch["find"] = [this](QWebEngineView*, const QString&)
    {
        if (m_browser)
            m_browser->setFindBarVisible(true);
    };

    m_dispatch["clear"] = [this](QWebEngineView*, const QString&)
    {
        if (m_terminal)
            m_terminal->clear();
    };

    m_dispatch["copy"] = [this](QWebEngineView*, const QString&)
    {
        if (m_terminal)
            m_terminal->copy();
    };

    m_dispatch["paste"] = [this](QWebEngineView*, const QString&)
    {
        if (m_terminal)
            m_terminal->paste();
    };

    m_dispatch["searchterm"] = m_dispatch["sterm"] = [this](QWebEngineView*, const QString&)
    {
        if (m_terminal)
            m_terminal->toggleSearchBar();
    };

    m_dispatch["profile"] = [this](QWebEngineView*, const QString& name)
    {
        if (!m_terminal)
            return;
        if (name.isEmpty())
        {
            QStringList profiles = Terminal::availableProfiles();
            m_terminal->sendText("# profiles: " + profiles.join(", ") + "\n");
        }
        else
        {
            m_terminal->setProfile(name);
            m_terminal->sendText("# profile: " + name + "\n");
        }
    };

    m_dispatch["bmbar"] = [this](QWebEngineView*, const QString&)
    {
        if (m_browser)
            m_browser->toggleBookmarkBar();
    };

    m_dispatch["pin"] = [this](QWebEngineView*, const QString&)
    {
        if (m_browser)
            m_browser->pinTab();
    };

    m_dispatch["run"] = [this](QWebEngineView*, const QString&)
    {
        if (!m_terminal)
            return;

        static const QStringList exts = {
            "cpp", "cxx", "c", "rs", "py", "js", "sh", "go", "rb", "lua", "zig", "kt", "ts"
        };
        QString snippet;
        for (const QString& ext : exts)
        {
            QString path = "/tmp/achroma-snippet." + ext;
            if (QFileInfo::exists(path))
            {
                snippet = path;
                break;
            }
        }
        if (snippet.isEmpty())
        {
            m_terminal->sendText("# no snippet found. Use Ctrl+Shift+. on a code block first.\n");
            return;
        }
        QString ext = QFileInfo(snippet).suffix();
        QString cmd = m_dev.runners.value(ext);
        if (cmd.isEmpty())
        {
            m_terminal->sendText("# no runner configured for ." + ext + "\n");
            return;
        }
        cmd.replace("{file}", snippet);
        m_terminal->sendText(cmd + "\n");
    };

    auto gitOpen = [this](const QString& path)
    {
        QWebEngineView* v = m_browser ? m_browser->currentView() : nullptr;
        if (!v)
            return;
        QString u = v->url().toString();
        QRegularExpression re("github\\.com/([^/]+/[^/?#]+)");
        auto m = re.match(u);
        if (m.hasMatch())
        {
            QString base = "https://github.com/" + m.captured(1);
            v->setUrl(QUrl(base + path));
        }
    };
    m_dispatch["issues"] = [gitOpen](QWebEngineView*, const QString&)
    {
        gitOpen("/issues");
    };
    m_dispatch["prs"] = [gitOpen](QWebEngineView*, const QString&)
    {
        gitOpen("/pulls");
    };
    m_dispatch["actions"] = [gitOpen](QWebEngineView*, const QString&)
    {
        gitOpen("/actions");
    };
    m_dispatch["blame"] = [this](QWebEngineView*, const QString&)
    {
        QWebEngineView* v = m_browser ? m_browser->currentView() : nullptr;
        if (!v)
            return;
        QString u = v->url().toString().replace("blob/", "blame/");
        v->setUrl(QUrl(u));
    };
    m_dispatch["permalink"] = [this](QWebEngineView*, const QString&)
    {
        QWebEngineView* v = m_browser ? m_browser->currentView() : nullptr;
        if (!v)
            return;
        v->page()->runJavaScript(
            R"(
            (function(){
                var l = document.querySelector('.js-permalink-shortcut');
                if (l) return l.href;
                return location.href;
            })();
        )",
            [this](const QVariant& res)
            {
                QString url = res.toString();
                if (!url.isEmpty() && m_terminal)
                    m_terminal->sendText(url + "\n");
            }
        );
    };

    auto docLookup = [this](const QMap<QString, QString>& urls, const QString& name)
    {
        QWebEngineView* v = m_browser ? m_browser->currentView() : nullptr;
        if (!v || name.isEmpty())
            return;
        QString matched;
        for (auto it = urls.begin(); it != urls.end(); ++it)
        {
            if (name.startsWith(it.key() + " "))
            {
                QString term = name.mid(it.key().length() + 1);
                matched = QString(it.value()).replace("{name}", QUrl::toPercentEncoding(term));
                break;
            }
        }
        if (!matched.isEmpty())
        {
            v->setUrl(QUrl(matched));
            return;
        }

        if (!urls.isEmpty())
        {
            matched = QString(urls.first()).replace("{name}", QUrl::toPercentEncoding(name));
            v->setUrl(QUrl(matched));
        }
    };

    m_dispatch["docs"] = [this, docLookup](QWebEngineView*, const QString& arg)
    {
        docLookup(m_dev.docsUrls, arg);
    };

    m_dispatch["man"] = [this](QWebEngineView*, const QString& arg)
    {
        QWebEngineView* v = m_browser ? m_browser->currentView() : nullptr;
        if (!v || arg.isEmpty())
            return;

        int section = 3;
        QString name = arg;
        QRegularExpression re("^(\\d+)?\\s*(\\S+)");
        auto m = re.match(arg);
        if (m.hasMatch())
        {
            if (!m.captured(1).isEmpty())
                section = m.captured(1).toInt();
            name = m.captured(2);
        }
        QString url = m_dev.manUrl;
        url.replace("{section}", QString::number(section));
        url.replace("{name}", name);
        v->setUrl(QUrl(url));
    };

    m_dispatch["tldr"] = [this](QWebEngineView*, const QString& arg)
    {
        QWebEngineView* v = m_browser ? m_browser->currentView() : nullptr;
        if (!v || arg.isEmpty())
            return;
        QString url = m_dev.tldrUrl;
        url.replace("{name}", arg);
        v->setUrl(QUrl(url));
    };

    m_dispatch["notes"] = [this](QWebEngineView*, const QString&)
    {
        if (!m_browser)
            return;
        QString notesPath = Achroma::configDir() + "/notes.txt";
        QString content;
        QFile nf(notesPath);
        if (nf.open(QIODevice::ReadOnly))
        {
            content = QString::fromUtf8(nf.readAll()).toHtmlEscaped();
            nf.close();
        }
        QString html =
            "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Notes</title></head>"
            "<body style='background:#080808;color:#ccc;font-family:monospace;margin:0;padding:20px;'>"
            "<textarea id='notes' style='width:100%;height:calc(100vh - 40px);background:#0d0d0d;"
            "color:#fff;border:none;outline:none;font-family:monospace;font-size:14px;resize:none;padding:12px;'>" +
            content +
            "</textarea>"
            "<script>"
            "var save=function(){var x=new XMLHttpRequest();"
            "x.open('POST','achroma://save-notes',false);x.send(document.getElementById('notes').value);};"
            "document.getElementById('notes').addEventListener('input',function(){clearTimeout(this._t);this._t="
            "setTimeout(save,1000);});"
            "</script></body></html>";
        m_browser->addNewTab(QUrl());
        QWebEngineView* nv = m_browser->currentView();
        if (nv)
            nv->setHtml(html, QUrl("achroma://notes"));
    };

    m_dispatch["markdown"] = m_dispatch["md"] = [this](QWebEngineView*, const QString& arg)
    {
        if (!m_browser || arg.isEmpty())
            return;
        QString path = arg.trimmed();
        if (path.startsWith('~'))
            path = QDir::homePath() + path.mid(1);
        if (!path.startsWith('/'))
        {
            QString cwd = m_terminal ? m_terminal->shellCwd() : QString();
            if (!cwd.isEmpty())
                path = QDir(cwd).absoluteFilePath(path);
            else
                path = QFileInfo(path).absoluteFilePath();
        }
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
        {
            if (m_terminal)
                m_terminal->sendText("# markdown: cannot open " + path + "\n");
            return;
        }
        const QString markdown = QString::fromUtf8(f.readAll());
        f.close();

        const QString html = Achroma::markdownPageHtml(QFileInfo(path).fileName(), markdown);

        m_browser->addNewTab(QUrl("about:blank"));
        QWebEngineView* nv = m_browser->currentView();
        if (nv)
            nv->setHtml(html, QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()));
    };

    m_dispatch["session"] = [this](QWebEngineView*, const QString& arg)
    {
        if (!m_browser || !m_terminal)
            return;
        QStringList parts = arg.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty())
        {
            m_terminal->sendText("# usage: :session save|load|list <name>\n");
            return;
        }
        QString cmd = parts[0];
        QString name = parts.mid(1).join(' ');
        QString sessionsDir = Achroma::configDir() + "/sessions";
        QDir().mkpath(sessionsDir);

        if (cmd == "list")
        {
            QDir dir(sessionsDir);
            QStringList files = dir.entryList({"*.json"}, QDir::Files, QDir::Name);
            m_terminal->sendText("# sessions: " + files.join(", ").replace(".json", "") + "\n");
        }
        else if (cmd == "save" && !name.isEmpty())
        {
            QJsonArray tabs;
            for (int i = 0; i < m_browser->tabCount(); i++)
            {
                QJsonObject tab;
                tab["url"] = m_browser->urlForTab(i);
                tab["pinned"] = m_browser->isPinned(i);
                tabs.append(tab);
            }
            QJsonObject root;
            root["tabs"] = tabs;
            QString cwd = m_terminal ? m_terminal->shellCwd() : QString();
            if (!cwd.isEmpty())
            {
                QJsonObject termState;
                termState["cwd"] = cwd;
                root["terminal"] = termState;
            }
            QFile f(sessionsDir + "/" + name + ".json");
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
            m_terminal->sendText("# session '" + name + "' saved (" + QString::number(tabs.size()) + " tabs)\n");
        }
        else if (cmd == "load" && !name.isEmpty())
        {
            QFile f(sessionsDir + "/" + name + ".json");
            if (!f.open(QIODevice::ReadOnly))
            {
                m_terminal->sendText("# session not found: " + name + "\n");
                return;
            }
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject() && doc.object().contains("tabs"))
            {
                QJsonArray tabs = doc.object()["tabs"].toArray();
                for (const QJsonValue& v : tabs)
                {
                    QJsonObject t = v.toObject();
                    QUrl url(t["url"].toString());
                    if (url.isValid())
                        m_browser->addNewTab(url);
                }
                if (doc.object().contains("terminal") && m_terminal)
                {
                    QString cwd = doc.object()["terminal"].toObject()["cwd"].toString();
                    if (!cwd.isEmpty())
                        m_terminal->sendText("cd " + cwd + "\n");
                }
                m_terminal->sendText("# session '" + name + "' loaded (" + QString::number(tabs.size()) + " tabs)\n");
            }
        }
    };

    auto sendToTerm = [this](const QString& js)
    {
        QWebEngineView* v = m_browser ? m_browser->currentView() : nullptr;
        if (!v || !m_terminal)
            return;
        v->page()->runJavaScript(
            js,
            [this](const QVariant& res)
            {
                QString text = res.toString().trimmed();
                if (text.isEmpty())
                    return;

                QJsonParseError err;
                QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &err);
                QString lang, code;
                if (err.error == QJsonParseError::NoError && doc.isObject())
                {
                    QJsonObject obj = doc.object();
                    lang = obj["lang"].toString();
                    code = obj["code"].toString();
                }
                else
                {
                    code = text;
                }

                static const QMap<QString, QString> extMap = {
                    {"cpp", "cpp"},     {"cxx", "cpp"},       {"c++", "cpp"},
                    {"cc", "cpp"},      {"c", "c"},           {"h", "h"},
                    {"python", "py"},   {"py", "py"},         {"javascript", "js"},
                    {"js", "js"},       {"typescript", "ts"}, {"ts", "ts"},
                    {"rust", "rs"},     {"rs", "rs"},         {"go", "go"},
                    {"java", "java"},   {"ruby", "rb"},       {"rb", "rb"},
                    {"lua", "lua"},     {"zig", "zig"},       {"swift", "swift"},
                    {"kotlin", "kt"},   {"kt", "kt"},         {"scala", "scala"},
                    {"haskell", "hs"},  {"hs", "hs"},         {"sh", "sh"},
                    {"bash", "sh"},     {"shell", "sh"},      {"zsh", "sh"},
                    {"html", "html"},   {"css", "css"},       {"yaml", "yml"},
                    {"yml", "yml"},     {"toml", "toml"},     {"json", "json"},
                    {"makefile", "mk"}, {"make", "mk"},       {"dockerfile", "Dockerfile"},
                    {"sql", "sql"},     {"php", "php"},       {"perl", "pl"},
                    {"pl", "pl"},       {"r", "r"},
                };

                QString ext = extMap.value(lang.toLower(), "txt");
                QString path = "/tmp/achroma-snippet." + ext;
                QFile f(path);
                if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                {
                    f.write(code.toUtf8());
                    f.close();
                    m_terminal->sendText("# " + path + "\n");
                }
            }
        );
    };

    m_dispatch["tosterm"] = [sendToTerm](QWebEngineView*, const QString&)
    {
        sendToTerm(
            "window.getSelection().toString().trim() || document.activeElement && document.activeElement.value ? "
            "document.activeElement.value : ''"
        );
    };

    m_dispatch["codeblock"] = [this, sendToTerm](QWebEngineView*, const QString&)
    {
        sendToTerm(Achroma::codeBlockScript());
    };

    m_dispatch["install"] = [this, sendToTerm](QWebEngineView*, const QString&)
    {
        sendToTerm(Achroma::installCmdScript());
    };

    m_dispatch["devtools"] = m_dispatch["dev"] = [](QWebEngineView* v, const QString&)
    {
        if (v)
            v->triggerPageAction(QWebEnginePage::InspectElement);
    };

    m_dispatch["system"] = [this](QWebEngineView* v, const QString&)
    {
        if (!v || !m_browser)
            return;
        const QUrl url = v->url();
        if (url.isValid() && !url.isEmpty())
            QDesktopServices::openUrl(url);
    };

    m_dispatch["headers"] = [this](QWebEngineView*, const QString&)
    {
        if (!m_browser)
            return;
        auto* tester = new HeaderDump(m_browser);
        tester->start();
        m_browser->navigateOrNewTab(QUrl(QString("http://127.0.0.1:%1/").arg(tester->port())));
    };

    m_dispatch["import-cookies"] = m_dispatch["cookies"] = [this](QWebEngineView*, const QString& arg)
    {
        auto* profile = Achroma::mainProfile();
        QStringList log;
        const QString trimmedArg = arg.trimmed();
        const QString source = trimmedArg.toLower();

        if (source == "status")
        {
            QString policy = "unknown";
            switch (profile->persistentCookiesPolicy())
            {
                case QWebEngineProfile::NoPersistentCookies:
                    policy = "none";
                    break;
                case QWebEngineProfile::AllowPersistentCookies:
                    policy = "persistent only";
                    break;
                case QWebEngineProfile::ForcePersistentCookies:
                    policy = "force session + persistent";
                    break;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
                case QWebEngineProfile::OnlyPersistentCookies:
                    policy = "only persistent";
                    break;
#endif
            }
            log << "cookie policy: " + policy;
            log << QString("off the record: %1").arg(profile->isOffTheRecord() ? "yes" : "no");
            log << "storage: " + profile->persistentStoragePath();
            log << "cache: " + profile->cachePath();
            if (m_terminal)
                m_terminal->sendText("# " + log.join("\n# ") + "\n");
            return;
        }

        if (source == "clear")
        {
            auto* store = profile->cookieStore();
            if (store)
            {
                store->deleteAllCookies();
                log << "all cookies cleared";
                log << "re-import with :import-cookies file <path>";
            }
            else
            {
                log << "cookie store not available";
            }
            if (m_terminal)
                m_terminal->sendText("# " + log.join("\n# ") + "\n");
            return;
        }

        QString filePath;
        if (source.startsWith("file "))
            filePath = trimmedArg.mid(5).trimmed();
        else if (source.endsWith(".txt") || source.endsWith(".cookies"))
            filePath = trimmedArg;
        if (!filePath.isEmpty())
        {
            if (filePath.startsWith('~'))
                filePath = QDir::homePath() + filePath.mid(1);
            if (!filePath.startsWith('/'))
            {
                const QString cwd = m_terminal ? m_terminal->shellCwd() : QString();
                filePath =
                    cwd.isEmpty() ? QFileInfo(filePath).absoluteFilePath() : QDir(cwd).absoluteFilePath(filePath);
            }

            int n = CookieImporter::importFromNetscapeFile(profile, filePath);
            if (n > 0)
            {
                log << "imported " + QString::number(n) + " cookie(s) from " + filePath;
                log << "reload Google tabs or open accounts.google.com";
            }
            else
            {
                log << "no cookies imported from " + filePath;
                log << "expected Netscape cookies.txt format";
            }
            if (m_terminal)
                m_terminal->sendText("# " + log.join("\n# ") + "\n");
            return;
        }

        const bool tryFirefox = source.isEmpty() || source == "firefox" || source == "ff";
        const bool tryChrome = source.isEmpty() || source == "chrome" || source == "chromium" || source == "brave";

        if (tryFirefox)
        {
            const QString ffPath = CookieImporter::findFirefoxCookiesPath();
            if (!ffPath.isEmpty())
            {
                log << "trying Firefox: " + ffPath;
                int n = CookieImporter::importFromFirefox(profile);
                if (n > 0)
                {
                    log << "imported " + QString::number(n) + " cookie(s) from Firefox";
                    log << "reload Google tabs or open accounts.google.com";
                    if (m_terminal)
                        m_terminal->sendText("# " + log.join("\n# ") + "\n");
                    return;
                }
                log << "Firefox returned " + QString::number(n);
            }
            else
            {
                log << "no Firefox profile found";
            }
        }

        if (tryChrome)
        {
            const QString chromePath = CookieImporter::findChromeCookiesPath();
            if (!chromePath.isEmpty())
            {
                log << "trying Chromium: " + chromePath;
                int n = CookieImporter::importFromChrome(profile);
                log << "Chromium returned " + QString::number(n);
                if (n > 0)
                {
                    log << "reload Google tabs or open accounts.google.com";
                    if (m_terminal)
                        m_terminal->sendText("# " + log.join("\n# ") + "\n");
                    return;
                }
                log << "Chromium cookies are often encrypted; Firefox import is more reliable";
            }
            else
            {
                log << "no Chrome/Chromium profile found";
            }
        }

        if (!source.isEmpty() && !tryFirefox && !tryChrome)
            log << "usage: :import-cookies [status|firefox|chromium|file <cookies.txt>]";
        log << "no cookies imported";
        if (m_terminal)
            m_terminal->sendText("# " + log.join("\n# ") + "\n");
    };

    m_dispatch["source"] = [this](QWebEngineView* v, const QString&)
    {
        if (!v || !m_browser)
            return;
        QPointer<BrowserTabs> guard(m_browser);
        v->page()->toHtml(
            [guard](const QString& html)
            {
                if (!guard)
                    return;
                QWebEngineView* nv = new QWebEngineView();
                nv->setHtml(
                    "<html><body "
                    "style='background:#000;color:#0f0;font-family:monospace;font-size:13px;padding:10px;white-space:"
                    "pre-wrap;'>" +
                        html.toHtmlEscaped() + "</body></html>",
                    QUrl("about:blank")
                );
                guard->tabWidget()->addTab(nv, "source");
            }
        );
    };

    m_dispatch["history"] = [this](QWebEngineView*, const QString&)
    {
        if (!m_terminal || !m_browser)
            return;
        QStringList h = m_browser->historyUrls();
        QString out;
        for (int i = 0; i < qMin(20, h.size()); i++)
            out += QString::number(i) + ": " + h[i] + "\n";
        m_terminal->sendText("# recent history:\n" + out);
    };

    m_dispatch["dark"] = m_dispatch["light"] = [this](QWebEngineView*, const QString&)
    {
        if (m_browser)
        {
            m_browser->toggleDarkMode();
            if (m_terminal)
                m_terminal->sendText(QString("# dark mode %1\n").arg(m_browser->darkModeEnabled() ? "on" : "off"));
        }
    };

    m_dispatch["adblock"] = [this](QWebEngineView*, const QString& arg)
    {
        if (arg == "download")
        {
            if (!m_terminal)
                return;
            QString dest = Achroma::configDir() + "/blocklist.txt";
            QDir().mkpath(Achroma::configDir());
            QString url = "https://easylist.to/easylist/easylist.txt";
            QProcess* p = new QProcess(this);
            p->setProcessChannelMode(QProcess::MergedChannels);
            m_terminal->sendText("# downloading EasyList to " + dest + " ...\n");
            connect(
                p,
                &QProcess::finished,
                this,
                [this, p, dest](int code, QProcess::ExitStatus)
                {
                    if (code == 0)
                        m_terminal->sendText("# blocklist saved — run :reload to apply\n");
                    else
                        m_terminal->sendText("# download failed: " + QString::fromUtf8(p->readAll()).trimmed() + "\n");
                    p->deleteLater();
                }
            );
            if (!QStandardPaths::findExecutable("curl").isEmpty())
                p->start("curl", {"-L", "--progress-bar", "-o", dest, url});
            else if (!QStandardPaths::findExecutable("wget").isEmpty())
                p->start("wget", {"-O", dest, url});
            else
                m_terminal->sendText("# curl or wget required — install either and retry\n");
            return;
        }
        if (arg == "status")
        {
            if (m_terminal)
                m_terminal->sendText(QString("# ad block %1\n").arg(m_adBlockEnabled ? "enabled" : "disabled"));
            return;
        }
        m_adBlockEnabled = !m_adBlockEnabled;
        if (adBlockToggleCallback)
            adBlockToggleCallback(m_adBlockEnabled);
        if (m_terminal)
            m_terminal->sendText(QString("# ad block %1\n").arg(m_adBlockEnabled ? "enabled" : "disabled"));
    };

    m_dispatch["zoom"] = [this](QWebEngineView* v, const QString& arg)
    {
        if (!v)
            return;
        QString host = v->url().host();
        if (arg.isEmpty() || arg == "reset")
        {
            v->setZoomFactor(1.0);
            if (!host.isEmpty())
            {
                QSettings s("Achroma", "Achroma");
                s.remove("zoom/" + host);
            }
            if (m_terminal)
                m_terminal->sendText("# zoom reset\n");
        }
        else
        {
            bool ok = false;
            double factor = arg.toDouble(&ok);
            if (!ok)
                return;
            if (factor > 10.0)
                factor /= 100.0;
            if (factor < 0.25)
                factor = 0.25;
            if (factor > 5.0)
                factor = 5.0;
            v->setZoomFactor(factor);
            if (!host.isEmpty())
            {
                QSettings s("Achroma", "Achroma");
                s.setValue("zoom/" + host, factor);
            }
            if (m_terminal)
                m_terminal->sendText(QString("# zoom %1%\n").arg(qRound(factor * 100)));
        }
    };

    m_dispatch["print"] = [](QWebEngineView* v, const QString&)
    {
        if (!v)
            return;
        v->page()->printToPdf(
            [](const QByteArray& data)
            {
                QString path = QFileDialog::getSaveFileName(
                    nullptr,
                    "Save PDF",
                    QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/page.pdf",
                    "PDF (*.pdf)"
                );
                if (!path.isEmpty())
                {
                    QFile f(path);
                    if (f.open(QIODevice::WriteOnly))
                        f.write(data);
                }
            }
        );
    };

    m_dispatch["reader"] = [](QWebEngineView* v, const QString&)
    {
        if (!v)
            return;
        v->page()->runJavaScript(R"(
(function(){
    if (window.__achromaReader) {
        if (document.getElementById('achroma-reader-style')) {
            document.getElementById('achroma-reader-style').remove();
            delete window.__achromaReader;
        }
        return;
    }
    window.__achromaReader = true;
    var style = document.createElement('style');
    style.id = 'achroma-reader-style';
    style.textContent = 'body { max-width:680px !important; margin:40px auto !important; padding:0 20px !important; font-size:18px !important; line-height:1.8 !important; color:#ccc !important; background:#111 !important; font-family:serif !important; }'
        + 'nav,header,footer,aside,.sidebar,#sidebar,.nav,.menu,.ads,.ad,script,style,iframe,.comments { display:none !important; }'
        + 'img,video,svg { max-width:100% !important; height:auto !important; }'
        + 'a { color:#8af !important; }'
        + 'h1,h2,h3,h4 { color:#fff !important; }'
        + 'pre,code { font-family:monospace !important; background:#222 !important; padding:2px 6px; border-radius:3px; }'
        + 'blockquote { border-left:3px solid #555; padding-left:16px; color:#aaa; }';
    document.head.appendChild(style);
})();
)");
    };

    m_dispatch["pipe"] = [this](QWebEngineView*, const QString& arg)
    {
        if (arg.isEmpty() || !m_browser)
            return;
        QStringList parts = QProcess::splitCommand(arg);
        if (parts.isEmpty())
            return;
        QString prog = parts.takeFirst();
        QProcess* p = new QProcess(this);
        p->setProcessChannelMode(QProcess::MergedChannels);
        connect(
            p,
            &QProcess::finished,
            this,
            [this, p](int, QProcess::ExitStatus)
            {
                QByteArray out = p->readAll();
                QString html =
                    "<html><body "
                    "style='background:#000;color:#0f0;font-family:monospace;font-size:13px;padding:10px;white-space:"
                    "pre-wrap;'>" +
                    QString::fromUtf8(out).toHtmlEscaped() + "</body></html>";
                QWebEngineView* nv = new QWebEngineView();
                nv->setHtml(html, QUrl("about:blank"));
                m_browser->tabWidget()->addTab(nv, "pipe");
                p->deleteLater();
            }
        );
        p->start(prog, parts);
    };

    m_builtinMeta = {
        {"open", "Navigate to URL or bookmark", "<url>", "navigation", {"o"}},
        {"tab", "Open URL in new tab", "<url>", "navigation", {"t"}},
        {"close", "Close current tab", "", "navigation", {"c"}},
        {"back", "Go back", "", "navigation", {"b"}},
        {"forward", "Go forward", "", "navigation", {"f"}},
        {"r", "Reload page", "", "navigation", {}},
        {"home", "Go to home dashboard", "", "navigation", {}},
        {"undo", "Reopen closed tab", "", "navigation", {"u"}},
        {"next", "Next tab", "", "navigation", {"n"}},
        {"prev", "Previous tab", "", "navigation", {"p"}},
        {"goto", "Switch to tab by index", "<n>", "navigation", {"g"}},
        {"duplicate", "Duplicate current tab", "", "navigation", {"dup"}},
        {"incognito", "Open incognito tab", "[url]", "navigation", {"incog"}},
        // search
        {"search", "DuckDuckGo search", "<query>", "search", {"s"}},
        {"bookmark", "Bookmark current page", "<name>", "search", {"bm"}},
        {"bookmarks", "List bookmarks in terminal", "", "search", {}},
        // tools
        {"help", "Show keyboard/command help", "", "tools", {}},
        {"fullscreen", "Toggle fullscreen", "", "tools", {}},
        {"fuzzy", "Open command/file finder", "", "tools", {"finder"}},
        {"find", "Find in page", "", "tools", {}},
        {"dark", "Toggle dark mode", "", "tools", {"light"}},
        {"adblock", "Toggle/download/status ad blocking", "[download|status]", "tools", {}},
        {"zoom", "Set page zoom", "<pct|reset>", "tools", {}},
        {"print", "Print or save as PDF", "", "tools", {}},
        {"reader", "Toggle reader mode", "", "tools", {}},
        {"notes", "Open scratch notes tab", "", "tools", {}},
        {"run", "Run extracted code snippet", "", "tools", {}},
        {"devtools", "Open developer tools", "", "tools", {"dev"}},
        {"source", "View page source", "", "tools", {}},
        {"system", "Open current URL in system browser", "", "tools", {}},
        {"headers", "Diagnose browser headers and fingerprint", "", "tools", {}},
        {"import-cookies",
         "Import Google cookies from Firefox/Chromium/cookies.txt",
         "[status|firefox|chromium|file <path>]",
         "tools",
         {"cookies"}},
        {"pipe", "Run command, show output in tab", "<cmd>", "tools", {}},
        {"history", "Show recent URLs in terminal", "", "tools", {}},
        {"session", "Save/load/list named session", "save|load|list <name>", "tools", {}},
        {"markdown", "Render markdown file in browser", "<path>", "tools", {"md"}},
        {"man", "Open man page in browser", "[section] <name>", "tools", {}},
        {"tldr", "Open tldr page", "<name>", "tools", {}},
        {"docs", "Open documentation URL", "<term>", "tools", {}},
        {"reload", "Reload config.json", "", "tools", {}},
        // github
        {"issues", "GitHub issues for current repo", "", "github", {}},
        {"prs", "GitHub pull requests", "", "github", {}},
        {"actions", "GitHub Actions", "", "github", {}},
        {"blame", "Switch blob to blame view", "", "github", {}},
        {"permalink", "Get permalink to current line", "", "github", {}},
        // web
        {"url", "Print current URL to terminal", "", "web", {}},
        {"focus", "Focus URL bar", "", "web", {}},
        {"lasturl", "Navigate to last detected URL", "", "web", {}},
        {"bmbar", "Toggle bookmark bar", "", "web", {}},
        {"pin", "Pin or unpin current tab", "", "web", {}},
        {"hint", "Show link hints overlay", "", "web", {}},
        // terminal
        {"clear", "Clear terminal", "", "terminal", {}},
        {"copy", "Copy terminal selection", "", "terminal", {}},
        {"paste", "Paste to terminal", "", "terminal", {}},
        {"searchterm", "Toggle terminal search bar", "", "terminal", {"sterm"}},
        {"profile", "Switch terminal profile", "[name]", "terminal", {}},
        {"codeblock", "Extract code block to /tmp file", "", "terminal", {}},
        {"tosterm", "Send page selection to terminal", "", "terminal", {}},
        {"install", "Find install command on page", "", "terminal", {}},
    };
}

void CommandDispatcher::loadConfig()
{
    m_customCommands.clear();
    m_searchEngines = Achroma::builtinSearchEngines;
    m_homepage = HomepageConfig();
    m_homepage.quickLinks.clear();

    m_homepage.quickLinks.append({"GitHub", "https://github.com", "<>"});
    m_homepage.quickLinks.append({"YouTube", "https://youtube.com", "▶"});
    m_homepage.quickLinks.append({"Wikipedia", "https://wikipedia.org", "W"});
    m_homepage.quickLinks.append({"DuckDuckGo", "https://duckduckgo.com", "D"});

    m_dev.runners["cpp"] = "g++ -std=c++23 {file} -o /tmp/achroma-out && /tmp/achroma-out";
    m_dev.runners["cxx"] = "g++ -std=c++23 {file} -o /tmp/achroma-out && /tmp/achroma-out";
    m_dev.runners["c"] = "gcc -std=c17 {file} -o /tmp/achroma-out && /tmp/achroma-out";
    m_dev.runners["rs"] = "rustc {file} -o /tmp/achroma-out && /tmp/achroma-out";
    m_dev.runners["py"] = "python3 {file}";
    m_dev.runners["js"] = "node {file}";
    m_dev.runners["sh"] = "bash {file}";
    m_dev.runners["go"] = "go run {file}";
    m_dev.runners["rb"] = "ruby {file}";
    m_dev.runners["lua"] = "lua {file}";
    m_dev.runners["zig"] = "zig run {file}";
    m_dev.runners["kt"] = "kotlinc {file} -include-runtime -d /tmp/achroma-out.jar && java -jar /tmp/achroma-out.jar";
    m_dev.fileHandlers.clear();
    m_dev.fileHandlers["pdf"] = "browser";
    m_dev.fileHandlers["cpp"] = "editor";
    m_dev.fileHandlers["c"] = "editor";
    m_dev.fileHandlers["h"] = "editor";
    m_dev.fileHandlers["hpp"] = "editor";
    m_dev.docsUrls["qt"] = "https://doc.qt.io/qt-6/{name}.html";
    m_dev.docsUrls["cpp"] = "https://en.cppreference.com/w/cpp/search?q={name}";

    QJsonArray defaults;
    {
        QJsonObject compilerError;
        compilerError["pattern"] = R"((?:fatal\s+)?error:\s*(.*))";
        compilerError["action"] = "search";
        compilerError["engine"] = "https://duckduckgo.com/?q={{match}}";
        defaults.append(compilerError);

        QJsonObject linkerError;
        linkerError["pattern"] = R"(undefined\s+reference\s+to\s*[`'"'']([^`'"'']+)[`'"''])";
        linkerError["action"] = "search";
        linkerError["engine"] = "https://duckduckgo.com/?q=c%20c%2B%2B%20undefined%20reference%20{{match}}";
        defaults.append(linkerError);

        QJsonObject urlTrigger;
        urlTrigger["pattern"] = R"((https?://[^\s<>"']+))";
        urlTrigger["action"] = "open";
        defaults.append(urlTrigger);

        QJsonObject fileLine;
        fileLine["pattern"] = R"(([^\s:]+):(\d+):\d*:?)";
        fileLine["action"] = "external";
        fileLine["command"] = "editor -g {{1}}:{{2}}";
        defaults.append(fileLine);
    }
    m_mergedTriggers = defaults;

    QString configPath = Achroma::configDir() + "/config.json";
    watchConfig(configPath);

    QFile f(configPath);
    if (!f.open(QIODevice::ReadOnly))
    {
        emit configChanged();
        return;
    }
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error || !doc.isObject())
    {
        emit configChanged();
        return;
    }
    QJsonObject root = doc.object();

    if (root.contains("appearance"))
    {
        QJsonObject a = root["appearance"].toObject();
        if (a.contains("bg"))
        {
            QString c = a["bg"].toString();
            if (QColor::isValidColorName(c))
                m_appearance.bg = c;
        }
        if (a.contains("fg"))
        {
            QString c = a["fg"].toString();
            if (QColor::isValidColorName(c))
                m_appearance.fg = c;
        }
        if (a.contains("font_family"))
            m_appearance.fontFamily = a["font_family"].toString();
        if (a.contains("font_size"))
        {
            int fs = a["font_size"].toInt(0);
            if (fs >= 4)
                m_appearance.fontSize = fs;
        }
        if (a.contains("dark_mode"))
            m_appearance.darkMode = a["dark_mode"].toBool();
        if (a.contains("qss_file"))
            m_appearance.qssFile = a["qss_file"].toString();
    }

    if (root.contains("homepage"))
    {
        QJsonObject hp = root["homepage"].toObject();
        if (hp.contains("source"))
            m_homepage.source = hp["source"].toString();
        if (hp.contains("quick_links") && hp["quick_links"].isArray())
        {
            m_homepage.quickLinks.clear();
            for (const QJsonValue& v : hp["quick_links"].toArray())
            {
                QJsonObject l = v.toObject();
                Achroma::QuickLink ql;
                ql.name = l["name"].toString();
                ql.url = l["url"].toString();
                ql.icon = l["icon"].toString();
                if (!ql.name.isEmpty() && !ql.url.isEmpty())
                    m_homepage.quickLinks.append(ql);
            }
        }
    }

    if (root.contains("dev"))
    {
        QJsonObject dev = root["dev"].toObject();
        if (dev.contains("runners") && dev["runners"].isObject())
        {
            QJsonObject r = dev["runners"].toObject();
            for (auto it = r.begin(); it != r.end(); ++it)
                m_dev.runners[it.key()] = it.value().toString();
        }
        if (dev.contains("man_url"))
            m_dev.manUrl = dev["man_url"].toString();
        if (dev.contains("tldr_url"))
            m_dev.tldrUrl = dev["tldr_url"].toString();
        if (dev.contains("editor"))
            m_dev.editor = dev["editor"].toString();
        if (dev.contains("file_handlers") && dev["file_handlers"].isObject())
        {
            QJsonObject fh = dev["file_handlers"].toObject();
            for (auto it = fh.begin(); it != fh.end(); ++it)
                m_dev.fileHandlers[it.key().toLower()] = it.value().toString().toLower();
        }
        if (dev.contains("docs_urls") && dev["docs_urls"].isObject())
        {
            QJsonObject d = dev["docs_urls"].toObject();
            for (auto it = d.begin(); it != d.end(); ++it)
                m_dev.docsUrls[it.key()] = it.value().toString();
        }
        if (dev.contains("search_dirs") && dev["search_dirs"].isArray())
        {
            m_dev.searchDirs.clear();
            for (const QJsonValue& v : dev["search_dirs"].toArray())
                m_dev.searchDirs.append(v.toString());
        }
    }

    if (root.contains("keys"))
    {
        QJsonObject k = root["keys"].toObject();
        if (k.contains("focus_terminal"))
            m_keyConfig.focusTerminal = k["focus_terminal"].toString();
        if (k.contains("focus_urlbar"))
            m_keyConfig.focusUrlbar = k["focus_urlbar"].toString();
        if (k.contains("toggle_split"))
            m_keyConfig.toggleSplit = k["toggle_split"].toString();
        if (k.contains("new_tab_from_urlbar"))
            m_keyConfig.newTabFromUrlbar = k["new_tab_from_urlbar"].toString();
        if (k.contains("copy_selection"))
            m_keyConfig.copySelection = k["copy_selection"].toString();
        if (k.contains("link_hints"))
            m_keyConfig.linkHints = k["link_hints"].toString();
        if (k.contains("show_help"))
            m_keyConfig.showHelp = k["show_help"].toString();
        if (k.contains("close_help"))
            m_keyConfig.closeHelp = k["close_help"].toString();
        if (k.contains("reopen_closed_tab"))
            m_keyConfig.reopenClosedTab = k["reopen_closed_tab"].toString();
        if (k.contains("next_tab"))
            m_keyConfig.nextTab = k["next_tab"].toString();
        if (k.contains("prev_tab"))
            m_keyConfig.prevTab = k["prev_tab"].toString();
        if (k.contains("find_in_page"))
            m_keyConfig.findInPage = k["find_in_page"].toString();
        if (k.contains("toggle_terminal"))
            m_keyConfig.toggleTerminal = k["toggle_terminal"].toString();
        if (k.contains("terminal_copy"))
            m_keyConfig.terminalCopy = k["terminal_copy"].toString();
        if (k.contains("terminal_paste"))
            m_keyConfig.terminalPaste = k["terminal_paste"].toString();
        if (k.contains("terminal_clear"))
            m_keyConfig.terminalClear = k["terminal_clear"].toString();
        if (k.contains("terminal_search"))
            m_keyConfig.terminalSearch = k["terminal_search"].toString();
        if (k.contains("terminal_zoom_in"))
            m_keyConfig.terminalZoomIn = k["terminal_zoom_in"].toString();
        if (k.contains("terminal_zoom_out"))
            m_keyConfig.terminalZoomOut = k["terminal_zoom_out"].toString();
        if (k.contains("terminal_zoom_reset"))
            m_keyConfig.terminalZoomReset = k["terminal_zoom_reset"].toString();
        if (k.contains("resize_up"))
            m_keyConfig.resizeUp = k["resize_up"].toString();
        if (k.contains("resize_down"))
            m_keyConfig.resizeDown = k["resize_down"].toString();
        if (k.contains("resize_left"))
            m_keyConfig.resizeLeft = k["resize_left"].toString();
        if (k.contains("resize_right"))
            m_keyConfig.resizeRight = k["resize_right"].toString();
        if (k.contains("fullscreen"))
            m_keyConfig.fullscreen = k["fullscreen"].toString();
        if (k.contains("dev_tools"))
            m_keyConfig.devTools = k["dev_tools"].toString();
        if (k.contains("toggle_dark_mode"))
            m_keyConfig.toggleDarkMode = k["toggle_dark_mode"].toString();
        if (k.contains("toggle_adblock"))
            m_keyConfig.toggleAdBlock = k["toggle_adblock"].toString();
        if (k.contains("view_source"))
            m_keyConfig.viewSource = k["view_source"].toString();
        if (k.contains("print_page"))
            m_keyConfig.printPage = k["print_page"].toString();
        if (k.contains("toggle_reader"))
            m_keyConfig.toggleReader = k["toggle_reader"].toString();
        if (k.contains("toggle_bmbar"))
            m_keyConfig.toggleBmbar = k["toggle_bmbar"].toString();
        if (k.contains("send_to_term"))
            m_keyConfig.sendToTerm = k["send_to_term"].toString();
        if (k.contains("code_block"))
            m_keyConfig.codeBlock = k["code_block"].toString();
        if (k.contains("install_cmd"))
            m_keyConfig.installCmd = k["install_cmd"].toString();
        if (k.contains("fuzzy_finder"))
            m_keyConfig.fuzzyFinder = k["fuzzy_finder"].toString();
    }

    if (root.contains("engines"))
    {
        QJsonObject engines = root["engines"].toObject();
        for (auto it = engines.begin(); it != engines.end(); ++it)
            m_searchEngines[it.key()] = it.value().toString();
    }

    if (root.contains("commands"))
    {
        QJsonObject cmds = root["commands"].toObject();
        for (auto it = cmds.begin(); it != cmds.end(); ++it)
        {
            if (it.value().isObject())
                m_customCommands[it.key()] = it.value().toObject();
        }
    }

    if (root.contains("triggers"))
    {
        QJsonArray fileTriggers = root["triggers"].toArray();
        for (const QJsonValue& v : fileTriggers)
        {
            if (v.isObject())
                m_mergedTriggers.append(v);
        }
    }

    emit configChanged();
}

void CommandDispatcher::watchConfig(const QString& path)
{
    QString dir = QFileInfo(path).absolutePath();
    if (!m_watcher)
    {
        m_watcher = new QFileSystemWatcher(this);

        connect(
            m_watcher,
            &QFileSystemWatcher::fileChanged,
            this,
            [this, path](const QString&)
            {
                QTimer::singleShot(
                    80,
                    this,
                    [this, path]()
                    {
                        if (QFileInfo::exists(path) && !m_watcher->files().contains(path))
                            m_watcher->addPath(path);
                        loadConfig();
                    }
                );
            }
        );
        connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString&) { loadConfig(); });
    }
    if (QFileInfo::exists(path) && !m_watcher->files().contains(path))
        m_watcher->addPath(path);
    if (QFileInfo::exists(dir) && !m_watcher->directories().contains(dir))
        m_watcher->addPath(dir);
}
