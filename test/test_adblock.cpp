#include "src/net/adblockinterceptor.h"
#include <QTemporaryFile>
#include <QUrl>
#include <QtTest>

static QTemporaryFile* makeBlocklist(const QString& content)
{
    auto* f = new QTemporaryFile();
    Q_UNUSED(f->open());
    f->write(content.toUtf8());
    f->close();
    return f;
}

class TestAdBlock : public QObject
{
    Q_OBJECT

private slots:
    void testEnabledByDefault()
    {
        AdBlockInterceptor interceptor;
        QVERIFY(interceptor.isEnabled());
    }

    void testSetEnabled()
    {
        AdBlockInterceptor interceptor;
        interceptor.setEnabled(false);
        QVERIFY(!interceptor.isEnabled());
        interceptor.setEnabled(true);
        QVERIFY(interceptor.isEnabled());
    }

    void testEmptyInterceptDoesNotCrash()
    {
        AdBlockInterceptor interceptor;

        QVERIFY(interceptor.isEnabled());
    }

    void testLoadEmptyFile()
    {
        AdBlockInterceptor interceptor;
        QTemporaryFile* f = makeBlocklist("");
        interceptor.loadBlocklist(f->fileName());
        delete f;
        QVERIFY(interceptor.isEnabled());
    }

    void testMatchExactDomain()
    {
        QTemporaryFile* f = makeBlocklist("doubleclick.net\n");
        AdBlockInterceptor interceptor;
        interceptor.loadBlocklist(f->fileName());
        delete f;
        QVERIFY(interceptor.matchesDomain(QUrl("https://doubleclick.net/ad.js")));
        QVERIFY(!interceptor.matchesDomain(QUrl("https://example.com/")));
    }

    void testMatchSubdomain()
    {
        QTemporaryFile* f = makeBlocklist("doubleclick.net\n");
        AdBlockInterceptor interceptor;
        interceptor.loadBlocklist(f->fileName());
        delete f;
        QVERIFY(interceptor.matchesDomain(QUrl("https://ad.doubleclick.net/pixel")));
        QVERIFY(interceptor.matchesDomain(QUrl("https://a.b.doubleclick.net/track")));
    }

    void testNoMatchUnrelated()
    {
        QTemporaryFile* f = makeBlocklist("doubleclick.net\n");
        AdBlockInterceptor interceptor;
        interceptor.loadBlocklist(f->fileName());
        delete f;
        QVERIFY(!interceptor.matchesDomain(QUrl("https://google.com/")));
        QVERIFY(!interceptor.matchesDomain(QUrl("https://notdoubleclick.net/")));
    }

    void testMultipleDomains()
    {
        QTemporaryFile* f = makeBlocklist("doubleclick.net\ngooglesyndication.com\n");
        AdBlockInterceptor interceptor;
        interceptor.loadBlocklist(f->fileName());
        delete f;
        QVERIFY(interceptor.matchesDomain(QUrl("https://doubleclick.net/ad")));
        QVERIFY(interceptor.matchesDomain(QUrl("https://ad.googlesyndication.com/pagead")));
        QVERIFY(!interceptor.matchesDomain(QUrl("https://google.com/")));
    }

    void testSkipCommentsAndBlank()
    {
        QTemporaryFile* f = makeBlocklist("# Blocklist\n! Comment\n\n  \ndoubleclick.net\n");
        AdBlockInterceptor interceptor;
        interceptor.loadBlocklist(f->fileName());
        delete f;
        QVERIFY(interceptor.matchesDomain(QUrl("https://doubleclick.net/")));
    }

    void testEasyListFormat()
    {
        QTemporaryFile* f = makeBlocklist("||evil.com^\n||ads.other.net^\n");
        AdBlockInterceptor interceptor;
        interceptor.loadBlocklist(f->fileName());
        delete f;
        QVERIFY(interceptor.matchesDomain(QUrl("https://evil.com/track")));
        QVERIFY(interceptor.matchesDomain(QUrl("https://cdn.ads.other.net/banner")));
    }
};

QTEST_MAIN(TestAdBlock)
#include "test_adblock.moc"
