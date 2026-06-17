#include "net/cookieimport.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkCookie>
#include <QRegularExpression>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimeZone>
#include <QUrl>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>

namespace
{

QDateTime durableImportExpiration()
{
    return QDateTime::currentDateTimeUtc().addDays(180);
}

bool isExpired(const QDateTime& expiry)
{
    return expiry.isValid() && expiry <= QDateTime::currentDateTimeUtc();
}

bool setCookieForHost(QWebEngineCookieStore* store, QNetworkCookie cookie, const QString& host)
{
    if (!store || host.isEmpty())
        return false;

    QString originHost = host;
    originHost.remove(QRegularExpression(R"(^\.+)"));
    if (originHost.isEmpty())
        return false;

    QUrl origin;
    origin.setScheme(cookie.isSecure() ? "https" : "http");
    origin.setHost(originHost);
    origin.setPath(cookie.path().isEmpty() ? "/" : cookie.path());
    store->setCookie(cookie, origin);
    return true;
}

}  // namespace

QString CookieImporter::findFirefoxCookiesPath()
{
    static const QStringList bases = {
        QDir::homePath() + "/.mozilla/firefox",
        QDir::homePath() + "/.config/mozilla/firefox",
        QDir::homePath() + "/.var/app/org.mozilla.firefox/.mozilla/firefox",
        QDir::homePath() + "/snap/firefox/common/.mozilla/firefox",
    };

    for (const QString& base : bases)
    {
        const QString profilesIni = base + "/profiles.ini";
        if (!QFile::exists(profilesIni))
            continue;

        QSettings ini(profilesIni, QSettings::IniFormat);
        QString path;
        bool isRelative = true;

        const QStringList groups = ini.childGroups();
        for (const QString& g : groups)
        {
            if (!g.startsWith("Profile"))
                continue;
            ini.beginGroup(g);
            const QString p = ini.value("Path").toString();
            const bool def = ini.value("Default", 0).toInt() == 1;
            isRelative = ini.value("IsRelative", 1).toInt() == 1;
            ini.endGroup();
            if (!p.isEmpty())
            {
                if (def)
                {
                    path = p;
                    break;
                }
                if (path.isEmpty())
                    path = p;
            }
        }

        if (path.isEmpty())
            continue;

        QString cookiesPath;
        if (isRelative)
            cookiesPath = base + "/" + path + "/cookies.sqlite";
        else
            cookiesPath = path + "/cookies.sqlite";

        if (QFile::exists(cookiesPath))
            return cookiesPath;
    }

    return {};
}

QString CookieImporter::findChromeCookiesPath()
{
    static const QStringList candidates = {
        QDir::homePath() + "/.config/google-chrome/Default/Cookies",
        QDir::homePath() + "/.config/chromium/Default/Cookies",
        QDir::homePath() + "/.config/google-chrome-beta/Default/Cookies",
        QDir::homePath() + "/.config/brave/Default/Cookies",
        QDir::homePath() + "/.var/app/com.google.Chrome/config/google-chrome/Default/Cookies",
        QDir::homePath() + "/.var/app/org.chromium.Chromium/config/chromium/Default/Cookies",
        QDir::homePath() + "/snap/chromium/common/chromium/Default/Cookies",
    };

    for (const QString& p : candidates)
    {
        if (QFile::exists(p))
            return p;
    }
    return {};
}

int CookieImporter::importFromDb(
    QWebEngineProfile* profile,
    const QString& dbPath,
    const QString& query,
    bool firefoxStyle
)
{
    if (!profile)
        return -1;

    QWebEngineCookieStore* store = profile->cookieStore();
    if (!store)
        return -1;

    QString effectivePath = dbPath;
    QString tmpPath;

    const QString connName = "achroma_cookie_import_" + QString::number(qHash(dbPath));
    int count = 0;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(effectivePath);

        if (!db.open())
        {
            // Database locked by running browser — copy to temp
            tmpPath = QDir::tempPath() + "/achroma_cookie_tmp_" + QString::number(qHash(dbPath));
            QFile::remove(tmpPath);
            if (QFile::copy(dbPath, tmpPath))
            {
                effectivePath = tmpPath;
                db.setDatabaseName(effectivePath);
                if (!db.open())
                {
                    QSqlDatabase::removeDatabase(connName);
                    QFile::remove(tmpPath);
                    return -1;
                }
            }
            else
            {
                QSqlDatabase::removeDatabase(connName);
                return -1;
            }
        }

        QSqlQuery q(db);
        if (!q.exec(query))
        {
            QSqlDatabase::removeDatabase(connName);
            if (!tmpPath.isEmpty())
                QFile::remove(tmpPath);
            return -1;
        }

        while (q.next())
        {
            const QString name = q.value(0).toString();
            const QString value = q.value(1).toString();
            const QString host = q.value(2).toString();
            const QString path = q.value(3).toString();

            if (name.isEmpty() || value.isEmpty() || host.isEmpty())
                continue;

            bool ok;
            qlonglong ts = q.value(4).toLongLong(&ok);
            QDateTime expiry;
            if (ok && ts > 0)
            {
                if (firefoxStyle)
                    expiry = QDateTime::fromSecsSinceEpoch(ts, QTimeZone::UTC);
                else
                    expiry = QDateTime::fromMSecsSinceEpoch((ts / 1000) - 11644473600000, QTimeZone::UTC);
            }

            const bool secure = q.value(5).toInt() != 0;
            const bool httpOnly = q.value(6).toInt() != 0;

            QNetworkCookie cookie(name.toUtf8(), value.toUtf8());
            cookie.setDomain(host);
            cookie.setPath(path.isEmpty() ? "/" : path);
            cookie.setSecure(secure);
            cookie.setHttpOnly(httpOnly);
            if (isExpired(expiry))
                continue;
            cookie.setExpirationDate(expiry.isValid() ? expiry : durableImportExpiration());

            if (setCookieForHost(store, cookie, host))
                count++;
        }

        db.close();
    }
    QSqlDatabase::removeDatabase(connName);
    if (!tmpPath.isEmpty())
        QFile::remove(tmpPath);
    return count;
}

int CookieImporter::importFromFirefox(QWebEngineProfile* profile)
{
    const QString dbPath = findFirefoxCookiesPath();
    if (dbPath.isEmpty())
        return -1;

    const QString query = QStringLiteral(
        "SELECT name, value, host, path, expiry, isSecure, isHttpOnly "
        "FROM moz_cookies "
        "WHERE host LIKE '%%.google.com' "
        "   OR host = 'accounts.google.com' "
        "   OR host LIKE '%%.youtube.com' "
        "   OR host LIKE '%%.gstatic.com'"
    );

    return importFromDb(profile, dbPath, query, true);
}

int CookieImporter::importFromChrome(QWebEngineProfile* profile)
{
    const QString dbPath = findChromeCookiesPath();
    if (dbPath.isEmpty())
        return -1;

    const QString query = QStringLiteral(
        "SELECT name, value, host_key, path, expires_utc, is_secure, is_httponly "
        "FROM cookies "
        "WHERE host_key LIKE '%%.google.com' "
        "   OR host_key = 'accounts.google.com' "
        "   OR host_key LIKE '%%.youtube.com' "
        "   OR host_key LIKE '%%.gstatic.com'"
    );

    return importFromDb(profile, dbPath, query, false);
}

int CookieImporter::importFromNetscapeFile(QWebEngineProfile* profile, const QString& path)
{
    if (!profile)
        return -1;

    QWebEngineCookieStore* store = profile->cookieStore();
    if (!store)
        return -1;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return -1;

    QTextStream in(&file);
    int count = 0;
    while (!in.atEnd())
    {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        bool httpOnly = false;
        if (line.startsWith("#HttpOnly_"))
        {
            httpOnly = true;
            line = line.mid(QString("#HttpOnly_").size());
        }
        else if (line.startsWith('#'))
        {
            continue;
        }

        const QStringList parts = line.split('\t');
        if (parts.size() < 7)
            continue;

        const QString domain = parts.at(0).trimmed();
        const QString cookiePath = parts.at(2).trimmed().isEmpty() ? "/" : parts.at(2).trimmed();
        const bool secure = parts.at(3).compare("TRUE", Qt::CaseInsensitive) == 0;
        bool ok = false;
        const qlonglong expirySecs = parts.at(4).toLongLong(&ok);
        const QString name = parts.at(5);
        const QString value = parts.mid(6).join("\t");

        if (domain.isEmpty() || name.isEmpty() || value.isEmpty())
            continue;

        QNetworkCookie cookie(name.toUtf8(), value.toUtf8());
        cookie.setDomain(domain);
        cookie.setPath(cookiePath);
        cookie.setSecure(secure);
        cookie.setHttpOnly(httpOnly);
        QDateTime expiry;
        if (ok && expirySecs > 0)
            expiry = QDateTime::fromSecsSinceEpoch(expirySecs, QTimeZone::UTC);
        if (isExpired(expiry))
            continue;
        cookie.setExpirationDate(expiry.isValid() ? expiry : durableImportExpiration());

        if (setCookieForHost(store, cookie, domain))
            count++;
    }

    return count;
}
