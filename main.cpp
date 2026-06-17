#include "ui/splash.h"
#include "ui/window.h"
#include <QApplication>
#include <QTimer>

int main(int argc, char* argv[])
{
    // Disable Chromium's native Sec-CH-UA header generation so our
    // QWebEngineUrlRequestInterceptor can supply Chrome-branded headers
    // without Chromium overwriting them with QtWebEngine brands.
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-features=UserAgentClientHint");

    QApplication app(argc, argv);
    app.setOrganizationName("achroma");
    app.setApplicationName("achroma");
    app.setDesktopFileName("Achroma");
    app.setWindowIcon(QIcon(":/achroma.svg"));

    auto* splash = new SplashScreen();
    auto* timer = new QTimer(&app);
    timer->setSingleShot(true);
    QObject::connect(
        timer,
        &QTimer::timeout,
        splash,
        [splash, &app]()
        {
            splash->showSplash(
                [&app]()
                {
                    auto* w = new AchromaWindow();
                    w->show();
                }
            );
        }
    );
    timer->start(50);

    return app.exec();
}
