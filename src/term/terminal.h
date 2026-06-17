#pragma once
#include <QObject>
#include <QString>

class QTermWidget;
class QTimer;
struct AppConfig;

class Terminal : public QObject
{
    Q_OBJECT
public:
    explicit Terminal(QWidget* parent = nullptr);

    QTermWidget* widget() const
    {
        return m_term;
    }
    void start();
    void sendText(const QString& text);
    void setFocus();
    void applyAppearance(const AppConfig& cfg);
    void startExitWatcher();

    void copy();
    void paste();
    void pasteSelection();
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void clear();
    void scrollToEnd();
    void toggleSearchBar();
    void setProfile(const QString& name);
    static QStringList availableProfiles();
    QString shellCwd() const;

signals:
    void outputAvailable(const QString& text);
    void shellExited();

private slots:
    void onTerminalData(const QString& text);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void applyZoom();
    void handleShellExit();

    QTermWidget* m_term = nullptr;
    QTimer* m_exitWatcher = nullptr;
    int m_baseFontSize = 18;
    int m_zoomLevel = 0;
    int m_lastShellPid = -1;
    bool m_exitEmitted = false;
};
