#pragma once
#include <QWebEngineProfile>
#include <QtGlobal>

inline const char* cookiePolicyLabel(QWebEngineProfile::PersistentCookiesPolicy p)
{
    switch (p)
    {
        case QWebEngineProfile::NoPersistentCookies:
            return "none";
        case QWebEngineProfile::AllowPersistentCookies:
            return "persistent only";
        case QWebEngineProfile::ForcePersistentCookies:
            return "force session + persistent";
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        case QWebEngineProfile::OnlyPersistentCookies:
            return "only persistent";
#endif
    }
    return "unknown";
}
