#include "utils.h"
#include <QRegularExpression>
#include <QSettings>
#include <QTextDocument>
#include <QUrl>

namespace Achroma
{

QString configDir()
{
    return QDir::homePath() + "/.config/achroma";
}

QString dataDir()
{
    return QDir::homePath() + "/.local/share/achroma";
}

QString urlEncode(const QString& text)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(text));
}

QString searchUrl(const QString& engine, const QString& query)
{
    QString q = urlEncode(query);
    if (engine.contains("{{match}}"))
        return QString(engine).replace("{{match}}", q);
    if (engine.contains("{{arg}}"))
        return QString(engine).replace("{{arg}}", q);
    return engine + q;
}

QString duckDuckGoUrl(const QString& query)
{
    return searchUrl("https://duckduckgo.com/?q=", query);
}

QString formatUrl(const QString& input)
{
    if (input.startsWith("http://") || input.startsWith("https://") || input.startsWith("file://") ||
        input.startsWith("ftp://"))
        return input;

    if (!input.contains(' ') && input.contains('.'))
    {
        static const QRegularExpression ipRe(R"(^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}(:\d+)?(/.*)?$)");
        static const QRegularExpression tldRe(R"([a-zA-Z0-9\-]+\.[a-zA-Z]{2,13}(/|$))");
        if (ipRe.match(input).hasMatch() || tldRe.match(input).hasMatch())
            return "https://" + input;
    }
    return duckDuckGoUrl(input);
}

QString stripTerminalControls(QString text)
{
    static const QRegularExpression csiRe("\x1B\\[[0-9;?]*[ -/]*[@-~]");
    static const QRegularExpression oscRe("\x1B\\][^\x07\x1B]*(?:\x07|\x1B\\\\)");
    text.remove(csiRe);
    text.remove(oscRe);
    text.remove('\r');
    return text;
}

QString markdownPageHtml(const QString& title, const QString& markdown)
{
    QTextDocument doc;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    doc.setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);
#else
    doc.setMarkdown(markdown);
#endif

    const QString rendered = doc.toHtml();
    QString body = rendered;
    QRegularExpression bodyRe("<body[^>]*>(.*)</body>", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch bodyMatch = bodyRe.match(rendered);
    if (bodyMatch.hasMatch())
        body = bodyMatch.captured(1);

    body.replace(QRegularExpression(R"(</pre>\s*<pre[^>]*>)"), "\n");
    body.replace(QRegularExpression(R"(<(td|th)[^>]*>\s*</\1>)"), "");

    return QString(
               "<!DOCTYPE html><html><head><meta charset='utf-8'><title>%1</title>"
               "<style>"
               ":root{color-scheme:dark}"
               "*{box-sizing:border-box}"
               "html{background:#080808}"
               "body{margin:0;background:#080808;color:#d2d2d2;"
               "font-family:Inter,ui-sans-serif,system-ui,-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
               "font-size:15px;line-height:1.68}"
               "main{width:min(860px,calc(100vw - 56px));margin:36px auto 72px}"
               "article{padding-bottom:24px}"
               "h1,h2,h3,h4,h5,h6{color:#f2f2f2;line-height:1.22;margin:28px 0 10px;font-weight:700;position:relative}"
               "h1{font-size:34px;letter-spacing:0;border-bottom:1px solid #242424;padding-bottom:14px;margin-top:0}"
               "h2{font-size:24px;border-bottom:1px solid #1e1e1e;padding-bottom:8px}"
               "h3{font-size:18px}h4,h5,h6{font-size:15px;color:#e2e2e2}"
               ".heading-anchor{opacity:0;margin-left:8px;color:#444;text-decoration:none;font-size:.7em;"
               "font-weight:400;border-bottom:none!important;vertical-align:middle}"
               "h1:hover .heading-anchor,h2:hover .heading-anchor,h3:hover .heading-anchor,"
               "h4:hover .heading-anchor{opacity:1}"
               "p{margin:10px 0}"
               "a{color:#8fb6ff;text-decoration:none;border-bottom:1px solid #2b4776}"
               "a:hover{color:#bed2ff;border-bottom-color:#7ea2e8}"
               "strong{color:#eeeeee;font-weight:650}"
               "em{color:#e0e0e0}"
               "code{background:#171717;color:#f0d78c;padding:2px 5px;border:1px solid #252525;border-radius:4px;"
               "font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;font-size:.92em}"
               "pre{background:#101010;border:1px solid #242424;border-radius:7px;padding:15px "
               "16px;overflow:auto;margin:16px 0;"
               "white-space:pre-wrap;word-break:break-word}"
               "pre+pre{margin-top:-17px;border-top:none;border-radius:0 0 7px 7px}"
               "pre code{background:transparent;border:none;color:#dedede;padding:0;font-size:13px;line-height:1.55}"
               "blockquote{border-left:3px solid #4b658d;margin:18px 0;padding:2px 0 2px "
               "18px;color:#aaaaaa;background:#0d0d0d}"
               "table{border-collapse:separate;border-spacing:0;width:auto;max-width:100%%;margin:18px "
               "0;display:block;overflow:auto;"
               "border:1px solid #2b2b2b;border-radius:7px;background:#0c0c0c}"
               "th,td{border:0;border-right:1px solid #2b2b2b;border-bottom:1px solid #242424;padding:9px "
               "12px;text-align:left;vertical-align:top}"
               "th:last-child,td:last-child{border-right:0}tr:last-child td{border-bottom:0}"
               "th{background:#151515;color:#eeeeee;font-weight:700}"
               "tr:nth-child(even) td{background:#101010}"
               "ul,ol{padding-left:26px;margin:12px 0}"
               "li{margin:5px 0}"
               "hr{border:none;border-top:1px solid #242424;margin:32px 0}"
               "img{max-width:100%%;height:auto;border-radius:6px;border:1px solid #222;background:#111}"
               "#achroma-toc{display:none;position:fixed;right:20px;top:64px;width:210px;"
               "max-height:calc(100vh - 90px);overflow-y:auto;background:#0c0c0c;"
               "border:1px solid #1e1e1e;border-radius:6px;padding:10px 0;font-size:11px;z-index:100;"
               "scrollbar-width:thin;scrollbar-color:#2a2a2a transparent}"
               "#achroma-toc-title{padding:4px 14px 8px;color:#444;font-size:9px;"
               "letter-spacing:3px;text-transform:uppercase}"
               "#achroma-toc .toc-item{padding:3px 14px}"
               "#achroma-toc .toc-h2{padding-left:14px}"
               "#achroma-toc .toc-h3{padding-left:26px}"
               "#achroma-toc .toc-h4{padding-left:38px}"
               "#achroma-toc a{color:#555;text-decoration:none;border-bottom:none;display:block;"
               "white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
               "#achroma-toc a:hover{color:#aaa}"
               "@media(max-width:1140px){#achroma-toc{display:none!important}}"
               "@media(max-width:640px){main{width:calc(100vw - 28px);margin:24px auto "
               "48px}h1{font-size:28px}h2{font-size:21px}}"
               "</style></head><body>"
               "<div id='achroma-toc'><div id='achroma-toc-title'>outline</div></div>"
               "<main><article>%2</article></main>"
               "<script>"
               "(function(){"
               "var headings=Array.from(document.querySelectorAll('article h1,article h2,article h3,article h4'));"
               "var toc=document.getElementById('achroma-toc');"
               "if(!toc||headings.length<3)return;"
               "var used={};"
               "headings.forEach(function(h){"
               "var base=h.textContent.trim().toLowerCase().replace(/[^a-z0-9]+/g,'-').replace(/^-|-$/g,'');"
               "if(!base)base='h';"
               "var id=base,n=1;"
               "while(used[id]){id=base+'-'+(++n);}"
               "used[id]=true;"
               "h.id=id;"
               "var anc=document.createElement('a');"
               "anc.href='#'+id;anc.className='heading-anchor';anc.textContent='#';"
               "h.appendChild(anc);"
               "var a=document.createElement('a');"
               "a.href='#'+id;"
               "a.textContent=h.textContent.replace(/#$/,'').trim();"
               "var item=document.createElement('div');"
               "item.className='toc-item toc-'+h.tagName.toLowerCase();"
               "item.appendChild(a);"
               "toc.appendChild(item);"
               "});"
               "toc.style.display='block';"
               "})();"
               "</script>"
               "</body></html>"
    )
        .arg(title.toHtmlEscaped(), body);
}

QString homePageHtml(const QMap<QString, QString>& bookmarks, const QList<Achroma::QuickLink>& quickLinks)
{
    QString bmList;
    QMap<QString, QString> bm = bookmarks;
    if (bm.isEmpty())
    {
        QSettings s("Achroma", "Achroma");
        s.beginGroup("bookmarks");
        for (const QString& key : s.childKeys())
            bm[key] = s.value(key).toString();
        s.endGroup();
    }
    for (auto it = bm.begin(); it != bm.end(); ++it)
    {
        bmList += "<a href='" + it.value().toHtmlEscaped() + "'>" + it.key().toHtmlEscaped() + "</a>";
    }

    QString tileList;
    QList<Achroma::QuickLink> links = quickLinks;
    if (links.isEmpty())
    {
        links.append({"GitHub", "https://github.com", "<>"});
        links.append({"YouTube", "https://youtube.com", "▶"});
        links.append({"Wikipedia", "https://wikipedia.org", "W"});
        links.append({"DuckDuckGo", "https://duckduckgo.com", "D"});
    }
    for (const auto& tl : links)
    {
        tileList += "<a class='tile' href='" + tl.url.toHtmlEscaped() +
                    "'>"
                    "<div class='tile-icon'>" +
                    tl.icon.toHtmlEscaped() +
                    "</div>"
                    "<div class='tile-name'>" +
                    tl.name.toHtmlEscaped() + "</div></a>";
    }

    QString svg =
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 256 256' width='120' height='120'>"
        "<rect width='256' height='256' fill='none'/>"
        "<g transform='translate(0,15)'>"
        "<path d='M 128 50 L 60 180 L 95 180 L 128 115 L 161 180 L 196 180 Z' fill='#ffffff'/>"
        "<rect x='170' y='145' width='30' height='35' fill='#ffffff'/>"
        "</g></svg>";

    QString page = QString(
        "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Achroma</title>"
        "<style>"
        "* { box-sizing:border-box; margin:0; padding:0; }"
        "body { background:#080808; color:#fff; font-family:'Source Code Pro',monospace;"
        "overflow:hidden; }"
        "canvas { position:fixed; top:0; left:0; width:100vw; height:100vh; z-index:0; }"
        ".overlay { position:fixed; top:0; left:0; width:100vw; height:100vh; z-index:1;"
        "display:flex; align-items:center; justify-content:center; pointer-events:none; }"
        ".container { text-align:center; max-width:480px; padding:0 24px;"
        "pointer-events:auto; }"
        ".logo { margin-bottom:20px; }"
        ".title { font-size:28px; font-weight:bold; letter-spacing:8px; margin-bottom:4px; }"
        ".subtitle { font-size:12px; color:#555; margin-bottom:48px; }"
        ".search { width:100%; background:transparent; border:none;"
        "border-bottom:1px solid #333; color:#fff; font-family:'Source Code Pro',monospace;"
        "font-size:18px; padding:12px 4px; outline:none; text-align:center;"
        "transition:border-color 0.3s; }"
        ".search:focus { border-bottom-color:#666; }"
        ".search::placeholder { color:#333; }"
        ".links { margin-top:40px; }"
        ".links a { color:#555; text-decoration:none; font-size:13px;"
        "margin:0 12px; transition:color 0.25s; }"
        ".links a:hover { color:#fff; }"
        ".tiles { display:flex; flex-wrap:wrap; justify-content:center; gap:40px;"
        "margin-top:48px; margin-bottom:42px; }"
        ".tile { display:inline-flex; flex-direction:column; align-items:center;"
        "text-decoration:none; padding:0; transition:transform 0.2s; }"
        ".tile:hover { transform:translateY(-1px); }"
        ".tile-icon { font-size:22px; color:#555; margin-bottom:6px;"
        "transition:color 0.2s; }"
        ".tile:hover .tile-icon { color:#fff; }"
        ".tile-name { font-size:11px; color:#444; letter-spacing:1px;"
        "transition:color 0.2s; }"
        ".tile:hover .tile-name { color:#aaa; }"
        "</style></head><body>"
        "<canvas id='bg'></canvas>"
        "<div class='overlay'><div class='container'>"
        "<div class='logo'>{{SVG}}</div>"
        "<div class='title'>A C H R O M A</div>"
        "<div class='subtitle'>browser &middot; environment</div>"
        "<input class='search' type='text' placeholder='Search the web...' autofocus"
        " onkeydown='if(event.key==\"Enter\"){var "
        "q=this.value.trim();if(q)location.href=\"https://duckduckgo.com/?q=\"+encodeURIComponent(q)}'>"
        "<div class='tiles'>{{TILES}}</div>"
        "<div class='links'>{{LINKS}}</div></div></div>"
        "<script>"
        "(function(){"
        "var c=document.getElementById('bg'),w=c.width=window.innerWidth,h=c.height=window.innerHeight;"
        "var ctx=c.getContext('2d'),particles=[],N=100,mx=-1000,my=-1000,connect=90,attract=160;"
        "window.addEventListener('resize',function(){w=c.width=window.innerWidth;h=c.height=window.innerHeight;});"
        "document.addEventListener('mousemove',function(e){mx=e.clientX;my=e.clientY;});"
        "document.addEventListener('mouseleave',function(){mx=-1000;my=-1000;});"
        "for(var i=0;i<N;i++){"
        "particles.push({x:Math.random()*w,y:Math.random()*h,"
        "vx:(Math.random()*2-1)*1.2,vy:(Math.random()*2-1)*1.2});}"
        "function draw(){ctx.clearRect(0,0,w,h);"
        "for(var i=0;i<N;i++){"
        "var p=particles[i],dx=mx-p.x,dy=my-p.y,dist=Math.sqrt(dx*dx+dy*dy);"
        "if(dist>0&&dist<attract){var f=0.12*(1-dist/attract);p.vx+=dx/dist*f;p.vy+=dy/dist*f;}"
        "p.x+=p.vx;p.y+=p.vy;"
        "if(Math.abs(p.vx)<0.2)p.vx=(Math.random()*2-1)*0.4;"
        "if(Math.abs(p.vy)<0.2)p.vy=(Math.random()*2-1)*0.4;"
        "if(p.x<-10)p.x+=w+20;if(p.x>w+10)p.x-=w+20;if(p.y<-10)p.y+=h+20;if(p.y>h+10)p.y-=h+20;"
        "var alpha=dist>0&&dist<attract?0.10+0.12*(1-dist/attract):0.05;"
        "for(var j=i+1;j<N;j++){var q=particles[j],ddx=p.x-q.x,ddy=p.y-q.y,dd=Math.sqrt(ddx*ddx+ddy*ddy);"
        "if(dd<connect){var la=alpha*(1-dd/connect);ctx.strokeStyle='rgba(255,255,255,'+la+')';"
        "ctx.lineWidth=0.35;ctx.beginPath();ctx.moveTo(p.x,p.y);ctx.lineTo(q.x,q.y);ctx.stroke();}}"
        "ctx.beginPath();ctx.arc(p.x,p.y,1.0,0,Math.PI*2);"
        "ctx.fillStyle='rgba(255,255,255,'+(dist>0&&dist<attract?0.3:0.12)+')';ctx.fill();}"
        "requestAnimationFrame(draw);}draw();})();"
        "</script></body></html>"
    );

    page.replace("{{SVG}}", svg);
    page.replace("{{TILES}}", tileList);
    page.replace("{{LINKS}}", bmList);
    return page;
}

QString linkHintsScript()
{
    return R"(
(function(){
    if (document.getElementById('achroma-hints')) return;
    var links = Array.from(document.querySelectorAll('a[href]')).filter(function(a){
        var r = a.getBoundingClientRect();
        return r.width > 4 && r.height > 4 && r.bottom > 0 && r.top < window.innerHeight;
    });
    if (!links.length) return;
    var c = 'abcdefghijklmnopqrstuvwxyz', codes = [], buf = '', cont;
    for (var i = 0; i < c.length && codes.length < links.length; i++)
        for (var j = 0; j < c.length && codes.length < links.length; j++)
            codes.push(c[i] + c[j]);
    cont = document.createElement('div');
    cont.id = 'achroma-hints';
    cont.style.cssText = 'position:fixed;top:0;left:0;width:100%;height:100%;z-index:2147483647;pointer-events:none;';
    links.forEach(function(link, i){
        var r = link.getBoundingClientRect(), h = document.createElement('div');
        h.textContent = codes[i];
        h.style.cssText = 'position:absolute;top:' + (r.top + window.scrollY) + 'px;left:' + (r.left + window.scrollX) + 'px;background:#ffd700;color:#000;font:bold 11px monospace;padding:1px 3px;border:1px solid #c00;z-index:2147483647;pointer-events:none;';
        cont.appendChild(h);
    });
    document.body.appendChild(cont);
    var handler = function(e){
        if (e.key === 'Escape') { cont.remove(); document.removeEventListener('keydown', handler); return; }
        if (e.key.length === 1 && e.key >= 'a' && e.key <= 'z') {
            buf += e.key;
            if (buf.length === 2) {
                var idx = codes.indexOf(buf);
                if (idx >= 0) links[idx].click();
                cont.remove();
                document.removeEventListener('keydown', handler);
            }
        }
    };
    document.addEventListener('keydown', handler);
})();
)";
}

QString keyScrollScript()
{
    return R"(
(function() {
    if (window.__achromaKeys) return;
    window.__achromaKeys = true;
    var gPending = false, gTimer = 0;
    document.addEventListener('keydown', function(e) {
        if (e.ctrlKey || e.metaKey || e.altKey) return;
        var t = (e.target.tagName || '').toLowerCase();
        if (t === 'input' || t === 'textarea' || e.target.isContentEditable) return;
        var k = e.key;
        if (gPending) {
            clearTimeout(gTimer);
            gPending = false;
            if (k === 'g') { window.scrollTo(0, 0); e.preventDefault(); return; }
        }
        if (k === 'j') { window.scrollBy(0, 50); e.preventDefault(); }
        else if (k === 'k') { window.scrollBy(0, -50); e.preventDefault(); }
        else if (k === 'd') { window.scrollBy(0, window.innerHeight * 0.4); e.preventDefault(); }
        else if (k === 'u') { window.scrollBy(0, -window.innerHeight * 0.4); e.preventDefault(); }
        else if (k === 'G') { window.scrollTo(0, document.body.scrollHeight); e.preventDefault(); }
        else if (k === 'g') {
            gPending = true;
            e.preventDefault();
            gTimer = setTimeout(function() { gPending = false; }, 500);
        }
    });
})();
)";
}

QString codeBlockScript()
{
    return R"(
(function(){
    function detectLang(el) {
        // Check the element and its ancestors for language hints
        var cur = el;
        while (cur && cur !== document.body) {
            var cls = cur.className || '';
            var m = cls.match(/language-(\w[\w-]*)/i) || cls.match(/lang-(\w[\w-]*)/i);
            if (m) return m[1].toLowerCase();
            m = cls.match(/^cpp$/i) || cls.match(/^cxx$/i) || cls.match(/source-cpp/i);
            if (m) return 'cpp';
            m = cls.match(/^c$/i) || cls.match(/source-c/i);
            if (m) return 'c';
            m = cls.match(/^rust$/i); if (m) return 'rust';
            m = cls.match(/^python$|^py$|source-py/i); if (m) return 'python';
            m = cls.match(/^javascript$|^js$|source-js/i); if (m) return 'js';
            m = cls.match(/^typescript$|^ts$/i); if (m) return 'ts';
            m = cls.match(/^go$|^golang$/i); if (m) return 'go';
            m = cls.match(/^java$/i); if (m) return 'java';
            m = cls.match(/^ruby$/i); if (m) return 'ruby';
            m = cls.match(/^lua$/i); if (m) return 'lua';
            m = cls.match(/^zig$/i); if (m) return 'zig';
            m = cls.match(/^sh$|^bash$|^shell$/i); if (m) return 'sh';
            m = cls.match(/^sql$/i); if (m) return 'sql';
            m = cls.match(/^haskell$|^hs$/i); if (m) return 'haskell';
            m = cls.match(/^swift$/i); if (m) return 'swift';
            m = cls.match(/^kotlin$/i); if (m) return 'kotlin';
            if (cur.getAttribute('data-lang')) return cur.getAttribute('data-lang').toLowerCase();
            if (cur.getAttribute('data-language')) return cur.getAttribute('data-language').toLowerCase();
            cur = cur.parentElement;
        }
        return '';
    }
    function isCodeContainer(el) {
        var tag = el.tagName ? el.tagName.toLowerCase() : '';
        if (tag === 'pre' || tag === 'code' || tag === 'textarea') return true;
        if (tag === 'div') {
            var cls = el.className || '';
            if (/\b(source-cpp|source-c|source-py|source-js|source-.*|highlight|syntaxhighlighter|codeblock|codesnippet|linenums)\b/i.test(cls)) return true;
            if (cls.match(/^cpp$/i) || cls.match(/^c$/i) || cls.match(/^rust$/i) || cls.match(/^python$/i)) return true;
        }
        return false;
    }

    var text = window.getSelection().toString().trim();
    if (text) return JSON.stringify({lang:'', code:text});

    var el = window.getSelection().anchorNode || document.activeElement;
    if (!el) return JSON.stringify({lang:'', code:''});
    if (el.nodeType === 3) el = el.parentElement;

    // Climb up to find a code container
    while (el && el !== document.body) {
        if (isCodeContainer(el)) {
            return JSON.stringify({lang: detectLang(el), code: el.textContent.trim()});
        }
        el = el.parentElement;
    }

    // Fallback: search for any code container on the page
    var selectors = 'pre, code, textarea, div.source-cpp, div.source-c, div.source-py, div.source-js';
    selectors += ', div.cpp, div.c, div.rust, div.python, div.highlight, div.codeblock, div.syntaxhighlighter';
    var blocks = document.querySelectorAll(selectors);
    for (var i = 0; i < blocks.length; i++) {
        var t = blocks[i].textContent ? blocks[i].textContent.trim() : (blocks[i].value || '');
        if (t && t.length > 10)
            return JSON.stringify({lang: detectLang(blocks[i]), code: t});
    }

    // Last fallback: any block-level element with substantial text
    blocks = document.querySelectorAll('pre, code, textarea');
    for (var i = 0; i < blocks.length; i++) {
        var t = blocks[i].textContent ? blocks[i].textContent.trim() : (blocks[i].value || '');
        if (t && t.length > 10)
            return JSON.stringify({lang: detectLang(blocks[i]), code: t});
    }
    return JSON.stringify({lang:'', code:''});
})();
)";
}

QString installCmdScript()
{
    return R"(
(function(){
    var patterns = [
        /(npm\s+(install|i)\s+[\w@\/.-]+)/gi,
        /(pip\s+install\s+[\w\[\].-]+)/gi,
        /(pip3\s+install\s+[\w\[\].-]+)/gi,
        /(gem\s+install\s+[\w-]+)/gi,
        /(cargo\s+install\s+[\w-]+)/gi,
        /(go\s+get\s+[\w\/.@-]+)/gi,
        /(apt\s+install\s+[\w-]+)/gi,
        /(apt-get\s+install\s+[\w-]+)/gi,
        /(dnf\s+install\s+[\w-]+)/gi,
        /(yum\s+install\s+[\w-]+)/gi,
        /(pacman\s+-S\s+[\w-]+)/gi,
        /(brew\s+install\s+[\w-]+)/gi,
        /(git\s+clone\s+[\w:\/.@-]+)/gi,
        /(git\s+clone.*)/gi,
        /(gcc\s+.*\.c)/gi,
        /(g\+\+\s+.*\.cpp)/gi,
        /(make\s+.*)/gi,
        /(cmake\s+.*)/gi,
        /(curl\s+.*)/gi,
        /(wget\s+.*)/gi,
    ];
    var body = (document.body ? document.body.innerText : '').substring(0, 50000);
    for (var i = 0; i < patterns.length; i++) {
        var m = body.match(patterns[i]);
        if (m) return m[0].trim();
    }
    return '';
})();
)";
}
const QMap<QString, QString> builtinSearchEngines = {
    {"g", "https://google.com/search?q="},
    {"w", "https://en.wikipedia.org/wiki/Special:Search?search="},
    {"yt", "https://youtube.com/results?search_query="},
    {"gh", "https://github.com/search?q="},
    {"ddg", "https://duckduckgo.com/?q="},
};

}  // namespace Achroma
