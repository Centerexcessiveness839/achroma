#include "ui/splash.h"
#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QPropertyAnimation>
#include <QScreen>
#include <QTimer>

SplashScreen::SplashScreen()
{
    m_logo = QPixmap(":/achroma.svg");

    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::SplashScreen);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(300, 300);

    m_opacity = new QGraphicsOpacityEffect(this);
    m_opacity->setOpacity(0.0);
    setGraphicsEffect(m_opacity);

    m_fadeIn = new QPropertyAnimation(m_opacity, "opacity", this);
    m_fadeIn->setDuration(300);
    m_fadeIn->setStartValue(0.0);
    m_fadeIn->setEndValue(1.0);
    m_fadeIn->setEasingCurve(QEasingCurve::OutCubic);

    m_fadeOut = new QPropertyAnimation(m_opacity, "opacity", this);
    m_fadeOut->setDuration(400);
    m_fadeOut->setStartValue(1.0);
    m_fadeOut->setEndValue(0.0);
    m_fadeOut->setEasingCurve(QEasingCurve::InCubic);

    QScreen* screen = QApplication::primaryScreen();
    if (screen)
    {
        QRect geom = screen->geometry();
        move((geom.width() - 300) / 2, (geom.height() - 300) / 2);
    }
}

void SplashScreen::showSplash(std::function<void()> onFinished)
{
    show();

    connect(
        m_fadeIn,
        &QPropertyAnimation::finished,
        this,
        [this, onFinished]()
        {
            QTimer::singleShot(
                800,
                this,
                [this, onFinished]()
                {
                    connect(
                        m_fadeOut,
                        &QPropertyAnimation::finished,
                        this,
                        [this, onFinished]()
                        {
                            onFinished();
                            deleteLater();
                        }
                    );
                    m_fadeOut->start();
                }
            );
        }
    );
    m_fadeIn->start();
}

void SplashScreen::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    p.fillRect(rect(), QColor("#0A0A0A"));

    if (!m_logo.isNull())
    {
        QPixmap scaled = m_logo.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        p.drawPixmap((width() - 80) / 2, 85, scaled);
    }

    QFont f("Source Code Pro");
    f.setStyleHint(QFont::TypeWriter);
    f.setPixelSize(20);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor("#FFFFFF"));
    p.drawText(QRect(0, 175, width(), 30), Qt::AlignCenter, "ACHROMA");

    QFont sf("Source Code Pro");
    sf.setStyleHint(QFont::TypeWriter);
    sf.setPixelSize(10);
    p.setFont(sf);
    p.setPen(QColor("#666666"));
    p.drawText(QRect(0, 205, width(), 20), Qt::AlignCenter, "BROWSER  ENVIRONMENT");
}
