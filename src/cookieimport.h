#pragma once
#include <QList>
#include <QNetworkCookie>
#include <QString>

class QWebEngineProfile;

class CookieImporter
{
public:
    static int importFromFirefox(QWebEngineProfile* profile);
    static int importFromChrome(QWebEngineProfile* profile);
    static int importFromNetscapeFile(QWebEngineProfile* profile, const QString& path);
    static QString findFirefoxCookiesPath();
    static QString findChromeCookiesPath();

private:
    static int importFromDb(QWebEngineProfile* profile, const QString& dbPath, const QString& query, bool firefoxStyle);
};
