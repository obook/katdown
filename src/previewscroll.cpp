// previewscroll.cpp
// PreviewWidget: keeping the editor and the page scrolled to the same place.
// Author: Olivier Booklage
// Date: October 2026
// License: GPL-3.0-or-later

#include "previewwidget.h"

#include <QPointer>
#include <QWebEnginePage>
#include <QWebEngineView>

#include <KTextEditor/Document>
#include <KTextEditor/View>
#include <ktexteditor_version.h>

void PreviewWidget::setView(KTextEditor::View *view)
{
    if (m_view == view) {
        return;
    }
    if (m_view) {
        m_view->disconnect(this);
    }
    m_view = view;
    if (view) {
        connect(view, &KTextEditor::View::verticalScrollPositionChanged, this, &PreviewWidget::syncFromEditor);
        // The editor's color theme can change while the application's palette stays.
        connect(view, &KTextEditor::View::configChanged, this, &PreviewWidget::applyTheme);
        syncFromEditor();
    }
}

// Bring the page to the source line at the top of the editor.
void PreviewWidget::syncFromEditor()
{
    if (!m_view || !m_doc || !m_loaded || m_paused) {
        return;
    }
    const int first = m_view->firstDisplayedLine();
    const bool atEnd = m_view->lastDisplayedLine() >= m_doc->lines() - 1;
    if (first <= 0 && atEnd) {
        return; // the whole document fits in the editor, there is nothing to follow
    }
    // With dynamic word wrap one source line spans several rows; the column of the first
    // visible character places the page inside that line's block. Without word wrap that
    // column only tells how far the view is scrolled sideways.
    qreal line = first;
    const bool wraps = m_view->configValue(QStringLiteral("dynamic-word-wrap")).toBool();
    const KTextEditor::Cursor top = m_view->coordinatesToCursor(m_view->textAreaRect().topLeft());
    if (wraps && top.isValid() && top.line() == first && m_doc->lineLength(first) > 0) {
        line += qreal(top.column()) / m_doc->lineLength(first);
    }
    runJs(QStringLiteral("window.__scrollToLine(%1, %2);").arg(line, 0, 'f', 3).arg(atEnd ? QLatin1String("true") : QLatin1String("false")));
}

// Bring the editor to the source line at the top of the page.
void PreviewWidget::syncFromPreview()
{
    if (!m_view || !m_loaded || m_paused) {
        return;
    }
    // The page answers null when the scroll is the one the editor just asked for, so the
    // editor never follows its own echo.
    m_web->page()->runJavaScript(QStringLiteral("window.__topLine()"), [self = QPointer<PreviewWidget>(this)](const QVariant &result) {
        bool ok = false;
        const double line = result.toDouble(&ok);
        if (!ok || !self || !self->m_view) {
            return;
        }
        KTextEditor::View *view = self->m_view;
        if (line < 0) { // -1: the page is at its end (see __topLine in scroll.js)
            view->setScrollPosition(view->maxScrollPosition());
            return;
        }
        int target = qRound(line);
#if KTEXTEDITOR_VERSION >= QT_VERSION_CHECK(6, 27, 0)
        // The view scrolls in visible lines: below a folded region, a source line sits
        // higher up than its number says.
        target = view->realToVisibleLine(target);
#endif
        view->setScrollPosition(KTextEditor::Cursor(target, 0));
    });
}
