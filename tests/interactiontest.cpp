// What the preview does with the editor and the files around it: scroll sync, PDF and
// HTML export, pasting a picture.

#include "imagepaste.h"
#include "previewutil.h"
#include "settings.h"
#include "testhelpers.h"

#include <QClipboard>
#include <QDir>
#include <QImage>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QToolTip>
#include <QWheelEvent>

#include <KTextEditor/View>

class InteractionTest : public PreviewTest
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        createFixture();
    }
    void followsScrollBothWays();
    void exportsPdfAndHtml();
    void pastesClipboardImage();
    void loadsRemoteImageOnlyWhenAllowed();
    void findsWhereALocalLinkPoints();
    void showsWhereAHoveredLinkGoes();
};

// Scroll sync runs both ways between a real editor view and the page: the block at the top
// of one side has to become the block at the top of the other.
void InteractionTest::followsScrollBothWays()
{
    KTextEditor::Document *doc = KTextEditor::Editor::instance()->createDocument(nullptr);
    QString text;
    for (int i = 0; i < 300; ++i) {
        text += QStringLiteral("- item %1\n").arg(i); // one block per source line
    }
    doc->setText(text);
    KTextEditor::View *view = doc->createView(nullptr);
    view->resize(600, 400);
    view->show();

    auto preview = std::make_unique<PreviewWidget>(nullptr, view, doc);
    preview->resize(600, 400);
    preview->show();
    QVERIFY(waitForPageText(preview.get(), QLatin1String("item 299")));

    // Distance from the top of the page's viewport to the block that starts on a source
    // line; far off the scale when no block carries that line.
    auto blockTop = [&preview](int line) {
        return evalJs(preview.get(),
                      QStringLiteral("(function () { var e = document.querySelector('[data-line=\"%1\"]');"
                                     " return e ? Math.round(e.getBoundingClientRect().top) : 99999; })()")
                          .arg(line))
            .toInt();
    };

    // Scrolled with the wheel, because the view only announces scrolls the user made:
    // setScrollPosition() moves it silently.
    QWidget *editor = view->focusProxy();
    QWheelEvent wheel(QPointF(50, 50), editor->mapToGlobal(QPointF(50, 50)), QPoint(), QPoint(0, -120 * 20), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(editor, &wheel);
    const int first = view->firstDisplayedLine();
    QVERIFY2(first > 20, qPrintable(QStringLiteral("the wheel did not scroll the editor, top line is %1").arg(first)));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(blockTop(first)) <= 2, 10000);

    evalJs(preview.get(), QStringLiteral("document.querySelector('[data-line=\"200\"]').scrollIntoView(); 1"));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(view->firstDisplayedLine() - 200) <= 1, 10000);
    // The editor followed; the page must stay where the reader put it, not be scrolled
    // back by an echo of the editor's move.
    QTest::qWait(500);
    QVERIFY(qAbs(blockTop(200)) <= 2);

    preview.reset();
    delete doc;
}

// Both exports end with exported(); the files have to hold the rendered document, and the
// HTML one must stand alone: no script, none of the preview's scroll-sync attributes.
void InteractionTest::exportsPdfAndHtml()
{
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(Body));
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), Body));
    doc->setText(QStringLiteral(":::info\nthe boxed line\n:::\n\nand $a^2$\n\n![red](%1)\n\n<script>var leaked = 1;</script>\n\n<p onclick=\"leak()\">tail</p>\n").arg(ImageName));
    QCOMPARE(waitForImageWidth(preview.get()), ImageWidth);
    QVERIFY(waitForPageText(preview.get(), QLatin1String("the boxed line")));

    QSignalSpy exported(preview.get(), &PreviewWidget::exported);
    const QString pdfPath = m_dir.filePath(QStringLiteral("out.pdf"));
    preview->exportPdf(pdfPath);
    QVERIFY(exported.wait(30000));
    QCOMPARE(exported.takeFirst().at(1).toBool(), true);
    QFile pdf(pdfPath);
    QVERIFY(pdf.open(QIODevice::ReadOnly));
    QVERIFY(pdf.read(5) == QByteArrayLiteral("%PDF-"));

    const QString htmlPath = m_dir.filePath(QStringLiteral("out.html"));
    preview->exportHtml(htmlPath);
    QVERIFY(exported.wait(30000));
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
    // The picture travels inside the file: it still shows when the file is moved.
    QVERIFY(page.contains(QLatin1String("src=\"data:image/png;base64,")));
    QVERIFY(!page.contains(QLatin1String("data-line")));

    delete doc;
}

// An image-only clipboard becomes a PNG beside the document and a link at the cursor.
void InteractionTest::pastesClipboardImage()
{
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(Body));
    KTextEditor::View *view = doc->createView(nullptr);

    QImage image(4, 4, QImage::Format_RGB32);
    image.fill(Qt::red);
    QGuiApplication::clipboard()->setImage(image);

    QString error;
    view->setCursorPosition(KTextEditor::Cursor(0, 0));
    QVERIFY2(pasteClipboardImage(view, &error), qPrintable(error));
    const QStringList saved = QDir(m_dir.path()).entryList({QStringLiteral("image-*.png")});
    QCOMPARE(saved.size(), 1);
    QVERIFY(doc->line(0).startsWith(QStringLiteral("![](%1)").arg(saved.first())));
    QCOMPARE(QImage(m_dir.filePath(saved.first())).size(), image.size());

    // The link replaces the selected text, as any paste does.
    doc->setText(QStringLiteral("replace me\n"));
    view->setSelection(KTextEditor::Range(0, 0, 0, 10));
    QGuiApplication::clipboard()->setImage(image);
    QVERIFY2(pasteClipboardImage(view, &error), qPrintable(error));
    QVERIFY2(!doc->text().contains(QLatin1String("replace me")), qPrintable(doc->text()));

    // A read-only document takes nothing, and no picture is left behind for it.
    const int before = QDir(m_dir.path()).entryList({QStringLiteral("image-*.png")}).size();
    doc->setReadWrite(false);
    QVERIFY(!pasteClipboardImage(view, &error));
    QCOMPARE(QDir(m_dir.path()).entryList({QStringLiteral("image-*.png")}).size(), before);
    doc->setReadWrite(true);

    // Text on the clipboard is left to Kate's own paste.
    QGuiApplication::clipboard()->setText(QStringLiteral("plain text"));
    QVERIFY(!pasteClipboardImage(view, &error));
    QVERIFY(error.isEmpty());

    delete doc;
}

// A picture on the web stays out until the setting allows it. The "web" here is a server
// on this machine that answers every request with the small red picture.
void InteractionTest::loadsRemoteImageOnlyWhenAllowed()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        QTcpSocket *socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
            socket->readAll();
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nConnection: close\r\nContent-Length: " + QByteArray::number(RedPng.size()) + "\r\n\r\n" + RedPng);
            socket->disconnectFromHost();
        });
    });
    const QString source = QStringLiteral("<div style=\"text-align:center\">\n    <img width=\"20%\" src=\"http://127.0.0.1:%1/red.png\">\n</div>\n\nthe last line\n").arg(server.serverPort());

    Settings::self()->setLoadRemoteMedia(false);
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(Body));
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), Body));
    doc->setText(source);
    QVERIFY(waitForPageText(preview.get(), QLatin1String("the last line")));
    // Kept out, the picture gives way to a note that names the setting and the address.
    QCOMPARE(evalJs(preview.get(), QStringLiteral("document.images.length")), QStringLiteral("0"));
    const QString note = evalJs(preview.get(), QStringLiteral("document.querySelector('.blocked-media').textContent"));
    QVERIFY2(note.contains(QLatin1String("Load media previews from remote URLs")) && note.contains(QLatin1String("/red.png")), qPrintable(note));

    Settings::self()->setLoadRemoteMedia(true);
    QCOMPARE(waitForImageWidth(preview.get()), ImageWidth);

    Settings::self()->setLoadRemoteMedia(false);
    delete doc;
}

// The page resolves every link against the folder of the document, which is wrong for
// "~/notes" and falls short for a wiki link written without ".md".
void InteractionTest::findsWhereALocalLinkPoints()
{
    const QUrl base = QUrl::fromLocalFile(m_dir.path() + QLatin1Char('/'));
    auto target = [&base](const char *link) {
        return localLinkTarget(base.resolved(QUrl(QLatin1String(link))), base);
    };

    QCOMPARE(target("~/Notes/test").toLocalFile(), QDir::homePath() + QLatin1String("/Notes/test"));
    QCOMPARE(target("sample").toLocalFile(), m_path);
    QCOMPARE(target("sample#title").fragment(), QStringLiteral("title"));
    QCOMPARE(target("sample.md").toLocalFile(), m_path);
    QCOMPARE(target("missing").toLocalFile(), m_dir.filePath(QStringLiteral("missing")));

    // A folder really named "~" beside the document keeps its links.
    QVERIFY(QDir(m_dir.path()).mkdir(QStringLiteral("~")));
    QCOMPARE(target("~/").toLocalFile(), m_dir.filePath(QStringLiteral("~/")));
}

// Pointing at a link shows its target, as a browser does.
void InteractionTest::showsWhereAHoveredLinkGoes()
{
    const QUrl base = QUrl::fromLocalFile(m_dir.path() + QLatin1Char('/'));
    QCOMPARE(linkHint(base.resolved(QUrl(QStringLiteral("sample"))), base), m_path);
    QCOMPARE(linkHint(base.resolved(QUrl(QStringLiteral("#title"))), base), QStringLiteral("#title"));
    QCOMPARE(linkHint(QUrl(QStringLiteral("https://kde.org/")), base), QStringLiteral("https://kde.org/"));
    QCOMPARE(linkHint(QUrl(), base), QString());

    // linkHovered is sent by hand: a pointer moved onto the link by the test showed the
    // tip on every run offscreen, but not on every run with a display.
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(Body));
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), Body));
    Q_EMIT preview->findChild<QWebEngineView *>()->page()->linkHovered(QStringLiteral("https://kde.org/"));
    QCOMPARE(QToolTip::text(), QStringLiteral("https://kde.org/"));
    QToolTip::hideText();
    preview.reset();
    delete doc;
}

QTEST_MAIN(InteractionTest)

#include "interactiontest.moc"
