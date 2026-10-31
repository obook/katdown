// exporttest.cpp
// The PDF and HTML exports: what the files hold, and what the preview shows meanwhile.
// Author: Olivier Booklage
// Date: October 2026
// License: GPL-3.0-or-later

#include "previewpage.h"
#include "settings.h"
#include "testhelpers.h"

#include <QElapsedTimer>
#include <QImage>
#include <QLabel>
#include <QProcess>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QVersionNumber>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>

class ExportTest : public PreviewTest
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        createFixture();
    }
    void exportsPdfAndHtml();
    void keepsItsLookDuringAnExport();
    void printsLightMarginsFromADarkPreview();
    void runsOneExportAtATime();
    void endsAnExportWhenThePageReloads();
    void numbersThePagesOfAPdf();
    void leavesASinglePageUnnumbered();
    void refusesFilesOnOtherMachines();

private:
    void readPdf(const QString &path, QString *text, QStringList *pageNumbers, QString *total);
};

// The text of a PDF, and the page numbers found at the foot of its pages, "1 / 3": the
// first figure of each, and the total they announce. Skips the test when the PDF cannot be
// read back or when this Chromium does not number pages.
void ExportTest::readPdf(const QString &path, QString *text, QStringList *pageNumbers, QString *total)
{
    const QString pdftotext = QStandardPaths::findExecutable(QStringLiteral("pdftotext"));
    if (pdftotext.isEmpty()) {
        QSKIP("pdftotext is needed to read the PDF back");
    }
    if (QVersionNumber::fromString(QLatin1String(qWebEngineChromiumVersion())).majorVersion() < 131) {
        QSKIP("page numbers need the Chromium of Qt 6.10 or later");
    }
    QProcess reader;
    reader.start(pdftotext, {path, QStringLiteral("-")});
    QVERIFY(reader.waitForFinished(ExportTimeoutMs));
    *text = QString::fromUtf8(reader.readAllStandardOutput());

    // pdftotext drops the spaces around the stroke.
    static const QRegularExpression pageNumber(QStringLiteral("(\\d+) ?/ ?(\\d+)"));
    QRegularExpressionMatchIterator found = pageNumber.globalMatch(*text);
    while (found.hasNext()) {
        const QRegularExpressionMatch number = found.next();
        *pageNumbers += number.captured(1);
        *total = number.captured(2);
    }
}

// Both exports end with exported(); the files have to hold the rendered document, and the
// HTML one must stand alone and be harmless: no script, nothing that sends the reader
// elsewhere, no file from outside the document's folder, none of the preview's scroll-sync
// attributes.
void ExportTest::exportsPdfAndHtml()
{
    // A picture stored outside the document's folder: the preview does not load it. Its
    // folder starts with the name of the document's, "/tmp/x-1" beside "/tmp/x".
    QTemporaryDir elsewhere(m_dir.path() + QStringLiteral("-XXXXXX"));
    QVERIFY(elsewhere.isValid());
    QFile outside(elsewhere.filePath(QStringLiteral("outside.png")));
    QVERIFY(outside.open(QIODevice::WriteOnly));
    QCOMPARE(outside.write(RedPng), RedPng.size());
    outside.close();
    // In the folder: a picture whose name needs escaping, and a file that is not a picture.
    QFile oddName(m_dir.filePath(QStringLiteral("a&b c.png")));
    QVERIFY(oddName.open(QIODevice::WriteOnly));
    QCOMPARE(oddName.write(RedPng), RedPng.size());
    oddName.close();
    QFile secret(m_dir.filePath(QStringLiteral(".env")));
    QVERIFY(secret.open(QIODevice::WriteOnly));
    QVERIFY(secret.write("TOKEN=do-not-export\n") > 0);
    secret.close();

    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));
    doc->setText(QStringLiteral(":::info\nthe boxed line\n:::\n\nand $a^2$\n\n![red](%1)\n\n<img src=\"%2\">\n\n![odd](<a&b c.png>)\n\n<img src=\".env\" width=\"0\">\n\n"
                                "<script>var leaked = 1;</script>\n\n"
                                "<meta http-equiv=\"refresh\" content=\"999;url=https://example.org/\">\n\n<p onclick=\"leak()\">tail</p>\n")
                     .arg(ImageName, QUrl::fromLocalFile(outside.fileName()).toString()));
    QCOMPARE(waitForImageWidth(preview.get()), ImageWidth);
    QVERIFY(waitForPageText(preview.get(), QLatin1String("the boxed line")));

    QSignalSpy exported(preview.get(), &PreviewWidget::exported);
    const QString pdfPath = m_dir.filePath(QStringLiteral("out.pdf"));
    preview->exportPdf(pdfPath);
    QVERIFY(exported.wait(ExportTimeoutMs));
    QCOMPARE(exported.takeFirst().at(1).toBool(), true);
    QFile pdf(pdfPath);
    QVERIFY(pdf.open(QIODevice::ReadOnly));
    QVERIFY(pdf.read(5) == QByteArrayLiteral("%PDF-"));

    const QString htmlPath = m_dir.filePath(QStringLiteral("out.html"));
    preview->exportHtml(htmlPath);
    QVERIFY(exported.wait(ExportTimeoutMs));
    QCOMPARE(exported.takeFirst().at(1).toBool(), true);
    QFile html(htmlPath);
    QVERIFY(html.open(QIODevice::ReadOnly));
    const QString page = QString::fromUtf8(html.readAll());
    QVERIFY(page.contains(QLatin1String("alert alert-info")));
    QVERIFY(page.contains(QLatin1String("the boxed line")));
    QVERIFY(page.contains(QLatin1String("class=\"katex\"")));
    QVERIFY(page.contains(QLatin1String("KaTeX_Main"))); // the math stylesheet came along
    QVERIFY(!page.contains(QLatin1String("<script")));
    QVERIFY(!page.contains(QLatin1String("onclick")));
    QVERIFY(!page.contains(QLatin1String("http-equiv=\"refresh\"")));
    QVERIFY(page.contains(QLatin1String("Content-Security-Policy")));
    // The pictures of the folder travel inside the file: they still show when the file is
    // moved. The one from elsewhere keeps its address and stays where it is. The file that
    // is not a picture leaves neither its content nor its address.
    QCOMPARE(page.count(QLatin1String("src=\"data:image/png;base64,")), 2);
    QVERIFY(page.contains(QLatin1String("outside.png")));
    QVERIFY(!page.contains(QString::fromLatin1(QByteArrayLiteral("TOKEN=do-not-export\n").toBase64())));
    QVERIFY(!page.contains(QLatin1String(".env")));
    QVERIFY(!page.contains(QLatin1String("data-line")));

    delete doc;
}

// "//host/share/a.png" is on another machine under Windows: the guard refuses it without
// looking it up, which would reach that host.
void ExportTest::refusesFilesOnOtherMachines()
{
    LocalFileGuard guard;
    guard.setRoot(m_dir.path());
    QVERIFY(guard.allows(m_dir.filePath(ImageName)));
    // On Linux "//tmp/.../red.png" is the picture of the folder itself: only the rule on
    // network paths turns it down.
    QVERIFY(!guard.allows(QLatin1Char('/') + m_dir.filePath(ImageName)));
    QVERIFY(!guard.allows(QStringLiteral("//evil.example@SSL@443/share/a.png")));
}

// An export redraws the page in GitHub's light look. The reader must not see it: on a
// dark preview it shows as a white flash that lasts as long as the export.
void ExportTest::keepsItsLookDuringAnExport()
{
    Settings *settings = Settings::self();
    const auto restore = qScopeGuard([settings, source = settings->colorSource(), variant = settings->githubVariant()]() {
        settings->setColorSource(source);
        settings->setGithubVariant(variant);
    });
    settings->setColorSource(Settings::GitHub);
    settings->setGithubVariant(Settings::Light);

    const std::unique_ptr<KTextEditor::Document> doc(openDocument());
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc.get());
    preview->resize(600, 400);
    preview->show();
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    // What the reader sees in a corner of the page, where only its background shows.
    auto *web = preview->findChild<QWebEngineView *>();
    auto looksDark = [web]() {
        const QImage seen = web->grab().toImage();
        return seen.pixelColor(10, seen.height() - 10).lightness() < 128;
    };
    // Light first: a picture that came out black for want of a page would pass for dark.
    QTRY_VERIFY_WITH_TIMEOUT(!looksDark(), ReactionTimeoutMs);
    settings->setGithubVariant(Settings::Dark);
    QTRY_VERIFY_WITH_TIMEOUT(looksDark(), ReactionTimeoutMs);

    QSignalSpy exported(preview.get(), &PreviewWidget::exported);
    preview->exportPdf(m_dir.filePath(QStringLiteral("dark.pdf")));
    // The picture that covers the page follows the view when the panel is resized.
    preview->resize(640, 420);
    QTRY_COMPARE(web->findChild<QLabel *>()->size(), web->size());

    // Watched over two exports in a row, and a moment longer while the page takes its look
    // back. The second one starts while the picture of the first is about to be removed.
    QDeadlineTimer deadline(2 * ExportTimeoutMs);
    QElapsedTimer sinceLastExport;
    while (!sinceLastExport.isValid() || sinceLastExport.elapsed() < 500) {
        QVERIFY2(looksDark(), "the preview turned light during an export");
        QVERIFY2(!deadline.hasExpired(), "the exports did not end");
        QTest::qWait(20);
        if (exported.size() == 1 && !preview->isExporting()) {
            preview->exportHtml(m_dir.filePath(QStringLiteral("dark.html")));
        } else if (exported.size() == 2 && !sinceLastExport.isValid()) {
            sinceLastExport.start();
        }
    }
    QCOMPARE(exported.at(0).at(1).toBool(), true);
    QCOMPARE(exported.at(1).at(1).toBool(), true);
    QTRY_VERIFY(!web->findChild<QLabel *>()); // the picture is gone in the end
}

// A PDF is printed in the light look, its margins too: Chromium fills them with the
// background Qt gives the page, not with the document's.
void ExportTest::printsLightMarginsFromADarkPreview()
{
    const QString pdftoppm = QStandardPaths::findExecutable(QStringLiteral("pdftoppm"));
    if (pdftoppm.isEmpty()) {
        QSKIP("pdftoppm is needed to look at the PDF");
    }
    Settings *settings = Settings::self();
    const auto restore = qScopeGuard([settings, source = settings->colorSource(), variant = settings->githubVariant()]() {
        settings->setColorSource(source);
        settings->setGithubVariant(variant);
    });
    settings->setColorSource(Settings::GitHub);
    settings->setGithubVariant(Settings::Dark);

    const std::unique_ptr<KTextEditor::Document> doc(openDocument());
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc.get());
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    QSignalSpy exported(preview.get(), &PreviewWidget::exported);
    const QString path = m_dir.filePath(QStringLiteral("dark-preview.pdf"));
    preview->exportPdf(path);
    QVERIFY(exported.wait(ExportTimeoutMs));
    QCOMPARE(exported.at(0).at(1).toBool(), true);

    // The first page at 20 dpi: its margins are about 12 pixels wide.
    const QString picture = m_dir.filePath(QStringLiteral("dark-preview"));
    QProcess render;
    render.start(pdftoppm, {QStringLiteral("-r"), QStringLiteral("20"), QStringLiteral("-png"), QStringLiteral("-singlefile"), path, picture});
    QVERIFY(render.waitForFinished(ExportTimeoutMs));
    const QImage page(picture + QStringLiteral(".png"));
    QVERIFY(!page.isNull());
    QVERIFY2(page.pixelColor(3, 3).lightness() > 128, "the margins of the PDF are dark");
}

// One export at a time: a second one asked meanwhile fails at once and leaves the first
// alone. On a preview that is already light nothing covers the page.
void ExportTest::runsOneExportAtATime()
{
    Settings *settings = Settings::self();
    const auto restore = qScopeGuard([settings, source = settings->colorSource(), variant = settings->githubVariant()]() {
        settings->setColorSource(source);
        settings->setGithubVariant(variant);
    });
    settings->setColorSource(Settings::GitHub);
    settings->setGithubVariant(Settings::Light);

    const std::unique_ptr<KTextEditor::Document> doc(openDocument());
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc.get());
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    QSignalSpy exported(preview.get(), &PreviewWidget::exported);
    const QString first = m_dir.filePath(QStringLiteral("first.pdf"));
    const QString second = m_dir.filePath(QStringLiteral("second.html"));
    preview->exportPdf(first);
    QVERIFY(preview->isExporting());
    QVERIFY(!preview->findChild<QWebEngineView *>()->findChild<QLabel *>());

    preview->exportHtml(second);
    QCOMPARE(exported.size(), 1);
    QCOMPARE(exported.at(0).at(0).toString(), second);
    QCOMPARE(exported.at(0).at(1).toBool(), false);

    QVERIFY(exported.wait(ExportTimeoutMs));
    QCOMPARE(exported.at(1).at(0).toString(), first);
    QCOMPARE(exported.at(1).at(1).toBool(), true);
    QVERIFY(!preview->isExporting());
    QVERIFY(!QFile::exists(second));
}

// A page that is loaded again cannot finish an export. It ends as failed, and the picture
// that covered the page does not stay on the new one.
void ExportTest::endsAnExportWhenThePageReloads()
{
    Settings *settings = Settings::self();
    const auto restore = qScopeGuard([settings, source = settings->colorSource(), variant = settings->githubVariant(), remote = settings->loadRemoteMedia()]() {
        settings->setColorSource(source);
        settings->setGithubVariant(variant);
        settings->setLoadRemoteMedia(remote);
    });
    settings->setColorSource(Settings::GitHub);
    settings->setGithubVariant(Settings::Dark);

    const std::unique_ptr<KTextEditor::Document> doc(openDocument());
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc.get());
    preview->resize(600, 400);
    preview->show();
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));
    auto *web = preview->findChild<QWebEngineView *>();

    QSignalSpy exported(preview.get(), &PreviewWidget::exported);
    preview->exportPdf(m_dir.filePath(QStringLiteral("reloaded.pdf")));
    QVERIFY(web->findChild<QLabel *>());

    settings->setLoadRemoteMedia(!settings->loadRemoteMedia()); // a setting that reloads the page
    QCOMPARE(exported.size(), 1);
    QCOMPARE(exported.at(0).at(1).toBool(), false);
    QVERIFY(!preview->isExporting());
    QTRY_VERIFY(!web->findChild<QLabel *>());
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));
}

// A PDF numbers its pages at the foot, "1 / 3", and all of it is text one can select:
// pdftotext reads the document and the numbers back.
void ExportTest::numbersThePagesOfAPdf()
{
    const std::unique_ptr<KTextEditor::Document> doc(KTextEditor::Editor::instance()->createDocument(nullptr));
    QString text;
    for (int i = 0; i < 200; ++i) {
        text += QStringLiteral("paragraph %1\n\n").arg(i);
    }
    doc->setText(text);
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc.get());
    QVERIFY(waitForPageText(preview.get(), QLatin1String("paragraph 199")));

    QSignalSpy exported(preview.get(), &PreviewWidget::exported);
    const QString path = m_dir.filePath(QStringLiteral("numbered.pdf"));
    preview->exportPdf(path);
    QVERIFY(exported.wait(ExportTimeoutMs));

    QString read;
    QStringList numbers;
    QString total;
    readPdf(path, &read, &numbers, &total);
    if (QTest::currentTestFailed() || QTest::currentTestResolved()) {
        return;
    }
    QVERIFY(read.contains(QLatin1String("paragraph 199")));
    QVERIFY2(numbers.size() > 1, qPrintable(QStringLiteral("page numbers read: %1").arg(numbers.join(QLatin1Char(' ')))));
    QCOMPARE(numbers.first(), QStringLiteral("1"));
    QCOMPARE(numbers.last(), total);
    QCOMPARE(QString::number(numbers.size()), total);
}

// A document that fits on one page gets no "1 / 1": a number tells nothing there. The
// numbers come back for the next document that needs them.
void ExportTest::leavesASinglePageUnnumbered()
{
    const std::unique_ptr<KTextEditor::Document> doc(openDocument());
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc.get());
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    QSignalSpy exported(preview.get(), &PreviewWidget::exported);
    const QString shortPath = m_dir.filePath(QStringLiteral("one-page.pdf"));
    preview->exportPdf(shortPath);
    QVERIFY(exported.wait(ExportTimeoutMs));
    QCOMPARE(exported.takeFirst().at(1).toBool(), true);

    QString read;
    QStringList numbers;
    QString total;
    readPdf(shortPath, &read, &numbers, &total);
    if (QTest::currentTestFailed() || QTest::currentTestResolved()) {
        return;
    }
    QVERIFY(read.contains(SampleBodyLine));
    QVERIFY2(numbers.isEmpty(), qPrintable(QStringLiteral("a single page is numbered: %1 / %2").arg(numbers.join(QLatin1Char(' ')), total)));

    QString text;
    for (int i = 0; i < 200; ++i) {
        text += QStringLiteral("paragraph %1\n\n").arg(i);
    }
    doc->setText(text);
    QVERIFY(waitForPageText(preview.get(), QLatin1String("paragraph 199")));
    const QString longPath = m_dir.filePath(QStringLiteral("many-pages.pdf"));
    preview->exportPdf(longPath);
    QVERIFY(exported.wait(ExportTimeoutMs));
    readPdf(longPath, &read, &numbers, &total);
    QVERIFY2(numbers.size() > 1, qPrintable(QStringLiteral("page numbers read: %1").arg(numbers.join(QLatin1Char(' ')))));
    QCOMPARE(QString::number(numbers.size()), total);
}

QTEST_MAIN(ExportTest)

#include "exporttest.moc"
