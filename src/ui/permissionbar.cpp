#include "ui/permissionbar.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

PermissionBar::PermissionBar(QWidget* parent) : QFrame(parent)
{
    setFixedHeight(30);
    hide();

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 4, 0);
    layout->setSpacing(6);

    m_message = new QLabel(this);
    m_message->setStyleSheet("QLabel { color: #aaa; font-family: monospace; font-size: 11px; border: none; }");
    layout->addWidget(m_message);

    layout->addStretch();

    m_allowBtn = new QPushButton("Allow", this);
    m_allowBtn->setStyleSheet(
        "QPushButton { color: #0a0; background: #111; border: 1px solid #333; padding: 2px 10px; "
        "font-family: monospace; font-size: 11px; }"
        "QPushButton:hover { color: #0f0; background: #222; border-color: #0a0; }"
        "QPushButton:focus { border-color: #0a0; }"
    );
    m_allowBtn->setFixedHeight(22);
    m_allowBtn->setFocusPolicy(Qt::StrongFocus);
    layout->addWidget(m_allowBtn);

    m_denyBtn = new QPushButton("Deny", this);
    m_denyBtn->setStyleSheet(
        "QPushButton { color: #a00; background: #111; border: 1px solid #333; padding: 2px 10px; "
        "font-family: monospace; font-size: 11px; }"
        "QPushButton:hover { color: #f00; background: #222; border-color: #a00; }"
        "QPushButton:focus { border-color: #a00; }"
    );
    m_denyBtn->setFixedHeight(22);
    m_denyBtn->setFocusPolicy(Qt::StrongFocus);
    layout->addWidget(m_denyBtn);

    m_closeBtn = new QPushButton("x", this);
    m_closeBtn->setStyleSheet(
        "QPushButton { color: #555; background: #111; border: 1px solid #333; padding: 2px 6px; "
        "font-family: monospace; font-size: 11px; }"
        "QPushButton:hover { color: #f00; background: #222; }"
    );
    m_closeBtn->setFixedSize(22, 22);
    m_closeBtn->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_closeBtn);

    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(30000);

    connect(
        m_allowBtn,
        &QPushButton::clicked,
        this,
        [this]()
        {
            m_timeout->stop();
            if (m_callback)
            {
                m_callback(true);
            }
            hide();
        }
    );

    connect(
        m_denyBtn,
        &QPushButton::clicked,
        this,
        [this]()
        {
            m_timeout->stop();
            if (m_callback)
            {
                m_callback(false);
            }
            hide();
        }
    );

    connect(m_closeBtn, &QPushButton::clicked, this, [this]() { denyAndHide(); });

    connect(m_timeout, &QTimer::timeout, this, [this]() { denyAndHide(); });
}

void PermissionBar::request(const QUrl& origin, QWebEnginePage::Feature feature, std::function<void(bool)> callback)
{
    m_callback = std::move(callback);
    m_message->setText(origin.host() + " wants to use your " + featureLabel(feature));
    m_allowBtn->setFocus();
    show();
    m_timeout->start();
}

void PermissionBar::denyAndHide()
{
    m_timeout->stop();
    if (m_callback)
    {
        m_callback(false);
    }
    hide();
}

QString PermissionBar::featureLabel(QWebEnginePage::Feature feature)
{
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    switch (feature)
    {
        case QWebEnginePage::MediaAudioCapture:
            return "microphone";
        case QWebEnginePage::MediaVideoCapture:
            return "camera";
        case QWebEnginePage::MediaAudioVideoCapture:
            return "camera + microphone";
        case QWebEnginePage::Geolocation:
            return "location";
        case QWebEnginePage::Notifications:
            return "notifications";
        case QWebEnginePage::DesktopAudioVideoCapture:
            return "screen sharing";
        default:
            return "device";
    }
    QT_WARNING_POP
}
