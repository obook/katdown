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
    connect(page, &QWebEnginePage::pdfPrintingFinished, this, [this](const QString &path, bool ok) {
        if (path == m_exportPath) {
            endExport(ok);
        }
    });
    // A page whose process died answers nothing more: an export would wait for ever.
    connect(page, &QWebEnginePage::renderProcessTerminated, this, [this]() {
        m_loaded = false;
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

void PreviewWidget::runJs(const QString &code)
{
    if (m_web && m_loaded) {
        m_web->page()->runJavaScript(code);
    }
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
