#pragma once
#include <QElapsedTimer>
#include <QJsonArray>
#include <QList>
#include <QObject>
#include <QString>

class BrowserTabs;

class TriggerEngine : public QObject
{
    Q_OBJECT
public:
    explicit TriggerEngine(QObject* parent = nullptr);

    void setBrowser(BrowserTabs* browser)
    {
        m_browser = browser;
    }
    void setTriggers(const QJsonArray& triggers);
    void setDebounceMs(int ms)
    {
        m_debounceMs = ms;
    }

    void processTerminalOutput(const QString& text);

private:
    bool checkLine(const QString& line);
    void executeAction(const QJsonObject& action, const QRegularExpressionMatch& m);
    static QString resolveTemplate(const QString& tmpl, const QRegularExpressionMatch& m);

    BrowserTabs* m_browser = nullptr;
    QJsonArray m_triggers;
    QString m_outputBuffer;
    int m_debounceMs = 2000;
    QList<QElapsedTimer> m_lastMatch;
    QElapsedTimer m_floodTimer;
    int m_floodLines = 0;
};
