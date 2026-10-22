// previewwidget.cpp
// PreviewWidget: building the bar and the web view, attaching a document, rendering.
// Code by uwuclxdy and Olivier Booklage.
// License: GPL-3.0-or-later

#include "previewwidget.h"
#include "previewpage.h"
#include "previewutil.h"
#include "settings.h"

#include <QAction>
#include <QCheckBox>
#include <QCursor>
#include <QDesktopServices>
#include <QIcon>
#include <QJsonObject>
#include <QLabel>
#include <QTimer>
#include <QToolBar>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineNewWindowRequest>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include <KLocalizedString>
#include <KTextEditor/Document>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>

namespace
{
// How long the text has to stay unchanged before the page renders it.
constexpr int RenderDebounceMs = 150;

constexpr int BarIconSizePx = 16;

// A page that has not answered for this long is said to be rendering, and the dots after
// the word move at this pace.
constexpr int BusyTickMs = 400;
constexpr int BusyLabelMarginPx = 8;

// Default titles of the alerts and callouts, in the user's language. The page has no
// access to the translations, so it gets them from here.
QJsonObject calloutLabels()
{
    return QJsonObject{
        {QStringLiteral("note"), i18nc("@title GitHub alert", "Note")},
        {QStringLiteral("tip"), i18nc("@title GitHub alert", "Tip")},
        {QStringLiteral("important"), i18nc("@title GitHub alert", "Important")},
        {QStringLiteral("warning"), i18nc("@title GitHub alert", "Warning")},
        {QStringLiteral("caution"), i18nc("@title GitHub alert", "Caution")},
        {QStringLiteral("abstract"), i18nc("@title Obsidian callout", "Abstract")},
        {QStringLiteral("info"), i18nc("@title Obsidian callout", "Info")},
        {QStringLiteral("todo"), i18nc("@title Obsidian callout", "Todo")},
        {QStringLiteral("success"), i18nc("@title Obsidian callout", "Success")},
        {QStringLiteral("question"), i18nc("@title Obsidian callout", "Question")},
        {QStringLiteral("failure"), i18nc("@title Obsidian callout", "Failure")},
        {QStringLiteral("danger"), i18nc("@title Obsidian callout", "Danger")},
        {QStringLiteral("bug"), i18nc("@title Obsidian callout", "Bug")},
        {QStringLiteral("example"), i18nc("@title Obsidian callout", "Example")},
        {QStringLiteral("quote"), i18nc("@title Obsidian callout", "Quote")},
        {QStringLiteral("definition"), i18nc("@title teaching callout", "Definition")},
        {QStringLiteral("theorem"), i18nc("@title teaching callout", "Theorem")},
        {QStringLiteral("lemma"), i18nc("@title teaching callout", "Lemma")},
        {QStringLiteral("proposition"), i18nc("@title teaching callout", "Proposition")},
        {QStringLiteral("corollary"), i18nc("@title teaching callout", "Corollary")},
        {QStringLiteral("axiom"), i18nc("@title teaching callout", "Axiom")},
        {QStringLiteral("claim"), i18nc("@title teaching callout", "Claim")},
        {QStringLiteral("assumption"), i18nc("@title teaching callout", "Assumption")},
        {QStringLiteral("conjecture"), i18nc("@title teaching callout", "Conjecture")},
        {QStringLiteral("hypothesis"), i18nc("@title teaching callout", "Hypothesis")},
        {QStringLiteral("proof"), i18nc("@title teaching callout", "Proof")},
        {QStringLiteral("exercise"), i18nc("@title teaching callout", "Exercise")},
        {QStringLiteral("method"), i18nc("@title teaching callout", "Method")},
        {QStringLiteral("remark"), i18nc("@title teaching callout", "Remark")},
        {QStringLiteral("solution"), i18nc("@title teaching callout", "Solution")},
    };
}
} // namespace

PreviewWidget::PreviewWidget(KTextEditor::MainWindow *mainWindow, KTextEditor::View *view, KTextEditor::Document *doc, QWidget *parent)
    : QWidget(parent)
    , m_mainWindow(mainWindow)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    createBar();
    layout->addWidget(m_bar);
    createWebView();
    layout->addWidget(m_web);

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(RenderDebounceMs);
    connect(m_debounce, &QTimer::timeout, this, &PreviewWidget::render);

    connect(Settings::self(), &Settings::changed, this, &PreviewWidget::applyTheme);
    connect(Settings::self(), &Settings::changed, this, &PreviewWidget::applyMediaPolicy);
    connect(Settings::self(), &Settings::changed, this, &PreviewWidget::applyMacros);
    connect(Settings::self(), &Settings::changed, this, &PreviewWidget::applySyntax);

    setWindowIcon(QIcon::fromTheme(QStringLiteral("text-markdown")));
    applyMediaPolicy();
    attachDocument(doc, view);
}

// The bar above the preview: what the page renders on the left, the exports on the right
// once the window hands them over.
void PreviewWidget::createBar()
{
    m_bar = new QToolBar(this);
    m_bar->setIconSize(QSize(BarIconSizePx, BarIconSizePx));
    m_bar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    // A check box: a flat button that is off reads as a mere label.
    m_githubOnlyBox = new QCheckBox(i18n("GitHub only"), m_bar);
    m_githubOnlyBox->setToolTip(i18n("Render only what GitHub renders, without the CodiMD, Obsidian and MkDocs syntax"));
    m_githubOnlyBox->setChecked(Settings::self()->githubOnly());
    connect(m_githubOnlyBox, &QCheckBox::toggled, Settings::self(), &Settings::setGithubOnly);
    m_bar->addWidget(m_githubOnlyBox);
    // Empty while the page keeps up: see runJs().
    m_busyLabel = new QLabel(m_bar);
    m_busyLabel->setObjectName(QStringLiteral("busy"));
    m_busyLabel->setContentsMargins(BusyLabelMarginPx, 0, 0, 0);
    m_bar->addWidget(m_busyLabel);
    m_busyTimer = new QTimer(this);
    m_busyTimer->setInterval(BusyTickMs);
    connect(m_busyTimer, &QTimer::timeout, this, [this]() {
        m_busyDots = m_busyDots % 3 + 1;
        m_busyLabel->setText(i18nc("@info:status the preview is rendering a long document; one to three dots follow", "Rendering")
                             + QString(m_busyDots, QLatin1Char('.')));
    });
    auto *spacer = new QWidget(m_bar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_bar->addWidget(spacer);
}

void PreviewWidget::createWebView()
{
    // Each preview has its own off-the-record profile and guard, so it stays inside its
    // own document folder.
    m_guard = new LocalFileGuard(this);
    m_profile = new QWebEngineProfile(this);
    m_profile->setUrlRequestInterceptor(m_guard);

    m_web = new QWebEngineView(this);
    auto *page = new PreviewPage(m_profile, m_web);
    page->onLinkActivated = [this](const QUrl &url) {
        openLink(url);
    };
    // A link that asks for a window of its own (target="_blank", a middle click) opens
    // like any other.
    connect(page, &QWebEnginePage::newWindowRequested, this, [this](QWebEngineNewWindowRequest &request) {
        openLink(request.requestedUrl());
    });
    // Where a link goes, shown while the pointer is on it. Leaving the link sends an
    // empty address, which hides the tip.
    connect(page, &QWebEnginePage::linkHovered, this, [this](const QString &address) {
        QToolTip::showText(QCursor::pos(), linkHint(QUrl(address), baseUrl()), m_web);
    });
    m_web->setPage(page);
    // The menu is built in showContextMenu() instead of being left to Qt WebEngine.
    m_web->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_web, &QWidget::customContextMenuRequested, this, &PreviewWidget::showContextMenu);
    connect(page, &QWebEnginePage::scrollPositionChanged, this, &PreviewWidget::syncFromPreview);
    connect(page, &QWebEnginePage::pdfPrintingFinished, this, &PreviewWidget::onPdfPrinted);
    // A page whose process died answers nothing more: an export would wait for ever.
    connect(page, &QWebEnginePage::renderProcessTerminated, this, [this]() {
        m_loaded = false;
        setIdle();
        endExport(false);
    });
    m_web->settings()->setAttribute(QWebEngineSettings::FocusOnNavigationEnabled, false);
    m_web->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    // Watch the view for the lazily-created render widget so we can attach to it.
    m_web->installEventFilter(this);
    installInputFilter();
    connect(m_web, &QWebEngineView::loadFinished, this, &PreviewWidget::onLoadFinished);
}

void PreviewWidget::onLoadFinished(bool ok)
{
    if (!ok) {
        return;
    }
    m_loaded = true;
    installInputFilter();
    runJs(QStringLiteral("window.__setLabels(%1);").arg(compactJson(calloutLabels())));
    // While pictures from the web are kept out, the page names the setting that lets them
    // in, in place of each one: the same text as on the settings page (configpage.cpp).
    // A change of that setting reloads the page.
    const QString mediaHint = Settings::self()->loadRemoteMedia() ? QString() : i18n("Load media previews from remote URLs");
    runJs(QStringLiteral("window.__setRemoteMediaHint(%1);").arg(jsLiteral(mediaHint)));
    applyTheme();
    applyMacros();
    applySyntax();
    render();
}

void PreviewWidget::attachDocument(KTextEditor::Document *doc, KTextEditor::View *view)
{
    setView(view); // the same document can come back under another view (split views)
    if (doc && m_doc == doc) {
        return;
    }
    if (m_doc) {
        m_doc->disconnect(this);
    }
    m_doc = doc;
    if (doc) {
        connect(doc, &KTextEditor::Document::textChanged, this, &PreviewWidget::scheduleRender);
        connect(doc, &KTextEditor::Document::documentUrlChanged, this, &PreviewWidget::onDocumentUrlChanged);
        connect(doc, &KTextEditor::Document::aboutToClose, this, &PreviewWidget::snapshotSource);
    }
    // Reloading the page costs a blank flash and the diagrams already drawn. It is only
    // needed when the document's folder changes, because relative images and the file
    // guard depend on it; otherwise pushing the new text is enough.
    const QUrl previousBase = baseUrl();
    if (doc) {
        m_url = doc->url();
    }
    if (m_loaded && baseUrl() == previousBase) {
        render();
    } else {
        loadPage();
    }
}

// aboutToClose is the last moment the buffer still holds the document's text;
// closeUrl() empties it immediately afterwards.
void PreviewWidget::snapshotSource()
{
    if (m_doc) {
        m_text = m_doc->text();
    }
}

// Delete in this order: the view and its page, then the profile, then the guard the
// profile uses. Qt promises no order for deleting children, so it is done by hand.
PreviewWidget::~PreviewWidget()
{
    delete m_web;
    m_web = nullptr;
    delete m_profile;
    m_profile = nullptr;
    delete m_guard;
    m_guard = nullptr;
}

void PreviewWidget::openLink(const QUrl &url)
{
    if (url.isLocalFile() && m_mainWindow) {
        m_mainWindow->openUrl(localLinkTarget(url, baseUrl()));
    } else if (!url.scheme().isEmpty()) {
        QDesktopServices::openUrl(url);
    }
}

// The page runs its scripts one after the other, so an answer to the last one sent means
// it has caught up. While it has not, the bar says so after a moment: a long document
// takes seconds to render.
void PreviewWidget::runJs(const QString &code)
{
    if (!m_web || !m_loaded) {
        return;
    }
    const int script = ++m_scriptsSent;
    if (!m_busyTimer->isActive()) {
        m_busyTimer->start();
    }
    // The answer arrives later, when this widget may be gone: QPointer turns null then.
    const QPointer<PreviewWidget> self(this);
    m_web->page()->runJavaScript(code, [self, script](const QVariant &) {
        if (self) {
            self->onScriptAnswered(script);
        }
    });
}

void PreviewWidget::onScriptAnswered(int script)
{
    m_scriptsAnswered = qMax(m_scriptsAnswered, script);
    if (m_scriptsAnswered < m_scriptsSent) {
        m_busyTimer->start(); // the page answers: the wait starts over for the next script
    } else {
        setIdle();
    }
}

// Nothing is expected from the page any more: it has answered, or it is going away.
void PreviewWidget::setIdle()
{
    m_scriptsAnswered = m_scriptsSent;
    m_busyTimer->stop();
    m_busyDots = 0;
    m_busyLabel->clear();
}

void PreviewWidget::render()
{
    if (!m_loaded) {
        return;
    }
    if (m_doc) {
        m_text = m_doc->text();
    }
    runJs(QStringLiteral("window.__setMarkdown(%1);").arg(jsLiteral(m_text)));
    syncFromEditor();
}

// Hand the page the math macros of the settings; it renders again when they changed.
void PreviewWidget::applyMacros()
{
    runJs(QStringLiteral("window.__setMacros(%1);").arg(jsLiteral(Settings::self()->mathMacros())));
}

// Tell the page which syntax to read, and keep the check box in step with a change made
// from the bar of another window.
void PreviewWidget::applySyntax()
{
    const bool githubOnly = Settings::self()->githubOnly();
    m_githubOnlyBox->setChecked(githubOnly);
    runJs(QStringLiteral("window.__setGithubOnly(%1);").arg(githubOnly ? QStringLiteral("true") : QStringLiteral("false")));
}

void PreviewWidget::addExportActions(const QList<QAction *> &actions)
{
    addActions(actions); // showContextMenu() lists them
    m_bar->addActions(actions);
}

void PreviewWidget::scheduleRender()
{
    if (!m_paused) {
        m_debounce->start();
    }
}

void PreviewWidget::setPaused(bool paused)
{
    if (m_paused == paused) {
        return;
    }
    m_paused = paused;
    if (paused) {
        m_debounce->stop();
    } else {
        render(); // catch up with the edits made meanwhile
    }
}
