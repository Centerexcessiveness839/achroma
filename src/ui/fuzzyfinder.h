#pragma once
#include <QFrame>
#include <QMetaObject>
#include <QString>
#include <QStringList>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QTimer;
class BrowserTabs;
class CommandDispatcher;
class Terminal;

class FuzzyFinder : public QFrame
{
    Q_OBJECT
public:
    explicit FuzzyFinder(QWidget* parent = nullptr);

    void setBrowser(BrowserTabs* browser);
    void setDispatcher(CommandDispatcher* dispatcher);
    void setTerminal(Terminal* terminal);

    void open();
    void closeFinder();
    void toggle();
    void reposition();

signals:
    void itemActivated();
    void closed();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void updateFuzzyFinder(const QString& query);
    void activateFuzzyItem(QListWidgetItem* item);

private:
    static QStringList findMatchingFiles(const QString& query, int maxResults, const QStringList& roots);

    QLineEdit* m_input = nullptr;
    QListWidget* m_list = nullptr;
    QTimer* m_debounce = nullptr;
    QMetaObject::Connection m_debounceConn;
    QStringList m_fileResults;
    QString m_fileQuery;

    BrowserTabs* m_browser = nullptr;
    CommandDispatcher* m_dispatcher = nullptr;
    Terminal* m_terminal = nullptr;
};
