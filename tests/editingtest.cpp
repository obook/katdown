// editingtest.cpp
// The editing helpers work on the text of a document: no preview is involved, only an
// editor view.
// Author: Olivier Booklage
// Date: October 2026
// License: GPL-3.0-or-later

#include "markdownedit.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QTest>

#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

class EditingTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();
    void togglesInlineMarkers();
    void shiftsHeadingLevels();
    void pastesLinkOnSelection();
    void formatsTable();

private:
    KTextEditor::Document *m_doc = nullptr;
    KTextEditor::View *m_view = nullptr;
};

// A fresh document and view for every test.
void EditingTest::init()
{
    m_doc = KTextEditor::Editor::instance()->createDocument(nullptr);
    m_view = m_doc->createView(nullptr);
}

void EditingTest::cleanup()
{
    delete m_doc; // takes its view along
}

void EditingTest::togglesInlineMarkers()
{
    m_doc->setText(QStringLiteral("some word here"));
    m_view->setSelection(KTextEditor::Range(0, 5, 0, 9));
    toggleInlineMarker(m_view, QStringLiteral("**"));
    QCOMPARE(m_doc->text(), QStringLiteral("some **word** here"));
    // The word stays selected, so the same shortcut takes the marker off again.
    QCOMPARE(m_view->selectionText(), QStringLiteral("word"));

    // Italic on a bold word adds a third star instead of eating one of the two.
    toggleInlineMarker(m_view, QStringLiteral("*"));
    QCOMPARE(m_doc->text(), QStringLiteral("some ***word*** here"));
    toggleInlineMarker(m_view, QStringLiteral("*"));
    QCOMPARE(m_doc->text(), QStringLiteral("some **word** here"));
    toggleInlineMarker(m_view, QStringLiteral("**"));
    QCOMPARE(m_doc->text(), QStringLiteral("some word here"));

    // Without a selection, the word under the cursor is taken.
    m_view->removeSelection();
    m_view->setCursorPosition(KTextEditor::Cursor(0, 12));
    toggleInlineMarker(m_view, QStringLiteral("~~"));
    QCOMPARE(m_doc->text(), QStringLiteral("some word ~~here~~"));

    // On an empty line, the pair is written and the cursor waits between the two.
    m_doc->setText(QString());
    m_view->setCursorPosition(KTextEditor::Cursor(0, 0));
    toggleInlineMarker(m_view, QStringLiteral("**"));
    QCOMPARE(m_doc->text(), QStringLiteral("****"));
    QCOMPARE(m_view->cursorPosition(), KTextEditor::Cursor(0, 2));
}

void EditingTest::shiftsHeadingLevels()
{
    m_doc->setText(QStringLiteral("Title\n## Sub\n\nplain"));
    m_view->setSelection(KTextEditor::Range(0, 0, 1, 3));
    shiftHeadingLevel(m_view, 1);
    QCOMPARE(m_doc->text(), QStringLiteral("# Title\n### Sub\n\nplain"));

    m_view->removeSelection();
    m_view->setCursorPosition(KTextEditor::Cursor(0, 0));
    shiftHeadingLevel(m_view, -1);
    QCOMPARE(m_doc->line(0), QStringLiteral("Title"));
    shiftHeadingLevel(m_view, -1); // nothing left to remove
    QCOMPARE(m_doc->line(0), QStringLiteral("Title"));

    m_doc->setText(QStringLiteral("###### Six"));
    m_view->setCursorPosition(KTextEditor::Cursor(0, 0));
    shiftHeadingLevel(m_view, 1); // Markdown stops at six levels
    QCOMPARE(m_doc->text(), QStringLiteral("###### Six"));

    // Two whole lines selected end at the start of the third, which is not selected.
    m_doc->setText(QStringLiteral("one\ntwo\nthree"));
    m_view->setSelection(KTextEditor::Range(0, 0, 2, 0));
    shiftHeadingLevel(m_view, 1);
    QCOMPARE(m_doc->text(), QStringLiteral("# one\n# two\nthree"));
}

void EditingTest::pastesLinkOnSelection()
{
    m_doc->setText(QStringLiteral("see docs now"));
    m_view->setSelection(KTextEditor::Range(0, 4, 0, 8));
    QGuiApplication::clipboard()->setText(QStringLiteral("https://example.com/a"));
    QVERIFY(pasteClipboardLink(m_view));
    QCOMPARE(m_doc->text(), QStringLiteral("see [docs](https://example.com/a) now"));

    // Anything that is not an address is left to the ordinary paste.
    QGuiApplication::clipboard()->setText(QStringLiteral("not a link"));
    QVERIFY(!pasteClipboardLink(m_view));
    QCOMPARE(m_doc->text(), QStringLiteral("see [docs](https://example.com/a) now"));
}

void EditingTest::formatsTable()
{
    m_doc->setText(QStringLiteral("before\n\n| a | long header |\n|:-:|--:|\n| x | 1 |\n| yyyy | 22 |\n\nafter"));
    m_view->setCursorPosition(KTextEditor::Cursor(4, 2));
    QVERIFY(formatTableAtCursor(m_view));

    QCOMPARE(m_doc->line(0), QStringLiteral("before"));
    QCOMPARE(m_doc->line(7), QStringLiteral("after"));
    // The delimiter row keeps each column's alignment, at the column's width.
    QCOMPARE(m_doc->line(3), QStringLiteral("| :--: | ----------: |"));
    // Every row has its pipes at the same places.
    for (int line = 2; line <= 5; ++line) {
        const QString row = m_doc->line(line);
        QCOMPARE(row.size(), m_doc->line(3).size());
        QCOMPARE(row.indexOf(QLatin1Char('|'), 1), m_doc->line(3).indexOf(QLatin1Char('|'), 1));
    }
    // Right-aligned column: the numbers end against the closing pipe.
    QVERIFY(m_doc->line(4).endsWith(QLatin1String(" 1 |")));
    QVERIFY(m_doc->line(5).endsWith(QLatin1String(" 22 |")));
    QVERIFY(m_doc->line(5).startsWith(QLatin1String("| yyyy |")));

    // Outside a table there is nothing to format.
    m_view->setCursorPosition(KTextEditor::Cursor(0, 0));
    QVERIFY(!formatTableAtCursor(m_view));
}

QTEST_MAIN(EditingTest)

#include "editingtest.moc"
