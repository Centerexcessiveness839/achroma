#pragma once
#include <QMainWindow>
#include <QMetaObject>
#include <QPoint>
#include <QString>
#include <QStringList>

class QSplitter;
class QFrame;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QTimer;
class QWebEnginePage;
class QWebEngineView;
class AdBlockInterceptor;
class BrowserTabs;
class Terminal;
class CommandDispatcher;
class TriggerEngine;
class FuzzyFinder;
class IPCServer;

class AchromaWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit AchromaWindow(QWidget* parent = nullptr);
    ~AchromaWindow() override;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onUrlBarReturnPressed();

private:
    void setupUi();
    void setupHelpOverlay();
    void setupShortcuts();
    void showHelp();
    void showLinkHints();
    void resizeTerminal(int delta);
    void setStatus(const QString& msg, int timeoutMs = 5000);
    void openDevTools();
    void attachDevToolsTo(QWebEngineView* v);
    void setTitleAudioActive(bool active);
    void tickTitleAudioIndicator();

    QWidget* m_central = nullptr;
    QWidget* m_titleBar = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_audioIndicator = nullptr;
    QLabel* m_adblockIndicator = nullptr;
    QTimer* m_audioTimer = nullptr;
    bool m_audioActive = false;
    int m_audioPhase = 0;
    QPoint m_dragPos;
    bool m_dragging = false;
    QSplitter* m_splitter = nullptr;
    QFrame* m_helpFrame = nullptr;
    FuzzyFinder* m_finder = nullptr;
    QLabel* m_statusLabel = nullptr;

    BrowserTabs* m_browser = nullptr;
    Terminal* m_terminal = nullptr;
    CommandDispatcher* m_dispatcher = nullptr;
    TriggerEngine* m_triggers = nullptr;
    IPCServer* m_ipc = nullptr;
    AdBlockInterceptor* m_adBlocker = nullptr;

    QString m_cmdBuffer;
    QWidget* m_termContainer = nullptr;
    int m_savedTermH = 0;
    QWidget* m_devToolsBar = nullptr;
    QWebEngineView* m_devToolsView = nullptr;
    QWebEnginePage* m_devToolsTarget = nullptr;
    QMetaObject::Connection m_devToolsTargetConn;
};
