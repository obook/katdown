// previewinput.cpp
// PreviewWidget: handing back to Kate the keys and mouse buttons the page does not use,
// and the context menu of the page.
// Code by uwuclxdy, moved out of previewwidget.cpp.
// License: GPL-3.0-or-later

#include "previewwidget.h"

#include <QAction>
#include <QClipboard>
#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QWebEngineContextMenuRequest>
#include <QWebEnginePage>
#include <QWebEngineView>

#include <KActionCollection>
#include <KTextEditor/MainWindow>
#include <KXMLGUIClient>
#include <KXMLGUIFactory>

// QWebEngineView delivers input to a lazily-created internal child (its focus proxy),
// which can be swapped out across loads. Keep our filter attached to whatever child
// currently receives key/mouse events.
void PreviewWidget::installInputFilter()
{
    QWidget *proxy = m_web ? m_web->focusProxy() : nullptr;
    if (proxy == m_inputTarget) {
        return;
    }
    if (m_inputTarget) {
        m_inputTarget->removeEventFilter(this);
    }
    m_inputTarget = proxy;
    if (proxy) {
        proxy->installEventFilter(this);
    }
}

bool PreviewWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_web) {
        if (event->type() == QEvent::ChildAdded || event->type() == QEvent::ChildPolished) {
            installInputFilter();
        } else if (event->type() == QEvent::Resize && m_exportCover) {
            m_exportCover->resize(static_cast<QResizeEvent *>(event)->size());
        }
        return QWidget::eventFilter(obj, event);
    }

    switch (event->type()) {
    case QEvent::KeyPress:
        if (forwardKeyEvent(static_cast<QKeyEvent *>(event))) {
            return true;
        }
        break;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
        if (forwardMouseEvent(static_cast<QMouseEvent *>(event))) {
            return true;
        }
        break;
    default:
        break;
    }
    return QWidget::eventFilter(obj, event);
}

bool PreviewWidget::forwardKeyEvent(QKeyEvent *event)
{
    const Qt::KeyboardModifiers mods = event->modifiers();
    const int key = event->key();

    // Keys the preview handles itself: scrolling/navigation and selection clipboard.
    // These stay with the web view so reading the document keeps working.
    if (mods == Qt::NoModifier || mods == Qt::ShiftModifier) {
        switch (key) {
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
        case Qt::Key_Home:
        case Qt::Key_End:
        case Qt::Key_Space:
            return false;
        default:
            break;
        }
    }
    if (mods == Qt::ControlModifier && (key == Qt::Key_C || key == Qt::Key_A || key == Qt::Key_Insert)) {
        return false;
    }

    QAction *action = kateActionFor(QKeySequence(event->keyCombination()));
    if (!action) {
        return false;
    }
    action->trigger();
    return true;
}

bool PreviewWidget::forwardMouseEvent(QMouseEvent *event)
{
    // The page uses left (click/select), right (context menu), and middle; hand the
    // side/extra buttons (Back/Forward/X buttons) to Kate's window so its mouse
    // bindings fire instead of being eaten by the web view.
    switch (event->button()) {
    case Qt::LeftButton:
    case Qt::RightButton:
    case Qt::MiddleButton:
    case Qt::NoButton:
        return false;
    default:
        break;
    }

    QWidget *win = m_mainWindow ? m_mainWindow->window() : nullptr;
    if (!win) {
        return false;
    }
    const QPoint global = event->globalPosition().toPoint();
    QMouseEvent copy(event->type(), win->mapFromGlobal(global), global, event->button(), event->buttons(), event->modifiers());
    QCoreApplication::sendEvent(win, &copy);
    return true;
}

namespace
{
// The enabled action of a GUI client, or of one of its children, that a shortcut triggers.
QAction *actionFor(KXMLGUIClient *client, const QKeySequence &seq)
{
    const QList<QAction *> actions = client->actionCollection()->actions();
    for (QAction *action : actions) {
        if (action->isEnabled() && action->shortcuts().contains(seq)) {
            return action;
        }
    }
    const QList<KXMLGUIClient *> children = client->childClients();
    for (KXMLGUIClient *child : children) {
        if (QAction *action = actionFor(child, seq)) {
            return action;
        }
    }
    return nullptr;
}
} // namespace

// The action of Kate's window that a shortcut triggers. The window's GUI is made of
// clients: Kate itself, its plugins and the active view. A view that is not shown is not
// among them, so its actions cannot answer for it.
QAction *PreviewWidget::kateActionFor(const QKeySequence &seq) const
{
    KXMLGUIFactory *factory = (seq.isEmpty() || !m_mainWindow) ? nullptr : m_mainWindow->guiFactory();
    if (!factory) {
        return nullptr;
    }
    const QList<KXMLGUIClient *> clients = factory->clients();
    for (KXMLGUIClient *client : clients) {
        if (QAction *action = actionFor(client, seq)) {
            return action;
        }
    }
    return nullptr;
}

// Qt WebEngine's own menu offers Back, Reload or View Page Source, which mean nothing in
// a preview, and Qt translates it into fourteen languages only (no French). This menu
// holds what a preview needs. Its first entries take their text from Qt's own text
// widgets, which Qt translates everywhere; the last ones are the actions the plugin
// added to this widget (the exports).
void PreviewWidget::showContextMenu(const QPoint &position)
{
    const QWebEngineContextMenuRequest *request = m_web->lastContextMenuRequest();
    QUrl link;
    bool hasSelection = false;
    if (request) {
        link = request->linkUrl();
        hasSelection = !request->selectedText().isEmpty();
    }

    QMenu menu(this);
    QAction *copy = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), QCoreApplication::translate("QWidgetTextControl", "&Copy"));
    copy->setEnabled(hasSelection);
    connect(copy, &QAction::triggered, this, [this]() {
        m_web->triggerPageAction(QWebEnginePage::Copy);
    });
    if (link.isValid()) {
        QAction *copyLink = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-link")), QCoreApplication::translate("QWidgetTextControl", "Copy &Link Location"));
        connect(copyLink, &QAction::triggered, this, [link]() {
            QGuiApplication::clipboard()->setText(link.toString());
        });
    }
    QAction *selectAll = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-select-all")), QCoreApplication::translate("QWidgetTextControl", "Select All"));
    connect(selectAll, &QAction::triggered, this, [this]() {
        m_web->triggerPageAction(QWebEnginePage::SelectAll);
    });
    if (!actions().isEmpty()) {
        menu.addSeparator();
        menu.addActions(actions());
    }
    menu.exec(m_web->mapToGlobal(position));
}
