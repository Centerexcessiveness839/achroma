#include "ui/fuzzyfinder.h"
#include "commands.h"
#include "core/utils.h"
#include "term/terminal.h"
#include "ui/browser.h"
#include <QApplication>
#include <QColor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QShortcut>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>

namespace
{

struct FinderItem
{
    QString text;
    QString url;
    QChar kind;
    QString action;
    int score = 0;
};

int scoreFuzzyTerm(const QString& term, const QString& candidate)
{
    if (term.isEmpty() || candidate.isEmpty())
        return -1;

    const QString needle = term.toCaseFolded();
    const QString haystack = candidate.toCaseFolded();
    int needleIndex = 0;
    int firstMatch = -1;
    int lastMatch = -1;
    int gaps = 0;
    int contiguous = 0;
    int bestContiguous = 0;

    for (int i = 0; i < haystack.size() && needleIndex < needle.size(); ++i)
    {
        if (haystack.at(i) != needle.at(needleIndex))
            continue;
        if (firstMatch < 0)
            firstMatch = i;
        if (lastMatch >= 0)
        {
            if (i == lastMatch + 1)
            {
                ++contiguous;
            }
            else
            {
                gaps += i - lastMatch - 1;
                contiguous = 1;
            }
        }
        else
        {
            contiguous = 1;
        }
        bestContiguous = qMax(bestContiguous, contiguous);
        lastMatch = i;
        ++needleIndex;
    }

    if (needleIndex != needle.size())
        return -1;

    int score = 1000 - firstMatch * 8 - gaps * 3 - candidate.size();
    score += bestContiguous * 12;
    if (haystack.startsWith(needle))
        score += 250;
    if (haystack.contains(needle))
        score += 150;
    return score;
}

int scoreFuzzyQuery(const QString& query, const QString& candidate)
{
    int total = 0;
    const QStringList terms = query.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const QString& term : terms)
    {
        const int score = scoreFuzzyTerm(term, candidate);
        if (score < 0)
            return -1;
        total += score;
    }
    return total;
}

QString expandHomePath(const QString& path)
{
    if (path == "~")
        return QDir::homePath();
    if (path.startsWith("~/"))
        return QDir::homePath() + path.mid(1);
    return path;
}

bool isBrowserViewableFile(const QString& path)
{
    static const QSet<QString> exts = {"txt",  "md",   "markdown", "log",  "json", "jsonl", "xml",  "html",
                                       "htm",  "css",  "csv",      "tsv",  "rst",  "tex",   "org",  "ini",
                                       "conf", "yaml", "yml",      "toml", "sql",  "patch", "diff", "pdf",
                                       "svg",  "png",  "jpg",      "jpeg", "gif",  "webp",  "bmp",  "ico",
                                       "mp3",  "wav",  "ogg",      "flac", "mp4",  "webm",  "mov"};
    return exts.contains(QFileInfo(path).suffix().toLower());
}

bool isSourceFile(const QString& path)
{
    static const QSet<QString> exts = {"cpp",  "c",     "cxx",  "cc",    "h",     "hpp",        "hxx", "hh",   "rs",
                                       "py",   "go",    "java", "js",    "ts",    "jsx",        "tsx", "rb",   "lua",
                                       "zig",  "kt",    "kts",  "swift", "scala", "hs",         "ml",  "pl",   "php",
                                       "r",    "cmake", "make", "meson", "bazel", "dockerfile", "sh",  "bash", "zsh",
                                       "fish", "bat",   "ps1",  "vim",   "sql",   "proto"};
    QString ext = QFileInfo(path).suffix().toLower();
    if (exts.contains(ext))
        return true;
    QString name = QFileInfo(path).fileName().toLower();
    return name == "makefile" || name == "dockerfile" || name == "cmakelists.txt";
}

QString shellQuote(const QString& text)
{
    QString quoted = text;
    quoted.replace('\'', "'\\''");
    return "'" + quoted + "'";
}

}  // namespace

FuzzyFinder::FuzzyFinder(QWidget* parent) : QFrame(parent)
{
    setStyleSheet(Achroma::Theme::panelFrame());
    setFixedSize(600, 400);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    m_input = new QLineEdit(this);
    m_input->setStyleSheet(Achroma::Theme::inputField());
    m_input->setPlaceholderText("Search tabs, files, history, commands  ( : for command mode )");
    m_input->installEventFilter(this);
    layout->addWidget(m_input);

    m_list = new QListWidget(this);
    m_list->setStyleSheet(Achroma::Theme::listView());
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->installEventFilter(this);
    layout->addWidget(m_list);

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(250);

    connect(m_input, &QLineEdit::textChanged, this, [this](const QString& q) { updateFuzzyFinder(q); });
    connect(
        m_input,
        &QLineEdit::returnPressed,
        this,
        [this]() { activateFuzzyItem(m_list->currentItem() ? m_list->currentItem() : m_list->item(0)); }
    );

    auto moveSelection = [this](int delta)
    {
        const int count = m_list->count();
        if (count <= 0)
            return;
        int row = m_list->currentRow();
        if (row < 0)
            row = 0;
        row = qBound(0, row + delta, count - 1);
        m_list->setCurrentRow(row);
    };
    auto* downSc = new QShortcut(QKeySequence(Qt::Key_Down), m_input);
    downSc->setContext(Qt::WidgetShortcut);
    connect(downSc, &QShortcut::activated, this, [moveSelection]() { moveSelection(1); });
    auto* upSc = new QShortcut(QKeySequence(Qt::Key_Up), m_input);
    upSc->setContext(Qt::WidgetShortcut);
    connect(upSc, &QShortcut::activated, this, [moveSelection]() { moveSelection(-1); });

    connect(m_list, &QListWidget::itemActivated, this, &FuzzyFinder::activateFuzzyItem);

    auto* escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escShortcut, &QShortcut::activated, this, [this]() { closeFinder(); });

    hide();
}

void FuzzyFinder::setBrowser(BrowserTabs* browser)
{
    m_browser = browser;
}
void FuzzyFinder::setDispatcher(CommandDispatcher* dispatcher)
{
    m_dispatcher = dispatcher;
}
void FuzzyFinder::setTerminal(Terminal* terminal)
{
    m_terminal = terminal;
}

void FuzzyFinder::open()
{
    if (!parentWidget())
        return;
    reposition();
    m_input->clear();
    m_list->clear();
    show();
    raise();
    m_input->setFocus();
}

void FuzzyFinder::closeFinder()
{
    m_debounce->stop();
    hide();
    emit closed();
}

void FuzzyFinder::toggle()
{
    if (isVisible())
    {
        closeFinder();
        return;
    }
    open();
}

void FuzzyFinder::reposition()
{
    if (!parentWidget())
        return;
    QPoint center = parentWidget()->rect().center();
    move(center.x() - width() / 2, center.y() - height() / 2);
}

bool FuzzyFinder::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::KeyPress)
    {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape)
        {
            closeFinder();
            return true;
        }
    }
    return QFrame::eventFilter(obj, event);
}

void FuzzyFinder::activateFuzzyItem(QListWidgetItem* item)
{
    if (!item)
        return;

    const QString url = item->data(Qt::UserRole).toString();
    const QString action = item->data(Qt::UserRole + 1).toString();
    const QChar kind = item->data(Qt::UserRole + 2).toChar();
    m_debounce->stop();
    hide();

    if (kind == 'C' || kind == 'T')
    {
        if (m_dispatcher)
            m_dispatcher->execute(action);
    }
    else if (kind == 'F')
    {
        QString ext = QFileInfo(url).suffix().toLower();
        QString handler = m_dispatcher ? m_dispatcher->devConfig().fileHandlers.value(ext) : QString();
        if (handler.isEmpty())
            handler = "auto";

        const bool browserViewable = isBrowserViewableFile(url);
        const bool sourceFile = isSourceFile(url);

        if (handler == "browser" || ((handler == "auto" || handler == "system") && browserViewable))
        {
            if (m_browser)
                m_browser->addNewTab(QUrl::fromLocalFile(url));
        }
        else if (handler == "editor" || (handler == "auto" && sourceFile && m_terminal))
        {
            const QString editor = (m_dispatcher && !m_dispatcher->devConfig().editor.trimmed().isEmpty())
                                       ? m_dispatcher->devConfig().editor.trimmed()
                                       : QString("nvim");
            m_terminal->sendText(editor + " " + shellQuote(url) + "\n");
            m_terminal->setFocus();
        }
        else
        {
            QDesktopServices::openUrl(QUrl::fromLocalFile(url));
        }
    }
    else if (!url.isEmpty() && m_browser)
    {
        m_browser->navigateOrNewTab(QUrl(url));
    }

    emit itemActivated();
}

void FuzzyFinder::updateFuzzyFinder(const QString& query)
{
    const QString q = query.trimmed();
    m_list->clear();
    m_debounce->stop();

    if (q.isEmpty())
    {
        m_fileResults.clear();
        m_fileQuery.clear();
        return;
    }

    const bool cmdMode = q.startsWith(':');
    const QString cmdQuery = cmdMode ? q.mid(1) : q;

    QList<FinderItem> items;

    if (!cmdMode)
    {
        for (int i = 0; m_browser && i < m_browser->tabCount(); ++i)
        {
            const QString text = m_browser->tabText(i);
            const QString url = m_browser->urlForTab(i);
            items.append({text, url, 'T', "tab " + QString::number(i), 0});
        }

        {
            QSettings s("Achroma", "Achroma");
            s.beginGroup("bookmarks");
            for (const QString& key : s.childKeys())
                items.append({key, s.value(key).toString(), 'B', "", 0});
            s.endGroup();
        }

        if (m_browser)
        {
            QSet<QString> seen;
            for (int i = 0; i < m_browser->tabCount(); i++)
                seen.insert(m_browser->urlForTab(i));
            QSettings s("Achroma", "Achroma");
            s.beginGroup("bookmarks");
            for (const QString& key : s.childKeys())
                seen.insert(s.value(key).toString());
            s.endGroup();
            for (const QString& u : m_browser->historyUrls())
            {
                if (!seen.contains(u))
                    items.append({u, u, 'H', "", 0});
            }
        }
    }

    if (m_dispatcher)
    {
        for (const auto& meta : m_dispatcher->builtinCommands())
        {
            QString display = ":" + meta.name;
            if (!meta.argHint.isEmpty())
                display += " " + meta.argHint;
            display += "  —  " + meta.description;
            if (!meta.aliases.isEmpty())
                display += "  [" + meta.aliases.join(", ") + "]";
            items.append({display, "", 'C', meta.name, 0});
        }
        for (auto it = m_dispatcher->customCommands().constBegin(); it != m_dispatcher->customCommands().constEnd();
             ++it)
        {
            const QString action = it.value()["action"].toString("custom");
            items.append({":" + it.key() + "  —  " + action + " (custom)", "", 'C', it.key(), 0});
        }
        QSet<QString> knownEngines;
        for (const auto& meta : m_dispatcher->builtinCommands())
        {
            knownEngines.insert(meta.name);
            for (const auto& a : meta.aliases)
                knownEngines.insert(a);
        }
        for (auto it = m_dispatcher->searchEngines().constBegin(); it != m_dispatcher->searchEngines().constEnd(); ++it)
        {
            if (knownEngines.contains(it.key()) || m_dispatcher->customCommands().contains(it.key()))
                continue;
            items.append({":" + it.key() + " <query>  —  search engine", "", 'C', it.key(), 0});
        }
    }

    if (!cmdMode)
    {
        if (q.size() < 2)
        {
            m_fileResults.clear();
            m_fileQuery.clear();
        }
        else if (m_fileQuery == q)
        {
            for (const QString& path : m_fileResults)
                items.append({path, path, 'F', "", 0});
        }
        else
        {
            QStringList roots = m_dispatcher ? m_dispatcher->devConfig().searchDirs : QStringList();
            if (!roots.contains(QDir::homePath()))
                roots.append(QDir::homePath());

            disconnect(m_debounceConn);
            m_debounceConn = connect(
                m_debounce,
                &QTimer::timeout,
                this,
                [this, q, roots]()
                {
                    if (!m_input || m_input->text().trimmed() != q)
                        return;
                    auto* watcher = new QFutureWatcher<QStringList>(this);
                    connect(
                        watcher,
                        &QFutureWatcher<QStringList>::finished,
                        this,
                        [this, watcher, q]()
                        {
                            watcher->deleteLater();
                            if (!m_input || m_input->text().trimmed() != q)
                                return;
                            m_fileQuery = q;
                            m_fileResults = watcher->result();
                            updateFuzzyFinder(m_input->text());
                        }
                    );
                    watcher->setFuture(QtConcurrent::run(FuzzyFinder::findMatchingFiles, q, 30, roots));
                }
            );
            m_debounce->start();
        }
    }

    QList<FinderItem> matches;
    const QString fileDisplayQuery = expandHomePath(q);
    for (FinderItem item : items)
    {
        if (cmdMode && item.kind != 'C')
            continue;

        int score;
        if (item.kind == 'C')
        {
            const QString matchTarget = item.action + " " + item.text;
            score = cmdQuery.isEmpty() ? 0 : scoreFuzzyQuery(cmdQuery, matchTarget);
            if (score < 0 && !cmdQuery.isEmpty())
                continue;
        }
        else if (item.kind == 'F')
        {
            score = scoreFuzzyQuery(fileDisplayQuery, item.text);
            const QFileInfo info(item.url);
            const int nameScore = scoreFuzzyQuery(q, info.fileName());
            if (nameScore >= 0)
                score = qMax(score, nameScore + 180);
            if (score < 0)
                continue;
            score += 80;
        }
        else
        {
            score = scoreFuzzyQuery(q, item.text);
            if (score < 0)
                continue;
        }

        item.score = score;
        matches.append(item);
    }

    std::stable_sort(
        matches.begin(),
        matches.end(),
        [](const FinderItem& a, const FinderItem& b)
        {
            if (a.score != b.score)
                return a.score > b.score;
            return a.text.size() < b.text.size();
        }
    );

    const int maxRows = cmdMode ? 30 : 20;
    for (int i = 0; i < matches.size() && i < maxRows; ++i)
    {
        const FinderItem& item = matches.at(i);
        QString prefix;
        switch (item.kind.toLatin1())
        {
            case 'T':
                prefix = "tab  ";
                break;
            case 'B':
                prefix = "bm   ";
                break;
            case 'H':
                prefix = "hist ";
                break;
            case 'F':
                prefix = "file ";
                break;
            case 'C':
                prefix = "cmd  ";
                break;
            default:
                prefix = "     ";
                break;
        }
        QListWidgetItem* lw = new QListWidgetItem(prefix + item.text.left(130));
        lw->setForeground(item.kind == 'C' ? QColor("#8fb6ff") : QColor("#aaaaaa"));
        lw->setData(Qt::UserRole, item.url);
        lw->setData(Qt::UserRole + 1, item.action);
        lw->setData(Qt::UserRole + 2, item.kind);
        m_list->addItem(lw);
    }

    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

QStringList FuzzyFinder::findMatchingFiles(const QString& query, int maxResults, const QStringList& searchRoots)
{
    struct FileMatch
    {
        QString path;
        int score;
    };
    QList<FileMatch> matches;
    QSet<QString> seenRoots;
    QSet<QString> seenFiles;

    const QString expandedQuery = expandHomePath(query);
    const bool pathMode =
        expandedQuery.startsWith('/') || expandedQuery.startsWith("./") || expandedQuery.startsWith("../");
    QStringList roots;
    QString matchText = query;

    if (pathMode)
    {
        const QString cleanQuery = QDir::cleanPath(expandedQuery);
        QFileInfo queryInfo(cleanQuery);
        if (queryInfo.exists() && queryInfo.isDir())
        {
            roots.append(queryInfo.canonicalFilePath());
            matchText.clear();
        }
        else
        {
            QString root = queryInfo.absolutePath();
            matchText = queryInfo.fileName();
            QFileInfo rootInfo(root);
            while (!rootInfo.exists() || !rootInfo.isDir())
            {
                const QString parent = QFileInfo(root).absolutePath();
                if (parent == root)
                    break;
                matchText = QFileInfo(root).fileName() + "/" + matchText;
                root = parent;
                rootInfo.setFile(root);
            }
            if (rootInfo.exists() && rootInfo.isDir())
                roots.append(rootInfo.canonicalFilePath());
        }
    }
    else
    {
        roots = searchRoots;
        matchText = query;
    }

    const QSet<QString> ignoredDirs = {
        ".git", ".hg", ".svn", "build", "node_modules", "target", "dist", "out", "__pycache__", ".cache", ".venv"
    };
    const int maxDepth = pathMode ? 8 : 5;
    const int maxVisited = pathMode ? 75000 : 25000;
    int visited = 0;

    for (const QString& rootSpec : roots)
    {
        if (matches.size() >= maxResults * 4 || visited >= maxVisited)
            break;

        const QString rootPath = QDir::cleanPath(expandHomePath(rootSpec));
        QFileInfo rootInfo(rootPath);
        if (!rootInfo.exists() || !rootInfo.isDir())
            continue;

        const QString canonicalRoot = rootInfo.canonicalFilePath();
        if (canonicalRoot.isEmpty() || seenRoots.contains(canonicalRoot))
            continue;
        seenRoots.insert(canonicalRoot);

        QList<QPair<QString, int>> pending;
        pending.append({canonicalRoot, 0});

        for (int dirIndex = 0; dirIndex < pending.size() && visited < maxVisited; ++dirIndex)
        {
            const QString dirPath = pending.at(dirIndex).first;
            const int depth = pending.at(dirIndex).second;
            QDirIterator it(dirPath, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Readable);

            while (it.hasNext() && visited < maxVisited)
            {
                const QString path = it.next();
                const QFileInfo info(path);

                if (info.isDir())
                {
                    const QString name = info.fileName();
                    if (depth + 1 >= maxDepth || name.startsWith('.') || ignoredDirs.contains(name))
                        continue;
                    const QString canonicalDir = info.canonicalFilePath();
                    if (!canonicalDir.isEmpty())
                        pending.append({canonicalDir, depth + 1});
                    continue;
                }

                ++visited;
                if (!info.isFile())
                    continue;

                const QString canonicalFile = info.canonicalFilePath();
                if (canonicalFile.isEmpty() || seenFiles.contains(canonicalFile))
                    continue;
                seenFiles.insert(canonicalFile);

                const QString rel = QDir(canonicalRoot).relativeFilePath(canonicalFile);
                int score = 0;
                if (matchText.isEmpty())
                {
                    score = 1000 - depth * 40 - rel.size();
                }
                else
                {
                    const int nameScore = scoreFuzzyQuery(matchText, info.fileName());
                    const int relScore = scoreFuzzyQuery(matchText, rel);
                    if (nameScore < 0 && relScore < 0)
                        continue;
                    score = qMax(nameScore >= 0 ? nameScore + 200 : -1, relScore);
                }

                matches.append({canonicalFile, score});
                if (matches.size() >= maxResults * 4)
                    break;
            }

            if (matches.size() >= maxResults * 4)
                break;
        }
    }

    std::stable_sort(
        matches.begin(),
        matches.end(),
        [](const FileMatch& a, const FileMatch& b)
        {
            if (a.score != b.score)
                return a.score > b.score;
            return a.path.size() < b.path.size();
        }
    );

    QStringList results;
    for (int i = 0; i < matches.size() && results.size() < maxResults; ++i)
        results.append(matches.at(i).path);
    return results;
}
