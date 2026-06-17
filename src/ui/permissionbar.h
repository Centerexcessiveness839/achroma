#pragma once
#include <QFrame>
#include <QUrl>
#include <QWebEnginePage>
#include <functional>

class QLabel;
class QPushButton;
class QTimer;

class PermissionBar : public QFrame
{
    Q_OBJECT
public:
    explicit PermissionBar(QWidget* parent = nullptr);

    void request(const QUrl& origin, QWebEnginePage::Feature feature, std::function<void(bool)> callback);

private:
    void denyAndHide();
    static QString featureLabel(QWebEnginePage::Feature feature);

    QLabel* m_message = nullptr;
    QPushButton* m_allowBtn = nullptr;
    QPushButton* m_denyBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
    QTimer* m_timeout = nullptr;
    std::function<void(bool)> m_callback;
};
