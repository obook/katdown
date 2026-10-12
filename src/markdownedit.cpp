// markdownedit.cpp
// Editing helpers for Markdown documents: inline markers, heading levels, links.
// The table helper is in markdowntable.cpp.
// Author: Olivier Booklage
// Date: October 2026
// License: GPL-3.0-or-later

#include "markdownedit.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>

#include <KTextEditor/Document>
#include <KTextEditor/View>

namespace
{
// A Markdown heading has at most six levels.
constexpr int MaxHeadingLevel = 6;

// How many times the character is repeated just before the column.
int repeatCountBefore(const QString &line, int column, QChar character)
{
    int count = 0;
    while (column - count - 1 >= 0 && line.at(column - count - 1) == character) {
        ++count;
    }
    return count;
}

// How many times the character is repeated from the column on.
int repeatCountAfter(const QString &line, int column, QChar character)
{
    int count = 0;
    while (column + count < line.size() && line.at(column + count) == character) {
        ++count;
    }
    return count;
}

// Whether that many marker characters in a row hold the marker. "*" (italic) is there when
// the count is odd: one star, or three with bold. A two-character marker needs two of them.
bool repeatsHoldMarker(int repeatCount, const QString &marker)
{
    if (marker.size() == 1) {
        return repeatCount % 2 == 1;
    }
    return repeatCount >= marker.size();
}

} // namespace

void toggleInlineMarker(KTextEditor::View *view, const QString &marker)
{
    KTextEditor::Document *doc = view->document();
    KTextEditor::Range range = view->selectionRange();
    if (!view->selection()) {
        range = doc->wordRangeAt(view->cursorPosition());
    }
    if (!range.isValid()) {
        range = KTextEditor::Range(view->cursorPosition(), view->cursorPosition());
    }
    if (!range.onSingleLine()) {
        return; // an inline marker does not run over several lines
    }
    const int line = range.start().line();
    const int start = range.start().column();
    const int end = range.end().column();
    const int size = marker.size();
    const QChar markerChar = marker.at(0);
    const QString lineText = doc->line(line);
    const QString text = doc->text(range);

    // All the changes below count as one for undo.
    KTextEditor::Document::EditingTransaction transaction(doc);
    const bool markedOutside = repeatsHoldMarker(repeatCountBefore(lineText, start, markerChar), marker)
        && repeatsHoldMarker(repeatCountAfter(lineText, end, markerChar), marker);
    const bool markedInside = text.size() > 2 * size && repeatsHoldMarker(repeatCountAfter(text, 0, markerChar), marker)
        && repeatsHoldMarker(repeatCountBefore(text, text.size(), markerChar), marker);
    if (markedOutside) {
        // The markers sit around the range: take them off and keep the text selected.
        doc->replaceText(KTextEditor::Range(line, start - size, line, end + size), text);
        view->setSelection(KTextEditor::Range(line, start - size, line, end - size));
    } else if (markedInside) {
        // The markers were selected with the text.
        doc->replaceText(range, text.mid(size, text.size() - 2 * size));
        view->setSelection(KTextEditor::Range(line, start, line, end - 2 * size));
    } else {
        doc->replaceText(range, marker + text + marker);
        if (text.isEmpty()) {
            view->setCursorPosition(KTextEditor::Cursor(line, start + size));
        } else {
            view->setSelection(KTextEditor::Range(line, start + size, line, end + size));
        }
    }
}

void shiftHeadingLevel(KTextEditor::View *view, int delta)
{
    static const QRegularExpression heading(QStringLiteral("^(#{1,%1})[ \\t]+").arg(MaxHeadingLevel));
    KTextEditor::Document *doc = view->document();
    int first = view->cursorPosition().line();
    int last = first;
    if (view->selection()) {
        const KTextEditor::Range selection = view->selectionRange();
        first = selection.start().line();
        last = selection.end().line();
        // Whole lines selected end at column 0 of the next one, which is not selected.
        if (selection.end().column() == 0 && last > first) {
            --last;
        }
    }

    KTextEditor::Document::EditingTransaction transaction(doc);
    for (int line = first; line <= last; ++line) {
        const QString text = doc->line(line);
        if (text.trimmed().isEmpty()) {
            continue;
        }
        const QRegularExpressionMatch match = heading.match(text);
        const int level = match.hasMatch() ? match.capturedLength(1) : 0;
        QString changed = text;
        if (delta > 0 && level == 0) {
            changed = QStringLiteral("# ") + text;
        } else if (delta > 0 && level < MaxHeadingLevel) {
            changed = QLatin1Char('#') + text;
        } else if (delta < 0 && level == 1) {
            changed = text.mid(match.capturedLength()); // back to plain text
        } else if (delta < 0 && level > 1) {
            changed = text.mid(1);
        }
        if (changed != text) {
            doc->replaceText(KTextEditor::Range(line, 0, line, text.size()), changed);
        }
    }
}

bool pasteClipboardLink(KTextEditor::View *view)
{
    static const QStringList schemes{QStringLiteral("http"), QStringLiteral("https"), QStringLiteral("ftp"), QStringLiteral("mailto"), QStringLiteral("file")};
    static const QRegularExpression blank(QStringLiteral("\\s"));
    const QString address = QGuiApplication::clipboard()->text().trimmed();
    const QUrl url(address, QUrl::StrictMode);
    if (address.contains(blank) || !url.isValid() || !schemes.contains(url.scheme())) {
        return false;
    }
    KTextEditor::Document *doc = view->document();
    if (view->selection()) {
        const QString text = view->selectionText();
        doc->replaceText(view->selectionRange(), QStringLiteral("[%1](%2)").arg(text, address));
        return true;
    }
    // Nothing selected: leave the cursor between the brackets, ready for the text.
    const KTextEditor::Cursor start = view->cursorPosition();
    view->insertText(QStringLiteral("[](%1)").arg(address));
    view->setCursorPosition(KTextEditor::Cursor(start.line(), start.column() + 1));
    return true;
}
