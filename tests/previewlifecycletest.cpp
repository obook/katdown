// previewlifecycletest.cpp
// The preview outlives the document it shows and keeps rendering it, and its page stays
// within bounds: the document's folder, no script of the document, no other page.
// Code by uwuclxdy and Olivier Booklage.
// License: GPL-3.0-or-later

// Kate destroys a document when its editor tab closes, and it does so in two shapes:
// closing one tab runs closeUrl() first (which empties the url and then the buffer),
// while closing several at once deletes the document with its text still intact.
// Either way the preview widget outlives the document, so it has to keep rendering
// from its own copy of the source.

#include "testhelpers.h"

class PreviewLifecycleTest : public PreviewTest
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        createFixture();
    }
    void survivesDocumentClose();
    void survivesDeletionWithoutCloseUrl();
    void reattachesToReopenedDocument();
    void loadsImageBesideTheDocument();
    void runsNoScriptOfTheDocument();
    void staysOnItsPage();
};

// One editor tab closing: Kate calls closeUrl(), which clears the url before anything
// announces the close and empties the buffer straight after, then deletes the document.
void PreviewLifecycleTest::survivesDocumentClose()
{
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));

    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    QVERIFY(doc->closeUrl());
    delete doc;

    // Well past the render debounce and any reload the close could have kicked off.
    QTest::qWait(2000);
    const QString text = pageText(preview.get());
    QVERIFY2(text.contains(SampleBodyLine), qPrintable(QStringLiteral("preview went blank, article reads: '%1'").arg(text)));
}

// Closing several tabs at once takes the other path: no closeUrl, so the document is
// deleted with its url and text still intact and nothing announces it.
void PreviewLifecycleTest::survivesDeletionWithoutCloseUrl()
{
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));

    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    delete doc;

    QTest::qWait(2000);
    const QString text = pageText(preview.get());
    QVERIFY2(text.contains(SampleBodyLine), qPrintable(QStringLiteral("preview went blank, article reads: '%1'").arg(text)));
}

// Reopening the file hands the preview a fresh document and it goes live again.
void PreviewLifecycleTest::reattachesToReopenedDocument()
{
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));

    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));
    const QUrl url = preview->documentUrl();
    QCOMPARE(url, QUrl::fromLocalFile(m_path));

    QVERIFY(doc->closeUrl());
    delete doc;

    QTest::qWait(500);
    // The url survives the document.
    QCOMPARE(preview->documentUrl(), url);

    KTextEditor::Document *reopened = openDocument();
    QTRY_VERIFY(reopened->text().contains(SampleBodyLine));
    preview->attachDocument(reopened, nullptr);

    reopened->setText(QStringLiteral("# live again\n\nthe reattached body line.\n"));
    QVERIFY(waitForPageText(preview.get(), QLatin1String("the reattached body line")));
    delete reopened;
}

// An image referenced by a relative path has to survive LocalFileGuard, which compares the
// request's canonical path against the document folder's canonical path. Those two strings
// are produced on different code paths, and a filesystem that matches names
// case-insensitively or hands out 8.3 short names can make them differ while naming the same
// file. The guard blocks on any mismatch, so the failure is a silently missing image rather
// than an error anyone sees.
void PreviewLifecycleTest::loadsImageBesideTheDocument()
{
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));

    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    doc->setText(QStringLiteral("# katemark\n\n![red](%1)\n").arg(ImageName));
    const int width = waitForImageWidth(preview.get());
    QVERIFY2(width == ImageWidth, qPrintable(QStringLiteral("image beside the document did not load, naturalWidth is %1").arg(width)));

    delete doc;
}

// Raw HTML of a document is shown, but none of its script runs: an event handler could
// otherwise read the files beside the document and send them away.
void PreviewLifecycleTest::runsNoScriptOfTheDocument()
{
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));

    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    doc->setText(QStringLiteral("<img src=\"missing.png\" onerror=\"document.body.dataset.ran = 'yes'\">\n\n"
                                "<svg onload=\"document.body.dataset.ran = 'yes'\"></svg>\n\nthe last line\n"));
    QVERIFY(waitForPageText(preview.get(), QLatin1String("the last line")));
    QTest::qWait(500); // time for the picture to fail and for its handler to run, were it allowed
    QCOMPARE(evalJs(preview.get(), QStringLiteral("document.body.dataset.ran || 'no'")), QStringLiteral("no"));

    delete doc;
}

// Nothing in a document takes the preview to another page. Raw HTML would with
// <meta http-equiv="refresh">, here towards a file of the document's own folder.
void PreviewLifecycleTest::staysOnItsPage()
{
    QFile other(m_dir.filePath(QStringLiteral("other.html")));
    QVERIFY(other.open(QIODevice::WriteOnly));
    QVERIFY(other.write(QByteArrayLiteral("<p>another page</p>")) > 0);
    other.close();

    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));

    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    doc->setText(QStringLiteral("<meta http-equiv=\"refresh\" content=\"0;url=other.html\">\n\nthe last line\n"));
    QVERIFY(waitForPageText(preview.get(), QLatin1String("the last line")));
    QTest::qWait(1500); // well past the refresh
    const QString text = pageText(preview.get());
    QVERIFY2(text.contains(QLatin1String("the last line")), qPrintable(QStringLiteral("the preview left its page, article reads: '%1'").arg(text)));

    delete doc;
}

QTEST_MAIN(PreviewLifecycleTest)

#include "previewlifecycletest.moc"
