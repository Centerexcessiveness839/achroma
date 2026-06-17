#pragma once
#include <QObject>
#include <QString>

class QTcpServer;
class QTcpSocket;

class HeaderDump : public QObject
{
    Q_OBJECT
public:
    explicit HeaderDump(QObject* parent = nullptr);
    ~HeaderDump() override;
    void start();

    quint16 port() const
    {
        return m_port;
    }

private slots:
    void onConnection();

private:
    QString capturedHeadersAsHtml(const QString& rawRequest) const;
    QString diagnosticPage(const QString& headersHtml) const;

    QTcpServer* m_server = nullptr;
    QTcpSocket* m_client = nullptr;
    quint16 m_port = 0;
};
