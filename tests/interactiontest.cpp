// interactiontest.cpp
// What the preview does with the editor and the files around it: scroll sync, pasting a
// picture, pictures from the web, links. The exports are in exporttest.cpp.
// Author: Olivier Booklage
// Date: October 2026
// License: GPL-3.0-or-later

#include "imagepaste.h"
#include "previewutil.h"
#include "settings.h"
#include "testhelpers.h"

#include <QClipboard>
#include <QDir>
#include <QImage>
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
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(blockTop(first)) <= 2, ReactionTimeoutMs);

    evalJs(preview.get(), QStringLiteral("document.querySelector('[data-line=\"200\"]').scrollIntoView(); 1"));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(view->firstDisplayedLine() - 200) <= 1, ReactionTimeoutMs);
    // The editor followed; the page must stay where the reader put it, not be scrolled
    // back by an echo of the editor's move.
    QTest::qWait(500);
    QVERIFY(qAbs(blockTop(200)) <= 2);

    preview.reset();
    delete doc;
}

// An image-only clipboard becomes a PNG beside the document and a link at the cursor.
void InteractionTest::pastesClipboardImage()
{
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));
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
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));
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
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));
    Q_EMIT preview->findChild<QWebEngineView *>()->page()->linkHovered(QStringLiteral("https://kde.org/"));
    QCOMPARE(QToolTip::text(), QStringLiteral("https://kde.org/"));
    QToolTip::hideText();
    preview.reset();
    delete doc;
}

QTEST_MAIN(InteractionTest)

#include "interactiontest.moc"
