#pragma once
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QUrl>

class QWebEngineView;
class QTabWidget;
class QLineEdit;
class QLabel;
class QCompleter;
class QWidget;
class PermissionBar;
struct AppConfig;

class BrowserTabs : public QObject
{
    Q_OBJECT
public:
    explicit BrowserTabs(QWidget* parent = nullptr);

    QTabWidget* tabWidget() const
    {
        return m_tabs;
    }
    QLineEdit* urlBar() const
    {
        return m_urlBar;
    }
    QWidget* container() const
    {
        return m_container;
    }

    QWebEngineView* currentView() const;
    int tabCount() const;

    void addNewTab(const QUrl& url);
    void addIncognitoTab(const QUrl& url);
    void closeTab(int idx);
    void closeCurrentTab();
    void reopenLastTab();
    void duplicateTab();
    void pinTab();
    bool isPinned(int idx) const;
    void navigateOrNewTab(const QUrl& url);
    void switchToTab(int idx);
    void nextTab();
    void prevTab();

    QString tabText(int idx) const;
    QString urlForTab(int idx) const;
    QStringList allUrls() const;
    QStringList historyUrls() const;
    void setFindBarVisible(bool visible);
    void refreshBookmarkBar();
    void toggleBookmarkBar();
    void toggleDarkMode();
    bool darkModeEnabled() const
    {
        return m_darkModeEnabled;
    }
    void ensureHomeTab();
    void switchToHomeTab();
    void loadHomePage(QWebEngineView* view);

    void saveSession();
    void restoreSession();
    void applyAppearance(const AppConfig& cfg);

    QString detectedUrl() const
    {
        return m_detectedUrl;
    }
    void setDetectedUrl(const QString& url)
    {
        m_detectedUrl = url;
    }

signals:
    void urlBarReturnPressed();
    void linkHovered(const QString& url);
    void findBarVisibilityChanged(bool visible);
    void statusMessage(const QString& msg);
    void titleChanged(const QString& title);
    void audibleChanged(bool audible);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void updateAutocomplete();
    void updateLockIcon(const QUrl& url);
    void setUrlBarClean(const QUrl& url);
    void shiftTabIndices(int removedIdx);
    void adjustPinsForTabMove(int from, int to);
    bool hasAudibleTab() const;
    bool renderMarkdownFile(QWebEngineView* view, const QString& path) const;
    bool openLocalMarkdown(QWebEngineView* view, const QUrl& url) const;

    QTabWidget* m_tabs = nullptr;
    QLineEdit* m_urlBar = nullptr;
    QLabel* m_lockLabel = nullptr;
    QWidget* m_container = nullptr;
    QCompleter* m_completer = nullptr;
    QWidget* m_findBar = nullptr;
    QLineEdit* m_findInput = nullptr;
    bool m_findCase = false;
    bool m_findHighlight = false;
    QWidget* m_bookmarkBar = nullptr;
    PermissionBar* m_permissionBar = nullptr;
    QUrl m_lastClosedUrl;
    QList<QUrl> m_closedTabStack;
    QStringList m_history;
    bool m_darkModeEnabled = false;

    QString m_detectedUrl;
    QSet<int> m_pinnedTabs;
    int m_homeTabIndex = -1;
};
