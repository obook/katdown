// testhelpers.h
// What the test programs share: a document in a temporary folder, and ways to look
// into the rendered page.
// Code by uwuclxdy and Olivier Booklage.
// License: GPL-3.0-or-later

#pragma once

#include "previewwidget.h"

#include <QDeadlineTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QWebEnginePage>
#include <QWebEngineView>

#include <memory>

#include <KTextEditor/Document>
#include <KLocalizedString>
#include <KTextEditor/Editor>

// How long the tests wait, in milliseconds.
constexpr int JsAnswerTimeoutMs = 5000; // for one expression evaluated in the page
constexpr int ReactionTimeoutMs = 10000; // for the page or the editor to follow a change
constexpr int PageTimeoutMs = 20000; // for the page to render a document or load a picture
constexpr int ExportTimeoutMs = 30000; // for an export, or for reading one back

// The line of sample.md that the tests look for in the page.
const QLatin1String SampleBodyLine("the cached body line");
const QLatin1String ImageName("red.png");
constexpr int ImageWidth = 2;
// A 2x1 red PNG, inline so the test carries no fixture file. Two pixels wide because a
// failed load reports naturalWidth 0 and a placeholder reports 1.
const QByteArray RedPng =
    QByteArray::fromHex("89504e470d0a1a0a0000000d494844520000000200000001080200000"
                        "07b40e8dd0000000d4944415478da63f8cfc000440008fe01ff19c06be"
                        "70000000049454e44ae426082");

// What the page answers to evalJs(), and whether the answer has come yet.
struct JsAnswer {
    QString value;
    bool arrived = false;
};

// Evaluate one expression in the page and hand back its value as a string.
inline QString evalJs(PreviewWidget *preview, const QString &code)
{
    auto *view = preview->findChild<QWebEngineView *>();
    if (!view) {
        return QString();
    }
    // The callback outlives this call if the deadline expires first, so the answer
    // cannot live on the stack.
    auto answer = std::make_shared<JsAnswer>();
    view->page()->runJavaScript(code, [answer](const QVariant &result) {
        answer->value = result.toString();
        answer->arrived = true;
    });
    QDeadlineTimer deadline(JsAnswerTimeoutMs);
    while (!answer->arrived && !deadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return answer->value;
}

// Number of elements of the page that match a CSS selector.
inline int countElements(PreviewWidget *preview, const char *selector)
{
    return evalJs(preview, QStringLiteral("document.querySelectorAll('%1').length").arg(QLatin1String(selector))).toInt();
}

// Read the rendered article back out of the page.
inline QString pageText(PreviewWidget *preview)
{
    return evalJs(preview, QStringLiteral("document.getElementById('content').innerText"));
}

// naturalWidth stays 0 for an image the page never fetched, which is what separates a
// blocked request from a rendered <img> tag.
inline int waitForImageWidth(PreviewWidget *preview)
{
    QDeadlineTimer deadline(PageTimeoutMs);
    while (!deadline.hasExpired()) {
        const int width = evalJs(preview, QStringLiteral("document.images.length ? document.images[0].naturalWidth : 0")).toInt();
        if (width > 0) {
            return width;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return 0;
}

inline bool waitForPageText(PreviewWidget *preview, QLatin1String needle)
{
    QDeadlineTimer deadline(PageTimeoutMs);
    while (!deadline.hasExpired()) {
        if (pageText(preview).contains(needle)) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return false;
}

// Base of the test classes: a temporary folder holding sample.md and a small picture.
class PreviewTest : public QObject
{
protected:
    // To call from initTestCase().
    void createFixture()
    {
        // The tests compare titles written in English, whatever the session's language.
        KLocalizedString::setLanguages({QStringLiteral("en")});
        QVERIFY(m_dir.isValid());
        m_path = m_dir.filePath(QStringLiteral("sample.md"));
        QFile f(m_path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write(QStringLiteral("# katemark\n\n%1.\n").arg(SampleBodyLine).toUtf8()) > 0);
        f.close();

        QFile png(m_dir.filePath(ImageName));
        QVERIFY(png.open(QIODevice::WriteOnly));
        QCOMPARE(png.write(RedPng), RedPng.size());
        png.close();
    }

    KTextEditor::Document *openDocument()
    {
        KTextEditor::Document *doc = KTextEditor::Editor::instance()->createDocument(nullptr);
        doc->openUrl(QUrl::fromLocalFile(m_path));
        return doc;
    }

    QTemporaryDir m_dir;
    QString m_path;
};
