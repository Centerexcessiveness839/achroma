#pragma once
#include <QSet>
#include <QString>
#include <QWebEngineUrlRequestInterceptor>

class AdBlockInterceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
public:
    explicit AdBlockInterceptor(QObject* parent = nullptr);
    void interceptRequest(QWebEngineUrlRequestInfo& info) override;
    void loadBlocklist(const QString& path);
    bool isEnabled() const
    {
        return m_enabled;
    }
    void setEnabled(bool on)
    {
        m_enabled = on;
    }
    void setChromeVersion(const QString& ver)
    {
        m_chromeVersion = ver;
    }
    bool matchesDomain(const QUrl& url) const;
    int domainCount() const
    {
        return m_blockedDomains.size();
    }

private:
    void loadBuiltinList();

    QSet<QString> m_blockedDomains;
    QString m_chromeVersion;
    bool m_enabled = true;
};
