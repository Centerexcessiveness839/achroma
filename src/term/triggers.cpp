#include "term/triggers.h"
#include "core/utils.h"
#include "ui/browser.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>
#include <QWebEngineView>

TriggerEngine::TriggerEngine(QObject* parent) : QObject(parent)
{
}

void TriggerEngine::setTriggers(const QJsonArray& triggers)
{
    m_triggers = triggers;
    m_lastMatch.clear();
    m_lastMatch.resize(m_triggers.size());
    for (int i = 0; i < m_triggers.size(); i++)
        m_lastMatch[i].start();
}

void TriggerEngine::processTerminalOutput(const QString& text)
{
    QString clean = Achroma::stripTerminalControls(text);

    int newlines = clean.count('\n');
    if (!m_floodTimer.isValid() || m_floodTimer.elapsed() > 200)
    {
        m_floodTimer.start();
        m_floodLines = 0;
    }
    m_floodLines += newlines;
    if (m_floodTimer.elapsed() < 200 && m_floodLines > 5)
        return;

    QRegularExpression urlRe("https?://[^\\s<>\"']+");
    auto urlIt = urlRe.globalMatch(clean);
    while (urlIt.hasNext())
    {
        if (m_browser)
            m_browser->setDetectedUrl(urlIt.next().captured());
    }

    m_outputBuffer += clean;
    int newline = -1;
    while ((newline = m_outputBuffer.indexOf('\n')) >= 0)
    {
        QString line = m_outputBuffer.left(newline).trimmed();
        m_outputBuffer.remove(0, newline + 1);
        if (!line.isEmpty())
            checkLine(line);
    }

    if (m_outputBuffer.size() > 4096)
        m_outputBuffer = m_outputBuffer.right(4096);
}

bool TriggerEngine::checkLine(const QString& line)
{
    for (int i = 0; i < m_triggers.size(); i++)
    {
        QJsonObject t = m_triggers[i].toObject();
        QString pattern = t["pattern"].toString();
        QRegularExpression re(pattern);
        if (!re.isValid())
            continue;

        auto m = re.match(line);
        if (!m.hasMatch())
            continue;

        if (i < m_lastMatch.size() && m_lastMatch[i].elapsed() < m_debounceMs)
            continue;
        if (i < m_lastMatch.size())
            m_lastMatch[i].restart();

        if (t.contains("actions") && t["actions"].isArray())
        {
            for (const QJsonValue& av : t["actions"].toArray())
            {
                if (av.isObject())
                    executeAction(av.toObject(), m);
            }
            return true;
        }
        if (t.contains("action"))
        {
            executeAction(t, m);
            return true;
        }
    }
    return false;
}

void TriggerEngine::executeAction(const QJsonObject& action, const QRegularExpressionMatch& m)
{
    QString type = action["action"].toString();

    if (type == "external")
    {
        QString cmd = resolveTemplate(action["command"].toString(), m);
        if (cmd.isEmpty())
            return;
        QStringList parts = QProcess::splitCommand(cmd);
        if (parts.isEmpty())
            return;
        QString prog = parts.takeFirst();
        QProcess::startDetached(prog, parts);
        return;
    }

    QWebEngineView* wv = m_browser ? m_browser->currentView() : nullptr;

    if (type == "search")
    {
        QString engine = action["engine"].toString("https://duckduckgo.com/?q={{match}}");
        QString query = m.lastCapturedIndex() > 0 && !m.captured(1).isEmpty() ? m.captured(1) : m.captured(0);
        if (m_browser)
            m_browser->navigateOrNewTab(QUrl(Achroma::searchUrl(resolveTemplate(engine, m), query)));
    }
    else if (type == "open")
    {
        QString url = m.lastCapturedIndex() > 0 && !m.captured(1).isEmpty() ? m.captured(1) : m.captured(0);
        QUrl u(url);
        if ((u.scheme() == "http" || u.scheme() == "https") && m_browser)
            m_browser->navigateOrNewTab(u);
    }
    else if (type == "clear")
    {
    }
    else if (type == "reload")
    {
        if (wv)
            wv->reload();
    }
}

QString TriggerEngine::resolveTemplate(const QString& tmpl, const QRegularExpressionMatch& m)
{
    QString result = tmpl;
    result.replace("{{match}}", m.captured(0));
    for (int i = 1; i <= qMin(9, m.lastCapturedIndex()); i++)
        result.replace("{{" + QString::number(i) + "}}", m.captured(i));
    return result;
}
