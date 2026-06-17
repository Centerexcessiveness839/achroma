#include "ipc.h"
#include "commands.h"
#include "term/terminal.h"
#include "ui/browser.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLineEdit>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPixmap>
#include <QPointer>
#include <QStandardPaths>
#include <QTabWidget>
#include <QWebEnginePage>
#include <QWebEngineView>
#include <sys/socket.h>
#include <unistd.h>

IPCServer::IPCServer(QObject* parent) : QObject(parent)
{
}

bool IPCServer::start(const QString& sockPath)
{
    m_sockPath = sockPath;
    QFile::remove(m_sockPath);
    m_server = new QLocalServer(this);
    if (!m_server->listen(m_sockPath))
        return false;
    QFile::setPermissions(m_sockPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    connect(m_server, &QLocalServer::newConnection, this, &IPCServer::handleConnection);
    return true;
}

void IPCServer::handleConnection()
{
    QLocalSocket* client = m_server->nextPendingConnection();

    struct ucred cred;
    socklen_t len = sizeof(cred);
    if (getsockopt(client->socketDescriptor(), SOL_SOCKET, SO_PEERCRED, &cred, &len) == 0)
    {
        if (cred.uid != getuid())
        {
            QJsonObject resp;
            resp["ok"] = false;
            resp["error"] = "permission denied";
            client->write(QJsonDocument(resp).toJson(QJsonDocument::Compact) + "\n");
            client->disconnectFromServer();
            client->deleteLater();
            return;
        }
    }

    connect(
        client,
        &QLocalSocket::readyRead,
        this,
        [this, client]()
        {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(client->readAll(), &err);
            if (err.error)
            {
                QJsonObject resp;
                resp["ok"] = false;
                resp["error"] = err.errorString();
                client->write(QJsonDocument(resp).toJson(QJsonDocument::Compact) + "\n");
                client->disconnectFromServer();
                return;
            }
            QJsonObject req = doc.object();
            QString cmd = req["cmd"].toString();
            QString arg = req["arg"].toString();

            QPointer<QLocalSocket> guard(client);
            auto respond = [guard](bool ok, QString data = QString())
            {
                if (!guard)
                    return;
                QJsonObject r;
                r["ok"] = ok;
                if (!data.isEmpty())
                    r["data"] = data;
                guard->write(QJsonDocument(r).toJson(QJsonDocument::Compact) + "\n");
                guard->disconnectFromServer();
            };

            if (cmd == "url")
            {
                QWebEngineView* v = m_browser ? m_browser->currentView() : nullptr;
                respond(true, v ? v->url().toString() : "");
            }
            else if (cmd == "tabs")
            {
                if (!m_browser)
                {
                    respond(false, "no browser");
                    return;
                }
                QStringList t;
                for (int i = 0; i < m_browser->tabCount(); i++)
                    t += QString::number(i) + ": " + m_browser->tabText(i);
                respond(true, t.join("\n"));
            }
            else if (cmd == "focus")
            {
                if (m_browser)
                {
                    m_browser->urlBar()->setFocus();
                    m_browser->urlBar()->selectAll();
                }
                respond(true);
            }
            else if (cmd == "execute")
            {
                if (arg.isEmpty())
                {
                    respond(false, "missing arg");
                    return;
                }
                if (m_dispatcher)
                    m_dispatcher->execute(arg);
                respond(true);
            }
            else if (cmd == "open")
            {
                if (arg.isEmpty())
                {
                    respond(false, "missing arg");
                    return;
                }
                if (m_dispatcher)
                    m_dispatcher->execute("open " + arg);
                respond(true);
            }
            else if (cmd == "tab")
            {
                if (arg.isEmpty())
                {
                    respond(false, "missing arg");
                    return;
                }
                if (m_dispatcher)
                    m_dispatcher->execute("tab " + arg);
                respond(true);
            }
            else if (cmd == "search")
            {
                if (arg.isEmpty())
                {
                    respond(false, "missing arg");
                    return;
                }
                if (m_dispatcher)
                    m_dispatcher->execute("search " + arg);
                respond(true);
            }
            else if (cmd == "reload")
            {
                QWebEngineView* v = m_browser ? m_browser->currentView() : nullptr;
                if (v)
                    v->reload();
                respond(true);
            }
            else if (cmd == "back")
            {
                QWebEngineView* v = m_browser ? m_browser->currentView() : nullptr;
                if (v)
                    v->back();
                respond(true);
            }
            else if (cmd == "forward")
            {
                QWebEngineView* v = m_browser ? m_browser->currentView() : nullptr;
                if (v)
                    v->forward();
                respond(true);
            }
            else if (cmd == "title")
            {
                QWebEngineView* v = m_browser ? m_browser->currentView() : nullptr;
                respond(true, v ? v->title() : QString());
            }
            else if (cmd == "detected_url")
            {
                respond(true, m_browser ? m_browser->detectedUrl() : QString());
            }
            else if (cmd == "pipe")
            {
                if (arg.isEmpty() || !m_browser)
                {
                    respond(false, "no content");
                    return;
                }
                QString html =
                    "<html><body style='background:#0d0d0d;color:#c8ffa7;"
                    "font-family:monospace;font-size:13px;padding:20px;"
                    "white-space:pre-wrap;word-break:break-all;line-height:1.5;'>" +
                    arg.toHtmlEscaped() + "</body></html>";
                QWebEngineView* nv = new QWebEngineView();
                nv->setHtml(html, QUrl("about:blank"));
                int idx = m_browser->tabWidget()->addTab(nv, u8"⌘ pipe");
                m_browser->tabWidget()->setCurrentIndex(idx);
                respond(true);
            }
            else if (cmd == "js")
            {
                if (arg.isEmpty())
                {
                    respond(false, "missing js code");
                    return;
                }
                QWebEngineView* v = m_browser ? m_browser->currentView() : nullptr;
                if (!v)
                {
                    respond(false, "no active tab");
                    return;
                }

                v->page()->runJavaScript(
                    arg,
                    [respond](const QVariant& result)
                    {
                        if (!result.isValid())
                        {
                            respond(true, QString());
                            return;
                        }
                        QJsonValue jv = QJsonValue::fromVariant(result);
                        QString out;
                        if (jv.isString())
                            out = jv.toString();
                        else if (jv.isArray())
                            out = QString::fromUtf8(QJsonDocument(jv.toArray()).toJson(QJsonDocument::Compact));
                        else if (jv.isObject())
                            out = QString::fromUtf8(QJsonDocument(jv.toObject()).toJson(QJsonDocument::Compact));
                        else if (jv.isNull())
                            out = "null";
                        else
                            out = result.toString();
                        respond(true, out);
                    }
                );
                return;
            }
            else if (cmd == "screenshot")
            {
                QWebEngineView* v = m_browser ? m_browser->currentView() : nullptr;
                if (!v)
                {
                    respond(false, "no active tab");
                    return;
                }
                static int shotSeq = 0;
                QString path = !arg.isEmpty() ? arg : QDir::tempPath() + QString("/achroma-shot-%1.png").arg(++shotSeq);
                QPixmap px = v->grab();
                if (px.isNull())
                {
                    respond(false, "grab failed");
                    return;
                }
                if (px.save(path, "PNG"))
                    respond(true, path);
                else
                    respond(false, "failed to save: " + path);
            }
            else if (cmd == "import-cookies" || cmd == "cookies")
            {
                if (m_dispatcher)
                    m_dispatcher->execute("import-cookies" + (arg.isEmpty() ? QString() : " " + arg));
                respond(true);
            }
            else
            {
                respond(false, "unknown command: " + cmd);
            }
        }
    );
    connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
}
