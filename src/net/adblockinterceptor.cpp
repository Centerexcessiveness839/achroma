#include "net/adblockinterceptor.h"
#include <QFile>
#include <QRegularExpression>
#include <QUrl>
#include <QWebEngineUrlRequestInfo>

AdBlockInterceptor::AdBlockInterceptor(QObject* parent) : QWebEngineUrlRequestInterceptor(parent)
{
    loadBuiltinList();
}

void AdBlockInterceptor::loadBuiltinList()
{
    static const char* const builtins[] = {
        // Google ads & tracking
        "doubleclick.net",
        "googlesyndication.com",
        "googleadservices.com",
        "google-analytics.com",
        "googletagmanager.com",
        "googletagservices.com",
        "googleoptimize.com",
        "googlelead.com",
        "adservice.google.com",
        "pagead2.googlesyndication.com",
        // Facebook tracking
        "connect.facebook.net",
        "facebook-hardware.com",
        "fbsbx.com",
        // Major ad networks
        "adnxs.com",
        "adnxs-simple.com",
        "moatads.com",
        "moatpixel.com",
        "advertising.com",
        "adblade.com",
        "adform.net",
        "adfox.ru",
        "adgridwork.com",
        "adhigh.net",
        "admedia.com",
        "admixer.net",
        "adroll.com",
        "adskeeper.co.uk",
        "adspeed.com",
        "adtarget.me",
        "adtelligent.com",
        "adtechus.com",
        "adthink.com",
        "adtrue.com",
        "adxbid.info",
        "openx.net",
        "openx.com",
        "pubmatic.com",
        "rubiconproject.com",
        "casalemedia.com",
        "indexww.com",
        "outbrain.com",
        "taboola.com",
        "taboolasyndication.com",
        "criteo.com",
        "criteo.net",
        "media.net",
        "amazon-adsystem.com",
        "adsystem.com",
        "ads.twitter.com",
        "ads.linkedin.com",
        "ads.yahoo.com",
        "ads.pinterest.com",
        "yieldmanager.com",
        "yieldmanager.net",
        "yieldmo.com",
        "sovrn.com",
        "lijit.com",
        "33across.com",
        "appnexus.com",
        "contextweb.com",
        "betweendigital.com",
        "bidswitch.net",
        "bidvertiser.com",
        "brightmountainmedia.com",
        "buysellads.com",
        "buysellads.net",
        "carbonads.com",
        "carbonads.net",
        "cxense.com",
        "emxdgt.com",
        "exponential.com",
        "eyeota.net",
        "flashtalking.com",
        "fout.jp",
        "go.exitjunction.com",
        "gumgum.com",
        "iponweb.net",
        "justpremium.com",
        "kargo.com",
        "liftoff.io",
        "lijit.com",
        "loopme.com",
        "louderhound.com",
        "luth.com",
        "mgid.com",
        "mopub.com",
        "nend.net",
        "netseer.com",
        "nexac.com",
        "nuggad.net",
        "oath.com",
        "onetag.com",
        "onetag.net",
        "ownlocal.com",
        "pinterest.com.adserv.net",
        "platform.twitter.com",
        "popads.net",
        "popcash.net",
        "propellerads.com",
        "realgravity.com",
        "revcontent.com",
        "rhythmone.com",
        "rlcdn.com",
        "rtbhouse.com",
        "s8t.de",
        "serving-sys.com",
        "sharethrough.com",
        "smaato.com",
        "smaato.net",
        "smartadserver.com",
        "smartclip.net",
        "socdm.com",
        "spotxchange.com",
        "spotx.tv",
        "static.ads-twitter.com",
        "stickyadstv.com",
        "synacor.com",
        "teads.tv",
        "themoneytizer.com",
        "traffiqiq.com",
        "tremorhub.com",
        "tremorvideo.com",
        "tribalfusion.com",
        "triplelift.com",
        "turn.com",
        "underdog.media",
        "undertone.com",
        "unrulymedia.com",
        "vertamedia.com",
        "vibrantmedia.com",
        "vrtcal.com",
        "webads.nl",
        "xad.com",
        "yahoo.com.adserv.net",
        "yandex.com.ads.net",
        "zemanta.com",
        // Trackers & analytics
        "scorecardresearch.com",
        "quantserve.com",
        "hotjar.com",
        "mouseflow.com",
        "fullstory.com",
        "logrocket.com",
        "heap.io",
        "mixpanel.com",
        "segment.com",
        "segment.io",
        "amplitude.com",
        "chartbeat.com",
        "chartbeat.net",
        "chartbeat.io",
        "newrelic.com",
        "nr-data.net",
        "statcounter.com",
        "woopra.com",
        "kissmetrics.com",
        "crazyegg.com",
        "clicktale.com",
        "clicktale.net",
        "optimizely.com",
        "adsafeprotected.com",
        "doubleverify.com",
        "integral-marketing.com",
        "ias.com",
        "nielsen.com",
        "comscore.com",
        "cedexis.com",
        "cedexis.net",
        "demdex.net",
        "krxd.net",
        "bluekai.com",
        "exelator.com",
        "bkrtx.com",
        "addthis.com",
        "sharethis.com",
        "zergnet.com",
        "ligatus.com",
        "nuance.com.ads.net",
    };

    for (const char* d : builtins)
        m_blockedDomains.insert(QString::fromLatin1(d));
}

void AdBlockInterceptor::interceptRequest(QWebEngineUrlRequestInfo& info)
{
    if (!m_chromeVersion.isEmpty())
    {
        const QByteArray ver = m_chromeVersion.toLatin1();
        const QByteArray brands =
            "\"Not/A)Brand\";v=\"99\", "
            "\"Google Chrome\";v=\"" +
            ver +
            "\", "
            "\"Chromium\";v=\"" +
            ver + "\"";
        info.setHttpHeader("Sec-CH-UA", brands);
        info.setHttpHeader("Sec-CH-UA-Mobile", "?0");
        info.setHttpHeader("Sec-CH-UA-Platform", "\"Linux\"");
    }

    if (!m_enabled)
        return;
    if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame)
        return;
    if (matchesDomain(info.requestUrl()))
        info.block(true);
}

bool AdBlockInterceptor::matchesDomain(const QUrl& url) const
{
    const QString host = url.host();
    for (const QString& domain : m_blockedDomains)
    {
        if (host == domain || host.endsWith("." + domain))
        {
            return true;
        }
    }
    return false;
}

void AdBlockInterceptor::loadBlocklist(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return;

    static const QRegularExpression domainOnlyRule(R"(^\|\|([a-zA-Z0-9][-a-zA-Z0-9.]+[a-zA-Z0-9])\^(?:\$[^,]*)?$)");

    int added = 0;
    while (!f.atEnd() && added < 200000)
    {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith('!') || line.startsWith('#') || line.startsWith('@'))
            continue;

        const auto m = domainOnlyRule.match(line);
        if (m.hasMatch())
        {
            m_blockedDomains.insert(m.captured(1));
            ++added;
        }
        else if (
            !line.startsWith('|') && !line.startsWith('/') && !line.contains('/') && !line.startsWith('[') &&
            line.contains('.')
        )
        {
            // bare domain line
            m_blockedDomains.insert(line);
            ++added;
        }
    }
    f.close();
}
