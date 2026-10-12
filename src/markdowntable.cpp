// markdowntable.cpp
// Editing helper for Markdown documents: aligning the columns of a table.
// Author: Olivier Booklage
// Date: October 2026
// License: GPL-3.0-or-later

#include "markdownedit.h"

#include <QList>
#include <QRegularExpression>
#include <QStringList>

#include <KTextEditor/Document>
#include <KTextEditor/View>

namespace
{
// A table column is never narrower than its delimiter, "---".
constexpr int MinColumnWidth = 3;

// The delimiter row is the second row of a table, right under its header.
constexpr int DelimiterRow = 1;

// The cells of one table row, without the outer pipes. A pipe written "\|" stays in its
// cell: (?<!\\) means "not preceded by a backslash".
QStringList splitRow(const QString &line)
{
    static const QRegularExpression separator(QStringLiteral("(?<!\\\\)\\|"));
    QString row = line.trimmed();
    if (row.startsWith(QLatin1Char('|'))) {
        row.remove(0, 1);
    }
    if (row.endsWith(QLatin1Char('|')) && !row.endsWith(QLatin1String("\\|"))) {
        row.chop(1);
    }
    QStringList cells = row.split(separator);
    for (QString &cell : cells) {
        cell = cell.trimmed();
    }
    return cells;
}

// The second row of a table: only dashes, with an optional colon on either side.
bool isDelimiterRow(const QStringList &cells)
{
    static const QRegularExpression delimiter(QStringLiteral("^:?-+:?$"));
    for (const QString &cell : cells) {
        if (!delimiter.match(cell).hasMatch()) {
            return false;
        }
    }
    return !cells.isEmpty();
}

// One cell brought to the width of its column. The delimiter cell of the column tells
// the alignment: ":--" left, "--:" right, ":-:" centered.
QString padCell(const QString &cell, int width, const QString &delimiter)
{
    const bool left = delimiter.startsWith(QLatin1Char(':'));
    const bool right = delimiter.endsWith(QLatin1Char(':'));
    if (left && right) {
        const int before = (width - cell.size()) / 2;
        return QString(before, QLatin1Char(' ')) + cell.leftJustified(width - before);
    }
    if (right) {
        return cell.rightJustified(width);
    }
    return cell.leftJustified(width);
}

// The delimiter cell of a column, rebuilt at the column's width with its colons.
QString padDelimiter(int width, const QString &delimiter)
{
    const bool left = delimiter.startsWith(QLatin1Char(':'));
    const bool right = delimiter.endsWith(QLatin1Char(':'));
    QString dashes(width - (left ? 1 : 0) - (right ? 1 : 0), QLatin1Char('-'));
    if (left) {
        dashes.prepend(QLatin1Char(':'));
    }
    if (right) {
        dashes.append(QLatin1Char(':'));
    }
    return dashes;
}

// The lines of the table, each cell padded to the width of its column.
// ponytail: widths are counted in characters, so a column holding wide characters
// (Chinese, Japanese, emoji) comes out ragged. Count display cells if that matters.
QStringList alignedRows(QList<QStringList> rows, const QString &indent)
{
    int columnCount = 0;
    for (const QStringList &rowCells : rows) {
        columnCount = qMax(columnCount, int(rowCells.size()));
    }
    QList<int> widths(columnCount, MinColumnWidth);
    for (int row = 0; row < rows.size(); ++row) {
        rows[row].resize(columnCount); // a short row gets empty cells
        if (row == DelimiterRow) {
            continue; // the delimiter row adapts to the others
        }
        for (int column = 0; column < columnCount; ++column) {
            widths[column] = qMax(widths[column], int(rows[row][column].size()));
        }
    }
    const QStringList delimiters = rows[DelimiterRow];
    QStringList lines;
    for (int row = 0; row < rows.size(); ++row) {
        QStringList cells;
        for (int column = 0; column < columnCount; ++column) {
            const QString delimiter = delimiters[column].isEmpty() ? QStringLiteral("---") : delimiters[column];
            cells << (row == DelimiterRow ? padDelimiter(widths[column], delimiter) : padCell(rows[row][column], widths[column], delimiter));
        }
        lines << indent + QStringLiteral("| ") + cells.join(QStringLiteral(" | ")) + QStringLiteral(" |");
    }
    return lines;
}
} // namespace

bool formatTableAtCursor(KTextEditor::View *view)
{
    KTextEditor::Document *doc = view->document();
    auto isRow = [doc](int line) {
        return line >= 0 && line < doc->lines() && doc->line(line).contains(QLatin1Char('|'));
    };
    const KTextEditor::Cursor cursor = view->cursorPosition();
    if (!isRow(cursor.line())) {
        return false;
    }
    // The table is the run of lines holding a pipe around the cursor.
    int first = cursor.line();
    while (isRow(first - 1)) {
        --first;
    }
    int last = cursor.line();
    while (isRow(last + 1)) {
        ++last;
    }
    QList<QStringList> rows;
    for (int line = first; line <= last; ++line) {
        rows << splitRow(doc->line(line));
    }
    if (rows.size() <= DelimiterRow || !isDelimiterRow(rows[DelimiterRow])) {
        return false;
    }
    // A table inside a list item is indented: keep the indentation of its first row.
    const QString firstLine = doc->line(first);
    int indentWidth = 0;
    while (indentWidth < firstLine.size() && firstLine.at(indentWidth).isSpace()) {
        ++indentWidth;
    }
    const QString indent = firstLine.left(indentWidth);
    const QStringList lines = alignedRows(rows, indent);
    doc->replaceText(KTextEditor::Range(first, 0, last, doc->lineLength(last)), lines.join(QLatin1Char('\n')));
    view->setCursorPosition(KTextEditor::Cursor(cursor.line(), qMin(cursor.column(), doc->lineLength(cursor.line()))));
    return true;
}
