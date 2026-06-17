#include "src/core/utils.h"
#include "test_helpers.h"
#include <QRegularExpression>

void TestUtils::testFormatUrl_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("already https") << "https://archlinux.org" << "https://archlinux.org";
    QTest::newRow("domain no scheme") << "archlinux.org" << "https://archlinux.org";
    QTest::newRow("with path") << "github.com/anomalyco" << "https://github.com/anomalyco";
    QTest::newRow("search query") << "how to write qt" << "https://duckduckgo.com/?q=how%20to%20write%20qt";
    QTest::newRow("ip address") << "192.168.1.1" << "https://192.168.1.1";
    QTest::newRow("single word no dot") << "hello" << "https://duckduckgo.com/?q=hello";
    QTest::newRow("already http") << "http://example.com" << "http://example.com";
}

void TestUtils::testFormatUrl()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(Achroma::formatUrl(input), expected);
}

void TestUtils::testSearchUrl_data()
{
    QTest::addColumn<QString>("engine");
    QTest::addColumn<QString>("query");
    QTest::addColumn<QString>("expected");

    QTest::newRow("plain engine") << "https://google.com/search?q=" << "test" << "https://google.com/search?q=test";
    QTest::newRow("match template") << "https://ddg.gg/?q={{match}}" << "hello world"
                                    << "https://ddg.gg/?q=hello%20world";
    QTest::newRow("arg template") << "https://aur.archlinux.org/packages?K={{arg}}" << "firefox"
                                  << "https://aur.archlinux.org/packages?K=firefox";
    QTest::newRow("percent encode") << "https://google.com/search?q=" << "c++ tutorial"
                                    << "https://google.com/search?q=c%2B%2B%20tutorial";
}

void TestUtils::testSearchUrl()
{
    QFETCH(QString, engine);
    QFETCH(QString, query);
    QFETCH(QString, expected);
    QCOMPARE(Achroma::searchUrl(engine, query), expected);
}

void TestUtils::testStripTerminalControls()
{
    QString input = "\x1B[31mHello\x1B[0m \x1B[1mWorld\x1B[0m\n\x1B[2J";
    QString output = Achroma::stripTerminalControls(input);
    QVERIFY(!output.contains('\x1B'));
    QCOMPARE(output, QString("Hello World\n"));
}

void TestUtils::testStripCarriageReturn()
{
    QString input = QString("line1\r\nline2\r");
    QString output = Achroma::stripTerminalControls(input);
    QCOMPARE(output, QString("line1\nline2"));
}

void TestUtils::testUrlEncode()
{
    QCOMPARE(Achroma::urlEncode("hello world"), QString("hello%20world"));
    QCOMPARE(Achroma::urlEncode("c++"), QString("c%2B%2B"));
    QCOMPARE(Achroma::urlEncode("test/123"), QString("test%2F123"));
}

void TestUtils::testDuckDuckGoUrl()
{
    QCOMPARE(Achroma::duckDuckGoUrl("test"), QString("https://duckduckgo.com/?q=test"));
    QCOMPARE(Achroma::duckDuckGoUrl("hello world"), QString("https://duckduckgo.com/?q=hello%20world"));
}

void TestUtils::testBuiltinSearchEnginesExist()
{
    QVERIFY(Achroma::builtinSearchEngines.contains("g"));
    QVERIFY(Achroma::builtinSearchEngines.contains("w"));
    QVERIFY(Achroma::builtinSearchEngines.contains("yt"));
    QVERIFY(Achroma::builtinSearchEngines.contains("gh"));
    QVERIFY(Achroma::builtinSearchEngines.contains("ddg"));
    QCOMPARE(Achroma::builtinSearchEngines.size(), 5);
}

void TestUtils::testErrorPattern()
{
    QRegularExpression re(R"(error:\s*(.*))");
    auto m = re.match("error: undefined symbol 'foo'");
    QVERIFY(m.hasMatch());
    QCOMPARE(m.captured(1), QString("undefined symbol 'foo'"));

    m = re.match("warning: unused variable");
    QVERIFY(!m.hasMatch());
}

void TestUtils::testFatalErrorPattern()
{
    QRegularExpression re(R"((?:fatal\s+)?error:\s*(.*))");
    auto m = re.match("fatal error: stdio.h: No such file");
    QVERIFY(m.hasMatch());
    QCOMPARE(m.captured(1), QString("stdio.h: No such file"));
}

void TestUtils::testUrlPattern()
{
    QRegularExpression re(R"((https?://[^\s<>"']+))");
    auto m = re.match("Visit https://example.com/path for more info");
    QVERIFY(m.hasMatch());
    QCOMPARE(m.captured(1), QString("https://example.com/path"));

    m = re.match("No URL here");
    QVERIFY(!m.hasMatch());

    m = re.match("ftp://not-http.com");
    QVERIFY(!m.hasMatch());
}

void TestUtils::testLinkerErrorPattern()
{
    QRegularExpression re(R"(undefined\s+reference\s+to\s*[`'"'']([^`'"'']+)[`'"''])");
    auto m = re.match("undefined reference to `pthread_create'");
    QVERIFY(m.hasMatch());
    QCOMPARE(m.captured(1), QString("pthread_create"));
}

void TestUtils::testMultipleUrlsInOutput()
{
    QString output = "See https://example.com and https://test.org/path";
    QRegularExpression urlRe("https?://[^\\s<>\"']+");
    auto it = urlRe.globalMatch(output);
    int count = 0;
    while (it.hasNext())
    {
        it.next();
        count++;
    }
    QCOMPARE(count, 2);
}

void TestUtils::testLongUrlExtraction()
{
    QString output = "Downloading https://github.com/user/repo/releases/download/v1.0/pkg.tar.gz ...\nDone.\n";
    QRegularExpression urlRe("https?://[^\\s<>\"']+");
    auto it = urlRe.globalMatch(output);
    QVERIFY(it.hasNext());
    QCOMPARE(it.next().captured(), QString("https://github.com/user/repo/releases/download/v1.0/pkg.tar.gz"));
}

void TestUtils::testUtf8RoundTrip()
{
    QString original = QString::fromUtf8("error: implicit 'puts' \xe2\x80\x94 test");

    QByteArray raw = original.toUtf8();
    QString latin1Decoded = QString::fromLatin1(raw);

    QString fixed = QString::fromUtf8(latin1Decoded.toLatin1());
    QCOMPARE(fixed, original);

    QString unicode = QString::fromUtf8("\xe2\x80\x98hello\xe2\x80\x99");
    QByteArray r2 = unicode.toUtf8();
    QString l1 = QString::fromLatin1(r2);
    QString f2 = QString::fromUtf8(l1.toLatin1());
    QCOMPARE(f2, unicode);
}

void TestUtils::testStripComplexAnsi()
{
    QString input = "\x1B[1;31mBold Red\x1B[0m Normal \x1B[2J\x1B[H";
    QString output = Achroma::stripTerminalControls(input);
    QVERIFY(!output.contains('\x1B'));
    QCOMPARE(output, QString("Bold Red Normal "));
}

void TestUtils::testStripOscSequences()
{
    QString input = QString("Hello\x1B]0;title\x07 World\x1B]2;another\x1B\\End");
    QString output = Achroma::stripTerminalControls(input);
    QVERIFY(!output.contains('\x1B'));
    QCOMPARE(output, QString("Hello WorldEnd"));
}

void TestUtils::testPatternDebounceConcept()
{
    int debounceMs = 2000;

    bool firstFired = true;

    qint64 elapsed = 500;
    bool secondFired = (elapsed >= debounceMs);

    elapsed = 3000;
    bool thirdFired = (elapsed >= debounceMs);

    QVERIFY(firstFired);
    QVERIFY(!secondFired);
    QVERIFY(thirdFired);
}

void TestUtils::testUrlBarSearchPrefix()
{
    QStringList words = QString("g hello world").split(' ', Qt::SkipEmptyParts);
    QVERIFY(words.size() >= 2);
    QVERIFY(Achroma::builtinSearchEngines.contains(words[0]));
    QCOMPARE(words[0], QString("g"));
    QCOMPARE(words.mid(1).join(' '), QString("hello world"));

    words = QString("so test").split(' ', Qt::SkipEmptyParts);
    QVERIFY(!Achroma::builtinSearchEngines.contains(words[0]));

    words = QString("hello").split(' ', Qt::SkipEmptyParts);
    QVERIFY(words.size() < 2);
}

void TestUtils::testFormatUrlEdgeCases()
{
    QCOMPARE(Achroma::formatUrl(""), Achroma::duckDuckGoUrl(""));
    QCOMPARE(Achroma::formatUrl("https://sub.domain.com/path?a=1&b=2"), QString("https://sub.domain.com/path?a=1&b=2"));
    QCOMPARE(Achroma::formatUrl("192.168.1.1:8080"), QString("https://192.168.1.1:8080"));

    QVERIFY(Achroma::formatUrl("hello").startsWith("https://duckduckgo.com/"));
}

void TestUtils::testUrlEncodeEdgeCases()
{
    QCOMPARE(Achroma::urlEncode(""), QString(""));
    QCOMPARE(Achroma::urlEncode("a b"), QString("a%20b"));

    QCOMPARE(Achroma::urlEncode("hello@world"), QString("hello%40world"));
    QCOMPARE(Achroma::urlEncode("test&query=1"), QString("test%26query%3D1"));
}

void TestUtils::testKeyScrollScript()
{
    QString s = Achroma::keyScrollScript();
    QVERIFY(!s.isEmpty());
    QVERIFY(s.contains("__achromaKeys"));
    QVERIFY(s.contains("'j'"));
    QVERIFY(s.contains("'k'"));
    QVERIFY(s.contains("'d'"));
    QVERIFY(s.contains("'u'"));
    QVERIFY(s.contains("'G'"));
    QVERIFY(s.contains("'g'"));
    QVERIFY(s.contains("scrollBy"));
}

void TestUtils::testLinkHintsScript()
{
    QString s = Achroma::linkHintsScript();
    QVERIFY(!s.isEmpty());
    QVERIFY(s.contains("achroma-hints"));
    QVERIFY(s.contains("a[href]"));
    QVERIFY(s.contains("abcdefghijklmnopqrstuvwxyz"));
    QVERIFY(s.contains("getBoundingClientRect"));
    QVERIFY(s.contains("Escape"));
}

void TestUtils::testCodeBlockScript()
{
    QString s = Achroma::codeBlockScript();
    QVERIFY(!s.isEmpty());
    QVERIFY(s.contains("detectLang"));
    QVERIFY(s.contains("isCodeContainer"));
    QVERIFY(s.contains("JSON.stringify"));
    QVERIFY(s.contains("language-"));
    QVERIFY(s.contains("'cpp'"));
    QVERIFY(s.contains("'python'"));
    QVERIFY(s.contains("'rust'"));
}

void TestUtils::testInstallCmdScript()
{
    QString s = Achroma::installCmdScript();
    QVERIFY(!s.isEmpty());
    QVERIFY(s.contains("npm\\s+"));
    QVERIFY(s.contains("pip\\s+install"));
    QVERIFY(s.contains("pip3\\s+install"));
    QVERIFY(s.contains("cargo\\s+install"));
    QVERIFY(s.contains("apt\\s+install"));
    QVERIFY(s.contains("apt-get\\s+install"));
    QVERIFY(s.contains("git\\s+clone"));
    QVERIFY(s.contains("brew\\s+install"));
    QVERIFY(s.contains("document.body"));
}

void TestUtils::testHomePageHtml()
{
    QString html = Achroma::homePageHtml();
    QVERIFY(!html.isEmpty());
    QVERIFY(html.contains("<!DOCTYPE html>"));
    QVERIFY(html.contains("A C H R O M A"));
    QVERIFY(html.contains("canvas"));
    QVERIFY(html.contains("Search the web"));
    QVERIFY(html.contains("GitHub"));
    QVERIFY(html.contains("DuckDuckGo"));
}

void TestUtils::testInstallCmdNpmMatch()
{
    QRegularExpression re(R"((npm\s+(install|i)\s+[\w@\/.-]+))", QRegularExpression::CaseInsensitiveOption);
    QVERIFY(re.match("npm install express --save").hasMatch());
    QVERIFY(re.match("npm i react@18.2.0").hasMatch());
    QVERIFY(re.match("npm install @angular/cli -g").hasMatch());
    QVERIFY(!re.match("npm install").hasMatch());
    QVERIFY(!re.match("just some text").hasMatch());
}

void TestUtils::testInstallCmdPipMatch()
{
    QRegularExpression re(R"((pip3?\s+install\s+[\w\[\].-]+))", QRegularExpression::CaseInsensitiveOption);
    QVERIFY(re.match("pip install requests").hasMatch());
    QVERIFY(re.match("pip install django==4.2").hasMatch());
    QVERIFY(re.match("pip install numpy[all]").hasMatch());
    QVERIFY(re.match("pip3 install flask").hasMatch());
    QVERIFY(!re.match("hello world").hasMatch());
}

void TestUtils::testInstallCmdCargoMatch()
{
    QRegularExpression re(R"((cargo\s+install\s+[\w-]+))", QRegularExpression::CaseInsensitiveOption);
    QVERIFY(re.match("cargo install ripgrep").hasMatch());
    QVERIFY(re.match("cargo install bat").hasMatch());
    QVERIFY(re.match("cargo install cargo-watch --locked").hasMatch());
    QVERIFY(!re.match("npm install cargo").hasMatch());
}

void TestUtils::testInstallCmdAptMatch()
{
    QRegularExpression re(
        R"((apt\s+install\s+[\w-]+)|(apt-get\s+install\s+[\w-]+)|(dnf\s+install\s+[\w-]+)|(pacman\s+-S\s+[\w-]+))",
        QRegularExpression::CaseInsensitiveOption
    );
    QVERIFY(re.match("apt install gcc").hasMatch());
    QVERIFY(re.match("apt-get install cmake").hasMatch());
    QVERIFY(re.match("dnf install qt6-qtbase-devel").hasMatch());
    QVERIFY(re.match("pacman -S firefox").hasMatch());
    QVERIFY(!re.match("just text").hasMatch());
}

void TestUtils::testInstallCmdGitCloneMatch()
{
    QRegularExpression re(R"((git\s+clone\s+[\w:\/.@-]+))", QRegularExpression::CaseInsensitiveOption);
    QVERIFY(re.match("git clone https://github.com/user/repo.git").hasMatch());
    QVERIFY(re.match("git clone git@github.com:user/repo.git").hasMatch());
    QVERIFY(re.match("git clone --depth 1 https://example.com/repo").hasMatch());
    QVERIFY(!re.match("echo hello").hasMatch());
}

void TestUtils::testInstallCmdNoFalsePositives()
{
    QRegularExpression re(
        R"((npm\s+(install|i)\s+[\w@\/.-]+)|(pip\s+install\s+[\w\[\].-]+)|(cargo\s+install\s+[\w-]+)|(apt\s+install\s+[\w-]+))",
        QRegularExpression::CaseInsensitiveOption
    );
    QVERIFY(!re.match("hello world").hasMatch());
    QVERIFY(!re.match("the installation was successful").hasMatch());
    QVERIFY(!re.match("").hasMatch());
    QVERIFY(!re.match(" ").hasMatch());
}

QTEST_MAIN(TestUtils)
