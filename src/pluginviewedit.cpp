// pluginviewedit.cpp
// PluginView: the Markdown editing actions of the Tools menu, pasting a picture, and the
// messages shown over the editor.
// Author: Olivier Booklage
// Date: October 2026
// License: GPL-3.0-or-later

#include "imagepaste.h"
#include "markdownedit.h"
#include "pluginview.h"

#include <QAction>
#include <QDesktopServices>
#include <QIcon>
#include <QKeySequence>
#include <QUrl>

#include <KActionCollection>
#include <KLocalizedString>
#include <KStandardGuiItem>
#include <KTextEditor/Document>
#include <KTextEditor/MainWindow>
#include <KTextEditor/Message>
#include <KTextEditor/View>

namespace
{
// How long a message stays over the editor before it hides itself.
constexpr int MessageTimeoutMs = 5000;
} // namespace

// Show a short message at the top of the active view, the way Kate reports its own events.
// With fileToOpen, the message carries a button that opens that file and one that closes
// the message.
void PluginView::showMessage(const QString &text, bool isError, const QString &fileToOpen)
{
    KTextEditor::View *view = m_mainWindow->activeView();
    if (!view) {
        return;
    }
    auto *message = new KTextEditor::Message(text, isError ? KTextEditor::Message::Error : KTextEditor::Message::Positive);
    if (!fileToOpen.isEmpty()) {
        // A button: Kate does nothing when a link in the text is clicked. KDE's own "Open"
        // is already translated; its "..." would announce a dialog, and none comes.
        const KGuiItem open = KStandardGuiItem::open();
        const QString label = open.text().remove(QLatin1String("...")).remove(QChar(0x2026));
        auto *action = new QAction(open.icon(), label, message);
        connect(action, &QAction::triggered, message, [fileToOpen]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(fileToOpen));
        });
        message->addAction(action);
        // A message with an action loses Kate's close button: give it back as a button.
        const KGuiItem close = KStandardGuiItem::close();
        message->addAction(new QAction(close.icon(), close.text(), message));
    }
    message->setView(view);
    message->setAutoHide(MessageTimeoutMs);
    view->document()->postMessage(message);
}

// Runs after Kate's own paste, which does nothing with a picture (see onViewChanged).
void PluginView::pasteImage()
{
    KTextEditor::View *view = markdownView();
    if (!view) {
        return;
    }
    QString error;
    if (!pasteClipboardImage(view, &error) && !error.isEmpty()) {
        showMessage(error, true);
    }
}

// The active view when it shows a Markdown document, nullptr otherwise.
KTextEditor::View *PluginView::markdownView() const
{
    KTextEditor::View *view = m_mainWindow->activeView();
    if (!view || !isMarkdown(view->document())) {
        return nullptr;
    }
    return view;
}

// Register one editing action. It does nothing unless a Markdown document is active.
void PluginView::addEditAction(const QString &name, const QString &text, const QString &iconName, const QKeySequence &shortcut, const EditFunction &edit)
{
    QAction *action = addAction(name, text, QIcon::fromTheme(iconName));
    bool takesAltGr = false;
#ifdef Q_OS_WIN
    // Windows sends AltGr as Ctrl+Alt: such a shortcut would take the euro sign, or the "|"
    // a table needs. The action stays in the menu and the user can give it a key.
    const Qt::KeyboardModifiers altGr = Qt::ControlModifier | Qt::AltModifier;
    takesAltGr = (shortcut[0].keyboardModifiers() & altGr) == altGr;
#endif
    if (!takesAltGr) {
        actionCollection()->setDefaultShortcut(action, shortcut);
    }
    connect(action, &QAction::triggered, this, [this, edit]() {
        KTextEditor::View *view = markdownView();
        if (view) {
            edit(view);
        }
    });
}

// The shortcuts are ones that Kate and its bundled plugins leave free. Ctrl+B and Ctrl+I,
// usual in other Markdown editors, are Kate's bookmark and indentation shortcuts.
void PluginView::addEditActions()
{
    addEditAction(QStringLiteral("katemark_bold"),
                  i18n("Bold"),
                  QStringLiteral("format-text-bold"),
                  QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_B),
                  [](KTextEditor::View *view) {
                      toggleInlineMarker(view, QStringLiteral("**"));
                  });
    addEditAction(QStringLiteral("katemark_italic"),
                  i18n("Italic"),
                  QStringLiteral("format-text-italic"),
                  QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_E),
                  [](KTextEditor::View *view) {
                      toggleInlineMarker(view, QStringLiteral("*"));
                  });
    addEditAction(QStringLiteral("katemark_strikethrough"),
                  i18n("Strikethrough"),
                  QStringLiteral("format-text-strikethrough"),
                  QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S),
                  [](KTextEditor::View *view) {
                      toggleInlineMarker(view, QStringLiteral("~~"));
                  });
    addEditAction(QStringLiteral("katemark_heading_up"),
                  i18n("Increase Heading Level"),
                  QStringLiteral("format-indent-more"),
                  QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Equal),
                  [](KTextEditor::View *view) {
                      shiftHeadingLevel(view, 1);
                  });
    addEditAction(QStringLiteral("katemark_heading_down"),
                  i18n("Decrease Heading Level"),
                  QStringLiteral("format-indent-less"),
                  QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Minus),
                  [](KTextEditor::View *view) {
                      shiftHeadingLevel(view, -1);
                  });
    addEditAction(QStringLiteral("katemark_paste_link"),
                  i18n("Paste as Link"),
                  QStringLiteral("insert-link"),
                  QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V),
                  [](KTextEditor::View *view) {
                      pasteClipboardLink(view);
                  });
    addEditAction(QStringLiteral("katemark_format_table"),
                  i18n("Format Table"),
                  QStringLiteral("table"),
                  QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F),
                  [](KTextEditor::View *view) {
                      formatTableAtCursor(view);
                  });
}
