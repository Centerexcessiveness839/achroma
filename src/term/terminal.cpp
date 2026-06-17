#include "term/terminal.h"
#include "commands.h"
#include <QFont>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTimer>
#include <qtermwidget.h>
#include <cerrno>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

Terminal::Terminal(QWidget* parent) : QObject(parent)
{
    m_term = new QTermWidget(0, parent);
    m_term->setColorScheme("WhiteOnBlack");
    m_term->setScrollBarPosition(QTermWidget::ScrollBarRight);

    QFont termFont("Source Code Pro");
    termFont.setStyleHint(QFont::TypeWriter);
    termFont.setFixedPitch(true);
    termFont.setPixelSize(18);
    m_term->setTerminalFont(termFont);

    m_term->setShellProgram("/bin/bash");
    m_term->setArgs(QStringList() << "-i");
    m_term->setAutoClose(true);
    m_term->setTerminalSizeHint(false);

    connect(m_term, SIGNAL(receivedData(QString)), this, SLOT(onTerminalData(QString)));

    m_term->installEventFilter(this);
}

void Terminal::start()
{
    m_exitEmitted = false;
    m_lastShellPid = -1;
    m_term->startShellProgram();
    startExitWatcher();

    QTimer::singleShot(
        0,
        this,
        [this]()
        {
            QScrollBar* sb = m_term->findChild<QScrollBar*>();
            if (sb)
                sb->setStyleSheet(
                    "QScrollBar:vertical { width: 8px; background: #111; }"
                    "QScrollBar::handle:vertical { background: #444; border-radius: 3px; min-height: 20px; }"
                    "QScrollBar::handle:vertical:hover { background: #666; }"
                    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                );
        }
    );
}

void Terminal::sendText(const QString& text)
{
    m_term->sendText(text);
}

void Terminal::setFocus()
{
    m_term->setFocus();
}

void Terminal::applyAppearance(const AppConfig& cfg)
{
    m_baseFontSize = cfg.fontSize;
    m_zoomLevel = 0;
    QFont termFont(cfg.fontFamily);
    termFont.setStyleHint(QFont::TypeWriter);
    termFont.setFixedPitch(true);
    termFont.setPixelSize(cfg.fontSize);
    m_term->setTerminalFont(termFont);
}

void Terminal::onTerminalData(const QString& text)
{
    emit outputAvailable(QString::fromUtf8(text.toLatin1()));
}

void Terminal::copy()
{
    m_term->copyClipboard();
}

void Terminal::paste()
{
    m_term->pasteClipboard();
}

void Terminal::pasteSelection()
{
    m_term->pasteSelection();
}

void Terminal::zoomIn()
{
    m_zoomLevel++;
    applyZoom();
}

void Terminal::zoomOut()
{
    if (m_zoomLevel > -14)
    {
        m_zoomLevel--;
        applyZoom();
    }
}

void Terminal::zoomReset()
{
    m_zoomLevel = 0;
    applyZoom();
}

void Terminal::applyZoom()
{
    QFont f = m_term->getTerminalFont();
    int size = m_baseFontSize + m_zoomLevel;
    if (size < 4)
        size = 4;
    f.setPixelSize(size);
    m_term->setTerminalFont(f);
}

void Terminal::clear()
{
    m_term->clear();
}

void Terminal::scrollToEnd()
{
    m_term->scrollToEnd();
}

void Terminal::startExitWatcher()
{
    if (m_exitWatcher)
    {
        m_exitWatcher->start();
        return;
    }

    m_exitWatcher = new QTimer(this);
    m_exitWatcher->setInterval(250);
    connect(
        m_exitWatcher,
        &QTimer::timeout,
        this,
        [this]()
        {
            if (m_exitEmitted)
            {
                m_exitWatcher->stop();
                return;
            }

            const int pid = m_term->getShellPID();
            if (pid > 0)
            {
                m_lastShellPid = pid;
                errno = 0;
                if (kill(pid, 0) == 0 || errno == EPERM)
                    return;
            }
            else if (m_lastShellPid <= 0)
            {
                return;
            }

            handleShellExit();
        }
    );
    m_exitWatcher->start();
}

void Terminal::handleShellExit()
{
    if (m_exitEmitted)
        return;

    m_exitEmitted = true;
    if (m_exitWatcher)
        m_exitWatcher->stop();

    emit outputAvailable("# shell process exited\n");
    emit shellExited();
}

void Terminal::toggleSearchBar()
{
    m_term->toggleShowSearchBar();
}

void Terminal::setProfile(const QString& name)
{
    m_term->setColorScheme(name);
}

QStringList Terminal::availableProfiles()
{
    return QTermWidget::availableColorSchemes();
}

bool Terminal::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_term && event->type() == QEvent::Close)
    {
        handleShellExit();
        return false;
    }

    if (obj == m_term && event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::MiddleButton)
        {
            pasteSelection();
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

QString Terminal::shellCwd() const
{
    int pid = m_term ? m_term->getShellPID() : -1;
    if (pid <= 0 && m_lastShellPid > 0)
        pid = m_lastShellPid;
    if (pid <= 0)
        return QString();
    QString procPath = "/proc/" + QString::number(pid) + "/cwd";
    return QFileInfo(procPath).symLinkTarget();
}
