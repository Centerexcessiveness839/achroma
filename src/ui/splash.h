#pragma once
#include <QPixmap>
#include <QWidget>
#include <functional>

class QGraphicsOpacityEffect;
class QPropertyAnimation;

class SplashScreen : public QWidget
{
    Q_OBJECT
public:
    explicit SplashScreen();

    void showSplash(std::function<void()> onFinished);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QPixmap m_logo;
    QGraphicsOpacityEffect* m_opacity = nullptr;
    QPropertyAnimation* m_fadeIn = nullptr;
    QPropertyAnimation* m_fadeOut = nullptr;
};
