#include "ui/browser.h"
#include "commands.h"
#include "core/utils.h"
#include "ui/permissionbar.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCompleter>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QStringListModel>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWebEngineDownloadRequest>
#include <QWebEngineFindTextResult>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QWidget>
#include <QtGlobal>

namespace
{
void applyWebViewSettings(QWebEngineSettings* settings)
{
    if (!settings)
        return;
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    settings->setAttribute(QWebEngineSettings::PdfViewerEnabled, true);
#endif
}
}  // namespace

BrowserTabs::BrowserTabs(QWidget* parent) : QObject(parent)
{
    m_container = new QWidget(parent);
    QVBoxLayout* layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_urlBar = new QLineEdit(m_container);
    m_urlBar->setStyleSheet(
        "background-color: #000000; color: #FFFFFF; font-family: monospace; font-size: 14px; border: none; "
        "border-bottom: 1px solid #FFFFFF; padding: 5px;"
    );
    m_urlBar->setPlaceholderText(" [ URL / SEARCH / COMMAND ] - Press Ctrl+Shift+L to focus ");
    connect(m_urlBar, &QLineEdit::returnPressed, this, &BrowserTabs::urlBarReturnPressed);

    m_lockLabel = new QLabel(m_container);
    m_lockLabel->setFixedWidth(16);
    m_lockLabel->setStyleSheet(
        "QLabel { color: #444; font-family: monospace; font-size: 13px; border: none; padding-left: 4px; }"
    );

    QHBoxLayout* urlRow = new QHBoxLayout();
    urlRow->setContentsMargins(0, 0, 0, 0);
    urlRow->setSpacing(0);
    urlRow->addWidget(m_lockLabel);
    urlRow->addWidget(m_urlBar);
    layout->addLayout(urlRow);

    m_completer = new QCompleter(m_urlBar);
    m_completer->setMaxVisibleItems(8);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_urlBar->setCompleter(m_completer);
    m_urlBar->installEventFilter(this);

    m_findBar = new QWidget(m_container);
    m_findBar->setStyleSheet("background-color: #000000; border-bottom: 1px solid #333333;");
    QHBoxLayout* findLayout = new QHBoxLayout(m_findBar);
    findLayout->setContentsMargins(4, 2, 4, 2);
    findLayout->setSpacing(4);

    m_findInput = new QLineEdit(m_findBar);
    m_findInput->setStyleSheet(
        "background-color: #111; color: #fff; font-family: monospace; font-size: 12px; border: 1px solid #444; "
        "padding: 2px 4px;"
    );
    m_findInput->setPlaceholderText("find in page...");
    m_findInput->setFixedWidth(200);
    findLayout->addWidget(m_findInput);

    QLabel* findCount = new QLabel(m_findBar);
    findCount->setStyleSheet("color: #666; font-family: monospace; font-size: 11px; border: none;");
    findCount->setFixedWidth(40);
    findLayout->addWidget(findCount);

    auto makeBtn = [&](const QString& text)
    {
        QPushButton* b = new QPushButton(text, m_findBar);
        b->setStyleSheet(
            "QPushButton { color: #aaa; background: #111; border: 1px solid #444; padding: 2px 6px; font-family: "
            "monospace; font-size: 11px; } QPushButton:hover { color: #fff; background: #333; }"
        );
        b->setFixedHeight(22);
        findLayout->addWidget(b);
        return b;
    };
    QPushButton* prevBtn = makeBtn("<");
    QPushButton* nextBtn = makeBtn(">");
    QPushButton* closeFindBtn = makeBtn("x");
    closeFindBtn->setStyleSheet(
        "QPushButton { color: #c00; background: #111; border: 1px solid #444; padding: 2px 6px; font-family: "
        "monospace; font-size: 11px; } QPushButton:hover { color: #f00; background: #333; }"
    );

    QPushButton* caseBtn = makeBtn("Aa");
    caseBtn->setCheckable(true);
    caseBtn->setStyleSheet(
        "QPushButton { color: #555; background: #111; border: 1px solid #444; padding: 2px 6px; font-family: "
        "monospace; font-size: 11px; font-weight: bold; } QPushButton:checked { color: #fff; border-color: #666; } "
        "QPushButton:hover { color: #aaa; }"
    );
    connect(caseBtn, &QPushButton::toggled, this, [this](bool on) { m_findCase = on; });

    QPushButton* highlightBtn = makeBtn("≡");
    highlightBtn->setCheckable(true);
    highlightBtn->setStyleSheet(
        "QPushButton { color: #555; background: #111; border: 1px solid #444; padding: 2px 6px; font-family: "
        "monospace; font-size: 13px; } QPushButton:checked { color: #ff0; border-color: #666; } QPushButton:hover { "
        "color: #aaa; }"
    );
    connect(highlightBtn, &QPushButton::toggled, this, [this](bool on) { m_findHighlight = on; });

    findLayout->addStretch();

    auto doFind = [this, findCount](bool forward)
    {
        QWebEngineView* v = currentView();
        if (!v)
            return;
        QString term = m_findInput->text();
        if (term.isEmpty())
            return;
        QWebEnginePage::FindFlags flags;
        if (!forward)
            flags |= QWebEnginePage::FindBackward;
        if (m_findCase)
            flags |= QWebEnginePage::FindCaseSensitively;
        v->findText(
            term,
            flags,
            [findCount](const QWebEngineFindTextResult& result)
            {
                int n = result.numberOfMatches();
                int a = result.activeMatch();
                findCount->setText(n > 0 ? QString::number(a) + "/" + QString::number(n) : " 0");
            }
        );
    };

    connect(m_findInput, &QLineEdit::returnPressed, this, [doFind]() { doFind(true); });
    connect(m_findInput, &QLineEdit::textChanged, this, [doFind]() { doFind(true); });
    connect(prevBtn, &QPushButton::clicked, this, [doFind]() { doFind(false); });
    connect(nextBtn, &QPushButton::clicked, this, [doFind]() { doFind(true); });
    connect(closeFindBtn, &QPushButton::clicked, this, [this]() { setFindBarVisible(false); });

    m_findBar->hide();
    layout->addWidget(m_findBar);

    m_bookmarkBar = new QWidget(m_container);
    m_bookmarkBar->setStyleSheet("background-color: #000;");
    QHBoxLayout* bmLayout = new QHBoxLayout(m_bookmarkBar);
    bmLayout->setContentsMargins(4, 1, 4, 1);
    bmLayout->setSpacing(2);
    m_bookmarkBar->setFixedHeight(24);
    layout->addWidget(m_bookmarkBar);
    refreshBookmarkBar();

    m_permissionBar = new PermissionBar(m_container);
    m_permissionBar->setStyleSheet("PermissionBar { background-color: #0a0a0a; border-bottom: 1px solid #1a1a1a; }");
    layout->addWidget(m_permissionBar);

    m_tabs = new QTabWidget(m_container);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setElideMode(Qt::ElideRight);
    m_tabs->setStyleSheet(
        "QTabBar::tab { background: #1a1a1a; color: #ccc; padding: 6px 12px; "
        "border: none; border-right: 1px solid #333; min-width: 80px; }"
        "QTabBar::tab:hover { background: #2a2a2a; color: #fff; }"
        "QTabBar::tab:selected { background: #000; color: #fff; }"
        "QTabBar::tab:selected:!enabled { background: #000; }"
        "QTabBar::close-button { width: 14px; height: 14px; background: #555; "
        "border: none; border-radius: 2px; margin: 2px; }"
        "QTabBar::close-button:hover { background: #c00; }"
        "QTabWidget::pane { border: none; }"
    );
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int idx) { closeTab(idx); });
    layout->addWidget(m_tabs);

    m_tabs->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(
        m_tabs,
        &QTabWidget::customContextMenuRequested,
        this,
        [this](const QPoint& pos)
        {
            int idx = m_tabs->tabBar()->tabAt(pos);
            if (idx < 0)
                return;
            QMenu menu;
            QAction* closeAct = menu.addAction("Close Tab");
            QAction* dupAct = menu.addAction("Duplicate Tab");
            QAction* reloadAct = menu.addAction("Reload Tab");
            QWebEngineView* tv = qobject_cast<QWebEngineView*>(m_tabs->widget(idx));
            bool muted = tv && tv->page()->isAudioMuted();
            QAction* muteAct = menu.addAction(muted ? "Unmute Tab" : "Mute Tab");
            menu.addSeparator();
            QAction* pinAct = menu.addAction(m_pinnedTabs.contains(idx) ? "Unpin Tab" : "Pin Tab");
            QAction* copyAct = menu.addAction("Copy URL");
            QAction* chosen = menu.exec(m_tabs->mapToGlobal(pos));
            if (!chosen)
                return;
            if (chosen == closeAct)
                closeTab(idx);
            else if (chosen == dupAct)
            {
                if (tv && tv->url().isValid())
                    addNewTab(tv->url());
            }
            else if (chosen == reloadAct && tv)
                tv->reload();
            else if (chosen == muteAct && tv)
                tv->page()->setAudioMuted(!muted);
            else if (chosen == copyAct && tv)
            {
                QApplication::clipboard()->setText(tv->url().toString());
            }
            else if (chosen == pinAct)
            {
                if (idx == m_homeTabIndex)
                    return;
                m_tabs->setCurrentIndex(idx);
                pinTab();
            }
        }
    );

    m_container->setAcceptDrops(true);
    m_container->installEventFilter(this);

    m_tabs->tabBar()->installEventFilter(this);

    auto* saveTimer = new QTimer(this);
    saveTimer->setInterval(30000);
    connect(saveTimer, &QTimer::timeout, this, &BrowserTabs::saveSession);
    saveTimer->start();

    connect(
        m_tabs,
        &QTabWidget::currentChanged,
        this,
        [this](int)
        {
            QWebEngineView* v = currentView();
            emit titleChanged(v ? v->title() : QString());
            emit audibleChanged(hasAudibleTab());
            updateLockIcon(v ? v->url() : QUrl());
            setUrlBarClean(v ? v->url() : QUrl());
        }
    );

    applyWebViewSettings(Achroma::mainProfile()->settings());
    restoreSession();
}

QWebEngineView* BrowserTabs::currentView() const
{
    return qobject_cast<QWebEngineView*>(m_tabs->currentWidget());
}

int BrowserTabs::tabCount() const
{
    return m_tabs->count();
}

bool BrowserTabs::hasAudibleTab() const
{
    for (int i = 0; i < m_tabs->count(); ++i)
    {
        auto* view = qobject_cast<QWebEngineView*>(m_tabs->widget(i));
        if (view && view->page() && view->page()->recentlyAudible())
            return true;
    }
    return false;
}

bool BrowserTabs::renderMarkdownFile(QWebEngineView* view, const QString& path) const
{
    if (!view)
        return false;

    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();
    if (suffix != "md" && suffix != "markdown")
        return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QString markdown = QString::fromUtf8(file.readAll());
    view->setHtml(Achroma::markdownPageHtml(info.fileName(), markdown), QUrl::fromLocalFile(info.absoluteFilePath()));
    return true;
}

bool BrowserTabs::openLocalMarkdown(QWebEngineView* view, const QUrl& url) const
{
    if (!url.isLocalFile())
        return false;
    return renderMarkdownFile(view, url.toLocalFile());
}

void BrowserTabs::switchToTab(int idx)
{
    if (idx >= 0 && idx < m_tabs->count())
        m_tabs->setCurrentIndex(idx);
}

void BrowserTabs::shiftTabIndices(int removedIdx)
{
    if (m_homeTabIndex > removedIdx)
        --m_homeTabIndex;

    QSet<int> updated;
    for (int idx : m_pinnedTabs)
    {
        if (idx == removedIdx)
            continue;
        updated.insert(idx > removedIdx ? idx - 1 : idx);
    }
    m_pinnedTabs = updated;
}

void BrowserTabs::adjustPinsForTabMove(int from, int to)
{
    if (from == to)
        return;
    QSet<int> updated;
    for (int idx : m_pinnedTabs)
    {
        if (idx == from)
        {
            updated.insert(to);
        }
        else if (from < to && idx > from && idx <= to)
        {
            updated.insert(idx - 1);
        }
        else if (from > to && idx >= to && idx < from)
        {
            updated.insert(idx + 1);
        }
        else
        {
            updated.insert(idx);
        }
    }
    m_pinnedTabs = updated;

    if (m_homeTabIndex == from)
        m_homeTabIndex = to;
    else if (from < to && m_homeTabIndex > from && m_homeTabIndex <= to)
        --m_homeTabIndex;
    else if (from > to && m_homeTabIndex >= to && m_homeTabIndex < from)
        ++m_homeTabIndex;
}

void BrowserTabs::addNewTab(const QUrl& url)
{
    if (url.isEmpty() && m_homeTabIndex >= 0 && m_homeTabIndex < m_tabs->count())
    {
        m_tabs->setCurrentIndex(m_homeTabIndex);
        return;
    }

    auto* page = new QWebEnginePage(Achroma::mainProfile(), nullptr);
    QWebEngineView* view = new QWebEngineView();
    view->setPage(page);
    applyWebViewSettings(view->settings());
    if (url.isEmpty())
    {
        loadHomePage(view);
    }
    else if (!openLocalMarkdown(view, url))
    {
        view->setUrl(url);
    }
    int idx = m_tabs->addTab(view, "Loading...");
    m_tabs->setTabToolTip(idx, url.toString());
    m_tabs->setCurrentIndex(idx);

    if (url.isEmpty())
        m_homeTabIndex = idx;

    connect(
        view,
        &QWebEngineView::urlChanged,
        this,
        [this, view](const QUrl& u)
        {
            if (u.isEmpty() || u.toString() == "achroma://home")
                return;
            int i = m_tabs->indexOf(view);
            if (i < 0)
                return;
            m_tabs->setTabToolTip(i, u.toString());
            if (view == currentView())
            {
                updateLockIcon(u);
                setUrlBarClean(u);
            }
            if (m_pinnedTabs.contains(i))
            {
                loadHomePage(view);
                addNewTab(u);
            }
        }
    );

    connect(
        view,
        &QWebEngineView::titleChanged,
        this,
        [this, view](const QString& t)
        {
            int i = m_tabs->indexOf(view);
            if (i >= 0)
            {
                if (i == m_homeTabIndex)
                    m_tabs->setTabText(i, "⌂ Home");
                else
                    m_tabs->setTabText(i, t.left(24));
            }
            if (view == currentView())
                emit titleChanged(t);
        }
    );
    connect(
        view,
        &QWebEngineView::loadStarted,
        this,
        [this, view]()
        {
            int i = m_tabs->indexOf(view);
            if (i >= 0)
                m_tabs->setTabText(i, "◌ " + view->url().host());
        }
    );
    connect(
        view,
        &QWebEngineView::iconChanged,
        this,
        [this, view](const QIcon& icon)
        {
            int i = m_tabs->indexOf(view);
            if (i >= 0)
                m_tabs->setTabIcon(i, icon);
        }
    );
    connect(
        view,
        &QWebEngineView::loadFinished,
        this,
        [this, view](bool ok)
        {
            if (!ok)
            {
                emit statusMessage("Failed to load: " + view->url().toString());
                return;
            }
            view->page()->runJavaScript(Achroma::keyScrollScript());
            QUrl u = view->url();
            if (!u.host().isEmpty())
            {
                QSettings zs("Achroma", "Achroma");
                double factor = zs.value("zoom/" + u.host(), 1.0).toDouble();
                if (qAbs(factor - 1.0) > 0.01)
                    view->setZoomFactor(factor);
            }
            if (u.isValid() && !u.isEmpty() && u.scheme().startsWith("http"))
            {
                m_history.removeAll(u.toString());
                m_history.prepend(u.toString());
                if (m_history.size() > 200)
                    m_history.resize(200);
                updateAutocomplete();

                QString host = u.host();
                QString scriptPath = Achroma::configDir() + "/scripts/" + host + ".js";
                QFile sf(scriptPath);
                if (sf.open(QIODevice::ReadOnly))
                {
                    view->page()->runJavaScript(QString::fromUtf8(sf.readAll()));
                }
            }
        }
    );
    connect(view->page(), &QWebEnginePage::linkHovered, this, &BrowserTabs::linkHovered);

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    connect(
        view->page(),
        &QWebEnginePage::featurePermissionRequested,
        this,
        [this, view](const QUrl& origin, QWebEnginePage::Feature feature)
        {
            m_permissionBar->request(
                origin,
                feature,
                [view, origin, feature](bool granted)
                {
                    view->page()->setFeaturePermission(
                        origin,
                        feature,
                        granted ? QWebEnginePage::PermissionGrantedByUser : QWebEnginePage::PermissionDeniedByUser
                    );
                }
            );
        }
    );
    QT_WARNING_POP

    connect(
        view->page(),
        &QWebEnginePage::recentlyAudibleChanged,
        this,
        [this, view](bool audible)
        {
            int i = m_tabs->indexOf(view);
            if (i < 0)
                return;
            QString title = view->title().left(24);
            m_tabs->setTabText(i, audible ? "♪ " + title : title);
            emit audibleChanged(hasAudibleTab());
        }
    );

    view->installEventFilter(this);

    view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(
        view,
        &QWidget::customContextMenuRequested,
        this,
        [view](const QPoint& pos)
        {
            QMenu* menu = view->createStandardContextMenu();
            menu->setAttribute(Qt::WA_DeleteOnClose);
            menu->addSeparator();
            QAction* inspect = menu->addAction("Inspect Element");
            connect(
                inspect,
                &QAction::triggered,
                view,
                [view]() { view->triggerPageAction(QWebEnginePage::InspectElement); }
            );
            menu->popup(view->mapToGlobal(pos));
        }
    );
}

void BrowserTabs::closeTab(int idx)
{
    if (idx == m_homeTabIndex)
        return;
    if (m_pinnedTabs.contains(idx))
        return;
    if (m_tabs->count() - static_cast<int>(m_pinnedTabs.size()) <= 1)
    {
        if (m_homeTabIndex >= 0 && m_homeTabIndex < m_tabs->count())
            m_tabs->setCurrentIndex(m_homeTabIndex);
    }
    QWebEngineView* v = qobject_cast<QWebEngineView*>(m_tabs->widget(idx));
    if (v)
    {
        m_lastClosedUrl = v->url();
        m_closedTabStack.prepend(v->url());
        if (m_closedTabStack.size() > 10)
            m_closedTabStack.resize(10);
    }
    QWidget* w = m_tabs->widget(idx);
    m_tabs->removeTab(idx);

    shiftTabIndices(idx);
    if (w)
        w->deleteLater();
}

void BrowserTabs::closeCurrentTab()
{
    int idx = m_tabs->currentIndex();
    if (idx >= 0)
        closeTab(idx);
}

void BrowserTabs::reopenLastTab()
{
    if (!m_closedTabStack.isEmpty())
        addNewTab(m_closedTabStack.takeFirst());
    else if (m_lastClosedUrl.isValid())
        addNewTab(m_lastClosedUrl);
}

void BrowserTabs::duplicateTab()
{
    QWebEngineView* v = currentView();
    if (v && v->url().isValid())
        addNewTab(v->url());
}

void BrowserTabs::addIncognitoTab(const QUrl& url)
{
    auto* otr = new QWebEngineProfile(QString::number(reinterpret_cast<quintptr>(this)), this);
    otr->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    otr->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    otr->setSpellCheckEnabled(false);

    auto* page = new QWebEnginePage(otr, this);
    QWebEngineView* view = new QWebEngineView();
    view->setPage(page);
    if (url.isEmpty())
        view->setHtml(Achroma::homePageHtml(), QUrl("achroma://home"));
    else if (!openLocalMarkdown(view, url))
        view->setUrl(url);
    int idx = m_tabs->addTab(view, "[I] Loading...");
    m_tabs->setCurrentIndex(idx);

    connect(
        view,
        &QWebEngineView::titleChanged,
        this,
        [this, view](const QString& t)
        {
            int i = m_tabs->indexOf(view);
            if (i >= 0)
                m_tabs->setTabText(i, "[I] " + t.left(22));
            if (view == currentView())
                emit titleChanged(t);
        }
    );
    connect(
        view,
        &QWebEngineView::loadFinished,
        [view](bool ok)
        {
            if (!ok)
                return;
            view->page()->runJavaScript(Achroma::keyScrollScript());
        }
    );
    connect(view->page(), &QWebEnginePage::linkHovered, this, &BrowserTabs::linkHovered);
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    connect(
        view->page(),
        &QWebEnginePage::featurePermissionRequested,
        this,
        [this, view](const QUrl& origin, QWebEnginePage::Feature feature)
        {
            m_permissionBar->request(
                origin,
                feature,
                [view, origin, feature](bool granted)
                {
                    view->page()->setFeaturePermission(
                        origin,
                        feature,
                        granted ? QWebEnginePage::PermissionGrantedByUser : QWebEnginePage::PermissionDeniedByUser
                    );
                }
            );
        }
    );
    QT_WARNING_POP
    connect(
        view->page(),
        &QWebEnginePage::recentlyAudibleChanged,
        this,
        [this, view](bool audible)
        {
            int i = m_tabs->indexOf(view);
            if (i < 0)
                return;
            QString title = view->title().left(22);
            m_tabs->setTabText(i, audible ? "♪[I]" + title : "[I] " + title);
            emit audibleChanged(hasAudibleTab());
        }
    );
}

void BrowserTabs::nextTab()
{
    if (m_tabs->count() < 2)
        return;
    int idx = (m_tabs->currentIndex() + 1) % m_tabs->count();
    m_tabs->setCurrentIndex(idx);
}

void BrowserTabs::prevTab()
{
    if (m_tabs->count() < 2)
        return;
    int idx = m_tabs->currentIndex() - 1;
    if (idx < 0)
        idx = m_tabs->count() - 1;
    m_tabs->setCurrentIndex(idx);
}

QString BrowserTabs::tabText(int idx) const
{
    if (idx < 0 || idx >= m_tabs->count())
        return QString();
    return m_tabs->tabText(idx);
}

QString BrowserTabs::urlForTab(int idx) const
{
    QWebEngineView* v = qobject_cast<QWebEngineView*>(m_tabs->widget(idx));
    return v ? v->url().toString() : QString();
}

QStringList BrowserTabs::allUrls() const
{
    QStringList urls;
    for (int i = 0; i < m_tabs->count(); i++)
        urls.append(urlForTab(i));
    return urls;
}

QStringList BrowserTabs::historyUrls() const
{
    return m_history;
}

void BrowserTabs::setUrlBarClean(const QUrl& url)
{
    if (m_urlBar->hasFocus())
        return;
    if (!url.isValid() || url.isEmpty() || url.scheme() == "achroma")
    {
        m_urlBar->clear();
        return;
    }
    if (url.scheme() == "http" || url.scheme() == "https")
    {
        QString host = url.host();
        if (host.startsWith("www."))
            host = host.mid(4);
        QString path = url.path();
        m_urlBar->setText((path.isEmpty() || path == "/") ? host : host + path.left(60));
    }
    else
    {
        m_urlBar->setText(url.toString());
    }
}

bool BrowserTabs::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_urlBar)
    {
        if (event->type() == QEvent::FocusIn)
        {
            QWebEngineView* v = currentView();
            if (v && v->url().isValid() && !v->url().isEmpty() && v->url().scheme() != "achroma")
                m_urlBar->setText(v->url().toString());
            QPointer<QLineEdit> bar = m_urlBar;
            QTimer::singleShot(
                0,
                this,
                [bar]()
                {
                    if (bar)
                        bar->selectAll();
                }
            );
        }
        else if (event->type() == QEvent::FocusOut)
        {
            setUrlBarClean(currentView() ? currentView()->url() : QUrl());
        }
        else if (event->type() == QEvent::KeyPress)
        {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Escape)
            {
                setUrlBarClean(currentView() ? currentView()->url() : QUrl());
                m_urlBar->clearFocus();
                return true;
            }
        }
    }
    if (event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        QWebEngineView* view = qobject_cast<QWebEngineView*>(obj);
        if (view)
        {
            if (me->button() == Qt::BackButton)
            {
                view->back();
                return true;
            }
            if (me->button() == Qt::ForwardButton)
            {
                view->forward();
                return true;
            }
        }
    }
    if (obj == m_container)
    {
        if (event->type() == QEvent::DragEnter)
        {
            QDragEnterEvent* de = static_cast<QDragEnterEvent*>(event);
            if (de->mimeData()->hasUrls() || de->mimeData()->hasText())
            {
                de->acceptProposedAction();
                return true;
            }
        }
        else if (event->type() == QEvent::Drop)
        {
            QDropEvent* de = static_cast<QDropEvent*>(event);
            for (const QUrl& url : de->mimeData()->urls())
            {
                addNewTab(url);
            }
            if (de->mimeData()->urls().isEmpty() && de->mimeData()->hasText())
            {
                addNewTab(QUrl(Achroma::formatUrl(de->mimeData()->text().trimmed())));
            }
            return true;
        }
    }
    if (obj == m_tabs->tabBar() && event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::MiddleButton)
        {
            int idx = m_tabs->tabBar()->tabAt(me->pos());
            if (idx >= 0)
                closeTab(idx);
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

void BrowserTabs::updateAutocomplete()
{
    QSet<QString> seen;
    QStringList items;
    for (const QString& u : m_history)
    {
        if (!seen.contains(u))
        {
            seen.insert(u);
            items.append(u);
        }
    }
    QSettings s("Achroma", "Achroma");
    s.beginGroup("bookmarks");
    for (const QString& key : s.childKeys())
    {
        QString bm = s.value(key).toString();
        if (!bm.isEmpty() && !seen.contains(bm))
        {
            seen.insert(bm);
            items.append(bm);
        }
    }
    s.endGroup();

    delete m_completer->model();
    m_completer->setModel(new QStringListModel(items, m_completer));
}

void BrowserTabs::setFindBarVisible(bool visible)
{
    m_findBar->setVisible(visible);
    if (visible)
    {
        m_findInput->setFocus();
        m_findInput->selectAll();
    }
    else
    {
        QWebEngineView* v = currentView();
        if (v)
            v->findText("");
        m_findInput->clear();
    }
    emit findBarVisibilityChanged(visible);
}

void BrowserTabs::saveSession()
{
    QSettings settings("Achroma", "Achroma");
    QStringList tabs;
    for (int i = 0; i < m_tabs->count(); i++)
    {
        if (i == m_homeTabIndex)
            continue;
        tabs.append(urlForTab(i));
    }
    settings.setValue("tabs", tabs);
    settings.setValue("history", m_history);
    QStringList pinned;
    for (int idx : m_pinnedTabs)
    {
        if (idx == m_homeTabIndex)
            continue;
        QString url = urlForTab(idx);
        if (!url.isEmpty())
            pinned.append(url);
    }
    settings.setValue("pinned", pinned);
}

void BrowserTabs::restoreSession()
{
    QSettings settings("Achroma", "Achroma");
    m_history = settings.value("history").toStringList();
    updateAutocomplete();

    QStringList savedTabs = settings.value("tabs").toStringList();
    QStringList pinnedUrls = settings.value("pinned").toStringList();

    for (const QString& url : pinnedUrls)
        addNewTab(QUrl(url));
    for (const QString& url : savedTabs)
        addNewTab(QUrl(url));

    if (savedTabs.isEmpty() && pinnedUrls.isEmpty())
        addNewTab(QUrl());

    ensureHomeTab();

    m_pinnedTabs.clear();
    m_pinnedTabs.insert(0);
    m_tabs->tabBar()->setTabButton(0, QTabBar::RightSide, nullptr);
    for (int i = 0; i < pinnedUrls.size(); i++)
    {
        int tabIdx = i + 1;
        if (tabIdx < m_tabs->count())
        {
            m_pinnedTabs.insert(tabIdx);
            m_tabs->tabBar()->setTabButton(tabIdx, QTabBar::RightSide, nullptr);
            m_tabs->setTabText(tabIdx, "◉ " + m_tabs->tabText(tabIdx).left(20));
        }
    }
}

void BrowserTabs::applyAppearance(const AppConfig& cfg)
{
    m_darkModeEnabled = cfg.darkMode;
    m_urlBar->setStyleSheet(QString(
                                "background-color: %1; color: %2; font-family: monospace; font-size: 14px; "
                                "border: none; border-bottom: 1px solid %2; padding: 5px;"
    )
                                .arg(cfg.bg, cfg.fg));
}

void BrowserTabs::refreshBookmarkBar()
{
    if (!m_bookmarkBar)
        return;
    QLayout* lay = m_bookmarkBar->layout();
    QLayoutItem* item;
    while ((item = lay->takeAt(0)) != nullptr)
    {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    QSettings s("Achroma", "Achroma");
    s.beginGroup("bookmarks");
    QStringList keys = s.childKeys();
    for (const QString& key : keys)
    {
        QString url = s.value(key).toString();
        if (url.isEmpty() || key.isEmpty())
            continue;
        QPushButton* btn = new QPushButton(key, m_bookmarkBar);
        btn->setStyleSheet(
            "QPushButton { background: #111; color: #aaa; border: 1px solid #333; "
            "padding: 1px 8px; font-family: monospace; font-size: 11px; }"
            "QPushButton:hover { background: #333; color: #fff; border-color: #555; }"
        );
        btn->setFixedHeight(20);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, url]() { navigateOrNewTab(QUrl(url)); });
        static_cast<QHBoxLayout*>(lay)->addWidget(btn);
    }
    s.endGroup();
    static_cast<QHBoxLayout*>(lay)->addStretch();
}

void BrowserTabs::toggleBookmarkBar()
{
    if (m_bookmarkBar)
        m_bookmarkBar->setVisible(!m_bookmarkBar->isVisible());
}

void BrowserTabs::pinTab()
{
    int idx = m_tabs->currentIndex();
    if (idx < 0)
        return;
    if (m_pinnedTabs.contains(idx))
    {
        m_pinnedTabs.remove(idx);
        QWebEngineView* v = qobject_cast<QWebEngineView*>(m_tabs->widget(idx));
        if (v)
            m_tabs->setTabText(idx, v->title().left(24));

        auto* closeBtn = new QToolButton(m_tabs->tabBar());
        closeBtn->setFixedSize(14, 14);
        closeBtn->setText("×");
        closeBtn->setStyleSheet(
            "QToolButton { background: #555; color: #ccc; border: none; border-radius: 2px; "
            "font-size: 11px; font-weight: bold; margin: 2px; padding: 0; }"
            "QToolButton:hover { background: #c00; color: #fff; }"
        );

        connect(
            closeBtn,
            &QToolButton::clicked,
            this,
            [this, v]()
            {
                int i = m_tabs->indexOf(v);
                if (i >= 0)
                    closeTab(i);
            }
        );
        m_tabs->tabBar()->setTabButton(idx, QTabBar::RightSide, closeBtn);
    }
    else
    {
        m_pinnedTabs.insert(idx);
        m_tabs->tabBar()->setTabButton(idx, QTabBar::RightSide, nullptr);
        QWebEngineView* v = qobject_cast<QWebEngineView*>(m_tabs->widget(idx));
        if (v)
        {
            bool isHome = (idx == m_homeTabIndex);
            m_tabs->setTabText(idx, (isHome ? "⌂ " : "◉ ") + v->title().left(20));
        }
    }
}

bool BrowserTabs::isPinned(int idx) const
{
    return m_pinnedTabs.contains(idx);
}

void BrowserTabs::navigateOrNewTab(const QUrl& url)
{
    if (m_pinnedTabs.contains(m_tabs->currentIndex()))
    {
        addNewTab(url);
        return;
    }
    if (QWebEngineView* v = currentView())
    {
        if (!openLocalMarkdown(v, url))
            v->setUrl(url);
    }
}

void BrowserTabs::loadHomePage(QWebEngineView* view)
{
    QSettings s("Achroma", "Achroma");
    s.beginGroup("homepage");
    QString source = s.value("source", "builtin").toString();
    s.endGroup();
    if (source == "duckduckgo")
    {
        view->setUrl(QUrl("https://duckduckgo.com"));
    }
    else if (source.startsWith('/') || source.startsWith('~'))
    {
        QString path = source.startsWith('~') ? QDir::homePath() + source.mid(1) : source;
        QFile f(path);
        if (f.open(QIODevice::ReadOnly))
            view->setHtml(QString::fromUtf8(f.readAll()), QUrl("achroma://home"));
        else
            view->setHtml(Achroma::homePageHtml(), QUrl("achroma://home"));
    }
    else
    {
        QMap<QString, QString> bm;
        s.beginGroup("bookmarks");
        for (const QString& key : s.childKeys())
            bm[key] = s.value(key).toString();
        s.endGroup();
        view->setHtml(Achroma::homePageHtml(bm), QUrl("achroma://home"));
    }
}

void BrowserTabs::ensureHomeTab()
{
    if (m_homeTabIndex >= 0 && m_homeTabIndex < m_tabs->count())
    {
        if (m_homeTabIndex != 0)
        {
            int from = m_homeTabIndex;
            m_tabs->tabBar()->moveTab(from, 0);
            adjustPinsForTabMove(from, 0);
        }
        m_homeTabIndex = 0;
        if (!m_pinnedTabs.contains(0))
        {
            m_pinnedTabs.insert(0);
            m_tabs->tabBar()->setTabButton(0, QTabBar::RightSide, nullptr);
        }
        QWebEngineView* v = qobject_cast<QWebEngineView*>(m_tabs->widget(0));
        if (v)
            loadHomePage(v);
        m_tabs->setTabText(0, "⌂ Home");
        m_tabs->setCurrentIndex(m_tabs->count() > 1 ? 1 : 0);
        return;
    }

    int beforeCount = m_tabs->count();
    addNewTab(QUrl());
    int newIdx = m_tabs->count() - 1;
    m_homeTabIndex = newIdx;
    m_pinnedTabs.insert(newIdx);
    m_tabs->tabBar()->setTabButton(newIdx, QTabBar::RightSide, nullptr);
    m_tabs->setTabText(newIdx, "⌂ Home");

    if (newIdx != 0)
    {
        m_tabs->tabBar()->moveTab(newIdx, 0);
        adjustPinsForTabMove(newIdx, 0);
    }
    m_homeTabIndex = 0;
    if (beforeCount > 0)
        m_tabs->setCurrentIndex(1);
}

void BrowserTabs::switchToHomeTab()
{
    if (m_homeTabIndex >= 0 && m_homeTabIndex < m_tabs->count())
    {
        m_tabs->setCurrentIndex(m_homeTabIndex);
        QWebEngineView* v = qobject_cast<QWebEngineView*>(m_tabs->widget(m_homeTabIndex));
        if (v)
            loadHomePage(v);
    }
    else
    {
        ensureHomeTab();
    }
}

void BrowserTabs::toggleDarkMode()
{
    m_darkModeEnabled = !m_darkModeEnabled;
    for (int i = 0; i < m_tabs->count(); i++)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        QWebEngineView* v = qobject_cast<QWebEngineView*>(m_tabs->widget(i));
        if (v)
            v->settings()->setAttribute(QWebEngineSettings::ForceDarkMode, m_darkModeEnabled);
#endif
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    Achroma::mainProfile()->settings()->setAttribute(QWebEngineSettings::ForceDarkMode, m_darkModeEnabled);
#endif
}

void BrowserTabs::updateLockIcon(const QUrl& url)
{
    if (url.scheme() == "https")
    {
        m_lockLabel->setText("S");
        m_lockLabel->setStyleSheet(
            "QLabel { color: #00ff00; font-family: monospace; font-size: 13px; border: none; padding-left: 4px; }"
        );
    }
    else if (url.scheme() == "http")
    {
        m_lockLabel->setText("!");
        m_lockLabel->setStyleSheet(
            "QLabel { color: #ff6600; font-family: monospace; font-size: 13px; border: none; padding-left: 4px; }"
        );
    }
    else
    {
        m_lockLabel->setText("");
    }
}
