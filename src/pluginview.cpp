// pluginview.cpp
// PluginView: the tool view, the preview it holds and the export actions.
// Code by uwuclxdy and Olivier Booklage.
// License: GPL-3.0-or-later

#include "pluginview.h"
#include "previewutil.h"
#include "previewwidget.h"

#include <QAction>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>

#include <KActionCollection>
#include <KLocalizedString>
#include <KXMLGUIFactory>
#include <KTextEditor/Document>
#include <KTextEditor/MainWindow>
#include <KTextEditor/Plugin>
#include <KTextEditor/View>

namespace
{
// Space around the text shown in the empty panel.
constexpr int HintMarginPx = 16;
} // namespace

// A document is Markdown by its highlighting mode, or by its file extension when the
// mode was not detected.
bool PluginView::isMarkdown(KTextEditor::Document *doc)
{
    if (doc->highlightingMode().compare(QLatin1String("Markdown"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    const QString path = doc->url().path();
    return path.endsWith(QLatin1String(".md"), Qt::CaseInsensitive) || path.endsWith(QLatin1String(".markdown"), Qt::CaseInsensitive)
        || path.endsWith(QLatin1String(".mkd"), Qt::CaseInsensitive);
}

PluginView::PluginView(KTextEditor::Plugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(plugin)
    , KXMLGUIClient()
    , m_mainWindow(mainWindow)
{
    setComponentName(QStringLiteral("katemark"), i18n("Katemark"));

    const QIcon icon = QIcon::fromTheme(QStringLiteral("text-markdown"), QIcon::fromTheme(QStringLiteral("view-preview")));
    m_toolView = m_mainWindow->createToolView(plugin, QStringLiteral("katemark"), KTextEditor::MainWindow::Right, icon, i18n("Markdown Preview"));
    // Kate has no signal for a tool view being shown; its Show event is the cue.
    m_toolView->installEventFilter(this);
    // The preview comes with the first Markdown document. Until then the panel would be
    // blank, with nothing to tell a new document from a broken plugin.
    auto *hint = new QLabel(i18n("No Markdown document yet. Save this one as .md, or set its mode to Markdown."), m_toolView);
    hint->setWordWrap(true);
    hint->setAlignment(Qt::AlignCenter);
    hint->setMargin(HintMarginPx);
    m_hint = hint;

    QAction *toggle = addAction(QStringLiteral("katemark_show"), i18n("Preview"), icon);
    toggle->setToolTip(i18n("Show or hide the Markdown preview beside the editor"));
    actionCollection()->setDefaultShortcut(toggle, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    connect(toggle, &QAction::triggered, this, &PluginView::togglePreview);

    m_exportPdf = addAction(QStringLiteral("katemark_export_pdf"), i18n("Export Preview as PDF..."), QIcon::fromTheme(QStringLiteral("application-pdf")));
    // What a toolbar shows beside the icon; the menu keeps the full text.
    m_exportPdf->setIconText(QStringLiteral("PDF"));
    connect(m_exportPdf, &QAction::triggered, this, [this]() {
        exportPreview(ExportFormat::Pdf);
    });
    m_exportHtml = addAction(QStringLiteral("katemark_export_html"), i18n("Export Preview as HTML..."), QIcon::fromTheme(QStringLiteral("text-html")));
    m_exportHtml->setIconText(QStringLiteral("HTML"));
    connect(m_exportHtml, &QAction::triggered, this, [this]() {
        exportPreview(ExportFormat::Html);
    });
    applyToolViewVisibility(false);
    addEditActions();

    setXML(readAsset(QStringLiteral(":/katemark/ui.rc")));

    m_mainWindow->guiFactory()->addClient(this);

    connect(m_mainWindow, &KTextEditor::MainWindow::viewChanged, this, &PluginView::onViewChanged);
    onViewChanged(m_mainWindow->activeView());
}

// Register an action under a name that data/ui.rc places in the menu and the toolbar.
QAction *PluginView::addAction(const QString &name, const QString &text, const QIcon &icon)
{
    QAction *action = actionCollection()->addAction(name);
    action->setText(text);
    action->setIcon(icon);
    return action;
}

PluginView::~PluginView()
{
    m_mainWindow->guiFactory()->removeClient(this);
    delete m_toolView;
}

bool PluginView::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_toolView && event->type() == QEvent::Show) {
        applyToolViewVisibility(true);
        // Queued, not direct: building the preview's QWebEngineView makes Qt recreate
        // the window, which re-delivers Show to the tool view in the middle of that
        // construction and would build a second preview.
        QMetaObject::invokeMethod(this, &PluginView::syncPreview, Qt::QueuedConnection);
    } else if (obj == m_toolView && event->type() == QEvent::Hide) {
        applyToolViewVisibility(false);
    }
    return QObject::eventFilter(obj, event);
}

// An export writes what the preview shows, so the exports are offered while it shows: once
// a Markdown document has been shown in the panel, and one export at a time.
void PluginView::setExportsOffered(bool toolViewVisible)
{
    const bool offered = toolViewVisible && m_preview && !m_preview->isExporting();
    m_exportPdf->setEnabled(offered);
    m_exportHtml->setEnabled(offered);
}

void PluginView::applyToolViewVisibility(bool visible)
{
    setExportsOffered(visible);
    if (m_preview) {
        m_preview->setPaused(!visible);
    }
}

void PluginView::onViewChanged(KTextEditor::View *view)
{
    // Each view has its own paste action. Kate's own slot runs first and does nothing
    // with an image; ours then takes it. UniqueConnection: coming back to a view must
    // not connect it a second time.
    QAction *paste = nullptr;
    if (view) {
        paste = view->action(QStringLiteral("edit_paste"));
    }
    if (paste) {
        connect(paste, &QAction::triggered, this, &PluginView::pasteImage, Qt::UniqueConnection);
    }
    // A document becomes Markdown when it is saved as .md or switched to that mode;
    // the preview has to pick it up then, not at the next change of tab.
    if (view) {
        KTextEditor::Document *doc = view->document();
        connect(doc, &KTextEditor::Document::documentUrlChanged, this, &PluginView::syncPreview, Qt::UniqueConnection);
        connect(doc, &KTextEditor::Document::highlightingModeChanged, this, &PluginView::syncPreview, Qt::UniqueConnection);
    }
    syncPreview();
}

void PluginView::exportPreview(ExportFormat format)
{
    if (!m_preview) {
        return; // the panel is open but no Markdown document was ever shown in it
    }
    const bool pdf = format == ExportFormat::Pdf;
    // Suggest the document's own name and folder, with the export's extension.
    const QFileInfo source(m_preview->documentUrl().toLocalFile());
    QString suggested = i18n("Untitled");
    if (!source.fileName().isEmpty()) {
        suggested = source.absolutePath() + QLatin1Char('/') + source.completeBaseName();
    }
    suggested += pdf ? QStringLiteral(".pdf") : QStringLiteral(".html");
    const QString path = QFileDialog::getSaveFileName(m_mainWindow->window(),
                                                      pdf ? i18n("Export as PDF") : i18n("Export as HTML"),
                                                      suggested,
                                                      pdf ? i18n("PDF files (*.pdf)") : i18n("HTML files (*.html)"));
    if (path.isEmpty()) {
        return;
    }
    if (pdf) {
        m_preview->exportPdf(path);
    } else {
        m_preview->exportHtml(path);
    }
    setExportsOffered(m_toolView->isVisible()); // not again until this one has ended
}

void PluginView::onExported(const QString &path, bool ok)
{
    setExportsOffered(m_toolView->isVisible());
    if (ok) {
        showMessage(i18n("Preview exported to %1", path), false, path);
    } else {
        showMessage(i18n("Could not export the preview to %1", path), true);
    }
}

void PluginView::togglePreview()
{
    if (m_toolView->isVisible()) {
        m_mainWindow->hideToolView(m_toolView);
    } else {
        m_mainWindow->showToolView(m_toolView);
    }
}

// Point the preview at the active view's document. Only while the tool view is showing,
// so the web view is not created until the preview is first used. A non-Markdown view
// leaves the last preview in place.
void PluginView::syncPreview()
{
    KTextEditor::View *view = m_mainWindow->activeView();
    if (!m_toolView->isVisible() || !view || !isMarkdown(view->document())) {
        return;
    }
    if (m_preview) {
        m_preview->attachDocument(view->document(), view);
    } else {
        delete m_hint;
        m_preview = new PreviewWidget(m_mainWindow, view, view->document(), m_toolView);
        connect(m_preview, &PreviewWidget::exported, this, &PluginView::onExported);
        m_preview->addExportActions({m_exportPdf, m_exportHtml});
        applyToolViewVisibility(true);
    }
}
