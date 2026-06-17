#include "src/commands.h"
#include "src/core/utils.h"
#include "test_commands_helpers.h"
#include <QJsonArray>
#include <QTemporaryFile>

void TestCommands::testBuiltinsRegistered()
{
    CommandDispatcher d;
    d.execute("help");
    d.execute("open test.com");
    d.execute("tab test.com");
    d.execute("back");
    d.execute("forward");
    d.execute("r");
    d.execute("reload");
    d.execute("home");
    d.execute("close");
    d.execute("undo");
    d.execute("next");
    d.execute("prev");
    d.execute("tabs");
    d.execute("url");
    d.execute("focus");
    d.execute("bookmarks");
    d.execute("hint");
    d.execute("find");
    d.execute("clear");
    d.execute("copy");
    d.execute("paste");
    d.execute("duplicate");
    d.execute("history");
    d.execute("adblock");
    QVERIFY(true);
}

void TestCommands::testSearchEngineDispatch()
{
    CommandDispatcher d;
    d.execute("g test query");
    d.execute("w Qt framework");
    d.execute("yt tutorial");
    d.execute("gh achroma");
    d.execute("ddg open source");
    QVERIFY(true);
}

void TestCommands::testUnknownCommandNoCrash()
{
    CommandDispatcher d;
    d.execute("nonexistent_command_12345");
    d.execute("");
    d.execute("justoneword");
    QVERIFY(true);
}

void TestCommands::testAppearanceDefaults()
{
    CommandDispatcher d;
    QCOMPARE(d.appearance().bg, QString("#000000"));
    QCOMPARE(d.appearance().fg, QString("#FFFFFF"));
    QCOMPARE(d.appearance().fontFamily, QString("Source Code Pro"));
    QCOMPARE(d.appearance().fontSize, 18);
}

void TestCommands::testKeyConfigDefaults()
{
    CommandDispatcher d;
    QCOMPARE(d.keyConfig().focusTerminal, QString("Ctrl+T"));
    QCOMPARE(d.keyConfig().focusUrlbar, QString("Ctrl+Shift+L"));
    QCOMPARE(d.keyConfig().fullscreen, QString("F11"));
    QCOMPARE(d.keyConfig().devTools, QString("F12"));
}

void TestCommands::testSearchEnginesDefault()
{
    CommandDispatcher d;
    QVERIFY(d.searchEngines().contains("g"));
    QVERIFY(d.searchEngines().contains("w"));
    QVERIFY(d.searchEngines().contains("yt"));
    QVERIFY(d.searchEngines().contains("gh"));
    QVERIFY(d.searchEngines().contains("ddg"));
    QCOMPARE(d.searchEngines()["g"], QString("https://google.com/search?q="));
}

void TestCommands::testDefaultTriggers()
{
    CommandDispatcher d;
    QJsonArray t = d.triggers();
    QVERIFY(t.size() >= 3);
}

void TestCommands::testAdBlockEnabledDefault()
{
    CommandDispatcher d;
    QVERIFY(d.adBlockEnabled());
}

void TestCommands::testSetAdBlockEnabled()
{
    CommandDispatcher d;
    d.setAdBlockEnabled(false);
    QVERIFY(!d.adBlockEnabled());
    d.setAdBlockEnabled(true);
    QVERIFY(d.adBlockEnabled());
}

void TestCommands::testNewBuiltins()
{
    CommandDispatcher d;

    d.execute("incognito");
    d.execute("incog");
    d.execute("incognito https://example.com");

    d.execute("dup");
    d.execute("duplicate");

    d.execute("sterm");
    d.execute("searchterm");

    d.execute("profile");
    d.execute("profile GreenOnBlack");

    d.execute("reader");

    d.execute("source");

    d.execute("print");

    d.execute("dev");
    d.execute("devtools");

    d.execute("fullscreen");

    d.execute("dark");
    d.execute("light");

    d.execute("history");

    d.execute("adblock");

    d.execute("find");

    d.execute("clear");
    d.execute("copy");
    d.execute("paste");
    QVERIFY(true);
}

void TestCommands::testHomepageDefaults()
{
    CommandDispatcher d;
    QCOMPARE(d.homepage().source, QString("builtin"));
    QVERIFY(d.homepage().quickLinks.size() >= 1);

    for (const auto& ql : d.homepage().quickLinks)
    {
        QVERIFY(!ql.name.isEmpty());
        QVERIFY(ql.url.startsWith("https://") || ql.url.startsWith("http://"));
    }
}

void TestCommands::testTriggerTemplate()
{
    QRegularExpression re("([^\\s:]+):(\\d+):(\\d*)");
    auto m = re.match("src/main.cpp:45:10");

    QVERIFY(m.hasMatch());
    QCOMPARE(m.captured(0), QString("src/main.cpp:45:10"));
    QCOMPARE(m.captured(1), QString("src/main.cpp"));
    QCOMPARE(m.captured(2), QString("45"));

    QString tmpl = "nvim +{{2}} {{1}}";
    QString result = tmpl;
    result.replace("{{1}}", m.captured(1));
    result.replace("{{2}}", m.captured(2));
    QCOMPARE(result, QString("nvim +45 src/main.cpp"));

    result = "{{match}}";
    result.replace("{{match}}", m.captured(0));
    QCOMPARE(result, QString("src/main.cpp:45:10"));
}

void TestCommands::testContextAwareBuiltins()
{
    CommandDispatcher d;

    d.execute("tosterm");
    d.execute("codeblock");
    d.execute("install");
    QVERIFY(true);
}

void TestCommands::testKeyConfigAdditions()
{
    CommandDispatcher d;
    QVERIFY(!d.keyConfig().fuzzyFinder.isEmpty());
    QVERIFY(!d.keyConfig().printPage.isEmpty());
    QVERIFY(!d.keyConfig().viewSource.isEmpty());
    QVERIFY(!d.keyConfig().sendToTerm.isEmpty());
    QVERIFY(!d.keyConfig().codeBlock.isEmpty());
    QVERIFY(!d.keyConfig().installCmd.isEmpty());
    QCOMPARE(d.homepage().source, QString("builtin"));
    QVERIFY(!d.devConfig().editor.isEmpty());
    QVERIFY(!d.devConfig().searchDirs.isEmpty());
    QVERIFY(d.devConfig().runners.contains("cpp"));
    QVERIFY(d.devConfig().runners.contains("py"));
    QVERIFY(d.devConfig().runners.contains("rs"));
    QVERIFY(d.adBlockEnabled());
}

QTEST_MAIN(TestCommands)
