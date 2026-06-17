#include "net/headerdump.h"
#include <QDateTime>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

HeaderDump::HeaderDump(QObject* parent) : QObject(parent)
{
}

HeaderDump::~HeaderDump()
{
    if (m_client)
    {
        m_client->close();
        m_client->deleteLater();
    }
    if (m_server)
    {
        m_server->close();
        m_server->deleteLater();
    }
}

void HeaderDump::start()
{
    m_server = new QTcpServer(this);
    m_server->listen(QHostAddress::LocalHost, 0);
    m_port = m_server->serverPort();
    connect(m_server, &QTcpServer::newConnection, this, &HeaderDump::onConnection);
}

void HeaderDump::onConnection()
{
    if (m_client)
        return;
    m_client = m_server->nextPendingConnection();
    if (!m_client)
        return;
    connect(
        m_client,
        &QTcpSocket::readyRead,
        this,
        [this]()
        {
            const QByteArray raw = m_client->readAll();
            if (raw.isEmpty())
                return;

            const QString headersHtml = capturedHeadersAsHtml(QString::fromUtf8(raw));
            const QString html = diagnosticPage(headersHtml);
            const QByteArray response = QByteArrayLiteral(
                                            "HTTP/1.1 200 OK\r\n"
                                            "Content-Type: text/html; charset=utf-8\r\n"
                                            "Connection: close\r\n"
                                            "Content-Length: "
                                        ) +
                                        QByteArray::number(html.toUtf8().size()) + "\r\n\r\n" + html.toUtf8();

            m_client->write(response);
            m_client->waitForBytesWritten(3000);
            m_client->close();
            m_server->close();
            this->deleteLater();
        }
    );
}

QString HeaderDump::capturedHeadersAsHtml(const QString& rawRequest) const
{
    const QStringList lines = rawRequest.split("\r\n");
    QString html =
        "<table style='width:100%;border-collapse:collapse;font-size:12px'>"
        "<tr style='background:#111'><th style='text-align:left;padding:4px 8px;border-bottom:1px solid "
        "#333'>Header</th>"
        "<th style='text-align:left;padding:4px 8px;border-bottom:1px solid #333'>Value</th></tr>";

    for (const QString& line : lines)
    {
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        const QString name = line.left(colon).trimmed().toHtmlEscaped();
        const QString value = line.mid(colon + 1).trimmed().toHtmlEscaped();
        html += "<tr><td style='padding:2px 8px;color:#8fb6ff;white-space:nowrap;vertical-align:top'>" + name +
                "</td><td style='padding:2px 8px;color:#ccc;word-break:break-all'>" + value + "</td></tr>";
    }

    html += "</table>";
    return html;
}

QString HeaderDump::diagnosticPage(const QString& headersHtml) const
{
    const QString ts = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    return QStringLiteral(R"HTML(
<!DOCTYPE html>
<html>
<head><meta charset="utf-8"><title>achroma headers diagnostic</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#080808;color:#ccc;font-family:monospace;padding:24px}
h2{color:#8fb6ff;font-size:16px;margin:20px 0 10px;padding-bottom:4px;border-bottom:1px solid #1e1e1e}
h3{color:#666;font-size:11px;text-transform:uppercase;letter-spacing:2px;margin:16px 0 6px}
pre{background:#0d0d0d;padding:8px 12px;border-radius:4px;overflow-x:auto;font-size:11px;line-height:1.6;border:1px solid #1a1a1a}
.good{color:#6f6} .warn{color:#f90} .bad{color:#f44} .muted{color:#555}
table{width:100%;border-collapse:collapse}
td,th{padding:2px 8px;text-align:left;vertical-align:top}
th{background:#111;border-bottom:1px solid #333}
tr:nth-child(even) td{background:#0c0c0c}
</style></head>
<body>
<h2>HTTP Request Headers</h2>
<p class="muted">Captured at %1 UTC</p>
%2

<h2>JavaScript Environment</h2>
<h3>Your Browser</h3>
<pre id="js-dump"></pre>

<h3>Expected Values (Real Chrome)</h3>
<pre id="expected" class="muted">webdriver:        undefined
plugins.length:   1
mimeTypes.length: 1
vendor:           Google Inc.
productSub:       20030107
chrome.runtime:   object
chrome.webstore:  object
userAgentData:    brands includes "Google Chrome"
screen.colorDepth: 24
language:         en-US</pre>
<script>
(function(){
var out=[];
var chk=function(prop,val,expect){
    var ok=false,bad=false;
    if(expect!==undefined){
        if(typeof val===typeof expect){
            if(typeof val==='boolean'){ok=val===expect;bad=val!==expect;}
            else if(typeof val==='string'){ok=val===expect;bad=val!==expect;}
            else if(typeof val==='number'){ok=val===expect;bad=val!==expect;}
            else if(typeof val==='object'&&val!==null){
                try{ok=JSON.stringify(val)===JSON.stringify(expect);bad=!ok;}
                catch(e){}
            }
        }
    }
    var cls=ok?'good':bad?'bad':'';
    var valStr;
    if(val===undefined)valStr='<span class=bad>undefined</span>';
    else if(val===null)valStr='<span class=warn>null</span>';
    else if(typeof val==='string')valStr=val.replace(/&/g,'&amp;').replace(/</g,'&lt;');
    else if(typeof val==='boolean'){var c=val?'good':'bad';valStr='<span class='+c+'>'+val+'</span>';}
    else if(typeof val==='number')valStr=String(val);
    else if(typeof val==='function')valStr='<span class=muted>function</span>';
    else if(typeof val==='object'){
        try{valStr=JSON.stringify(val,null,2).replace(/&/g,'&amp;').replace(/</g,'&lt;');}
        catch(e){valStr=String(val);}
    }
    var tag=cls?'<span class='+cls+'> ['+(ok?'OK':bad?'MISMATCH':'?')+']</span>':'';
    out.push(prop+': '+valStr+' '+tag);
};

chk('webdriver', navigator.webdriver, undefined);
chk('plugins.length', navigator.plugins.length, 1);
chk('vendor', navigator.vendor, 'Google Inc.');
chk('productSub', navigator.productSub, '20030107');
chk('hardwareConcurrency', navigator.hardwareConcurrency);
chk('deviceMemory', navigator.deviceMemory);
chk('screen.colorDepth', screen.colorDepth, 24);

var hasChrome=typeof window.chrome==='object' && window.chrome!==null;
chk('window.chrome', hasChrome?'object':typeof window.chrome, 'object');
chk('chrome.runtime', hasChrome?typeof window.chrome.runtime:'n/a', 'object');
chk('chrome.webstore', hasChrome?typeof window.chrome.webstore:'n/a', 'object');
chk('external', typeof window.external, 'object');

var uaHasGoogle=false;
if(navigator.userAgentData && Array.isArray(navigator.userAgentData.brands)){
    uaHasGoogle=navigator.userAgentData.brands.some(function(b){return b.brand==='Google Chrome';});
}
chk('UA brands has Google Chrome', uaHasGoogle, true);

out.push('');
out.push('--- USER-AGENT ---');
out.push(navigator.userAgent);

document.getElementById('js-dump').innerHTML=out.join('\n');
})();
</script>
</body></html>
)HTML")
        .arg(ts, headersHtml);
}
