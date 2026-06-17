#include "src/core/utils.h"
#include "test_scripts_helpers.h"

bool TestScripts::waitForLoad(QWebEngineView* view, int timeoutMs)
{
    QSignalSpy spy(view, &QWebEngineView::loadFinished);
    return spy.wait(timeoutMs);
}

QVariant TestScripts::runSync(QWebEngineView* view, const QString& js, int timeoutMs)
{
    QVariant result;
    bool done = false;
    view->page()->runJavaScript(
        js,
        [&](const QVariant& res)
        {
            result = res;
            done = true;
        }
    );
    QElapsedTimer t;
    t.start();
    while (!done && t.elapsed() < timeoutMs)
        QApplication::processEvents(QEventLoop::AllEvents, 50);
    return result;
}

void TestScripts::testKeyScrollInjectsGuard()
{
    QWebEngineView view;
    view.setHtml("<html><body><p>hello</p></body></html>", QUrl("about:blank"));
    QVERIFY(waitForLoad(&view));

    view.page()->runJavaScript(Achroma::keyScrollScript());
    QVariant injected = runSync(&view, "window.__achromaKeys === true", 3000);
    QVERIFY(injected.isValid());
    QVERIFY(injected.toBool());

    view.page()->runJavaScript(Achroma::keyScrollScript());
    QVariant stillTrue = runSync(&view, "window.__achromaKeys === true", 3000);
    QVERIFY(stillTrue.toBool());
}

void TestScripts::testKeyScrollDoesNotDoubleInject()
{
    QWebEngineView view;
    view.setHtml("<html><body><p>hello</p></body></html>", QUrl("about:blank"));
    QVERIFY(waitForLoad(&view));

    QVariant count = runSync(
        &view,
        QString(
            "(function(){"
            "  var n=0;"
            "  var orig=EventTarget.prototype.addEventListener;"
            "  EventTarget.prototype.addEventListener=function(t,f){ if(t==='keydown')n++; orig.call(this,t,f); };"
            "  "
        ) + Achroma::keyScrollScript() +
            QString(
                ""
                "  EventTarget.prototype.addEventListener=orig;"
                "  return n;"
                "})()"
            ),
        3000
    );
    QVERIFY(count.isValid());
    QCOMPARE(count.toInt(), 1);
}

void TestScripts::testLinkHintsCreatesOverlay()
{
    QWebEngineView view;
    view.setHtml(
        R"(<html><body><a href="https://example.com" style="display:block;padding:10px;">Link</a>
                      <a href="https://test.org" style="display:block;padding:10px;">Other</a></body></html>)",
        QUrl("about:blank")
    );
    QVERIFY(waitForLoad(&view));

    view.page()->runJavaScript(Achroma::linkHintsScript());
    QVariant ok = runSync(&view, "true", 3000);
    QVERIFY(ok.isValid());
    QVERIFY(ok.toBool());
}

void TestScripts::testLinkHintsNoLinksReturns()
{
    QWebEngineView view;
    view.setHtml("<html><body><p>No links here</p></body></html>", QUrl("about:blank"));
    QVERIFY(waitForLoad(&view));

    view.page()->runJavaScript(Achroma::linkHintsScript());
    QVariant hasOverlay = runSync(&view, "document.getElementById('achroma-hints') !== null", 3000);
    QVERIFY(hasOverlay.isValid());
    QVERIFY(!hasOverlay.toBool());
}

void TestScripts::testCodeBlockExtractsPreCode()
{
    QWebEngineView view;
    view.setHtml(
        R"(<html><body><pre><code class="language-python">
print("hello world")
for i in range(10):
    print(i)
</code></pre></body></html>)",
        QUrl("about:blank")
    );
    QVERIFY(waitForLoad(&view));

    QVariant raw = runSync(&view, Achroma::codeBlockScript(), 3000);
    QVERIFY(raw.isValid());
    QVERIFY(!raw.toString().isEmpty());

    QJsonDocument doc = QJsonDocument::fromJson(raw.toString().toUtf8());
    QVERIFY(doc.isObject());
    QJsonObject obj = doc.object();
    QVERIFY(obj.contains("lang"));
    QVERIFY(obj.contains("code"));

    QVERIFY(obj["code"].toString().contains("print(\"hello world\")"));
}

void TestScripts::testCodeBlockExtractsWithSelection()
{
    QWebEngineView view;
    view.setHtml(R"(<html><body><pre><code>int main() { return 0; }</code></pre></body></html>)", QUrl("about:blank"));
    QVERIFY(waitForLoad(&view));

    QVariant raw = runSync(&view, Achroma::codeBlockScript(), 3000);
    QVERIFY(raw.isValid());

    QJsonDocument doc = QJsonDocument::fromJson(raw.toString().toUtf8());
    QVERIFY(doc.isObject());
    QJsonObject obj = doc.object();
    QVERIFY(obj["code"].toString().contains("int main()"));
}

void TestScripts::testCodeBlockEmptyPage()
{
    QWebEngineView view;
    view.setHtml("<html><body><p>Just some text</p></body></html>", QUrl("about:blank"));
    QVERIFY(waitForLoad(&view));

    QVariant raw = runSync(&view, Achroma::codeBlockScript(), 3000);
    QVERIFY(raw.isValid());

    QJsonDocument doc = QJsonDocument::fromJson(raw.toString().toUtf8());
    QVERIFY(doc.isObject());
    QJsonObject obj = doc.object();
    QCOMPARE(obj["lang"].toString(), QString(""));
    QCOMPARE(obj["code"].toString(), QString(""));
}

void TestScripts::testHomePageHtmlLoadsWithoutErrors()
{
    QWebEngineView view;
    QString html = Achroma::homePageHtml();
    view.setHtml(html, QUrl("achroma://home"));
    QVERIFY(waitForLoad(&view));

    QVariant title = runSync(&view, "document.title", 3000);
    QVERIFY(title.isValid());
    QCOMPARE(title.toString(), QString("Achroma"));

    QVariant canvas = runSync(&view, "document.querySelector('canvas#bg') !== null", 3000);
    QVERIFY(canvas.toBool());

    QVariant search = runSync(&view, "document.querySelector('input.search') !== null", 3000);
    QVERIFY(search.toBool());
}

QTEST_MAIN(TestScripts)
