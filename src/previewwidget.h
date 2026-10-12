// previewwidget.h
// PreviewWidget: the Markdown preview, a bar above a web view.
// Code by uwuclxdy and Olivier Booklage.
// License: GPL-3.0-or-later

#pragma once

#include <QPointer>
#include <QUrl>
#include <QWidget>

#include <functional>

#include <KTextEditor/Document>
#include <KTextEditor/View>

class LocalFileGuard;
class QWebEngineView;
class QWebEngineProfile;
class QTimer;
class QToolBar;
class QAction;
class QCheckBox;
class QKeyEvent;
class QKeySequence;
class QMouseEvent;

namespace KTextEditor
{
class MainWindow;
}

/**
 * The Markdown preview: a QWebEngineView showing a self-contained HTML page. The widget
 * pushes the text to it on every change and takes its colors from the editor theme or
 * GitHub's palette.
 *
 * The widget outlives its document: Kate destroys the document when its editor tab
 * closes, so the widget keeps a copy of the text and url and shows it until it gets
 * another document.
 */
class PreviewWidget : public QWidget
{
    Q_OBJECT
public:
    PreviewWidget(KTextEditor::MainWindow *mainWindow, KTextEditor::View *view, KTextEditor::Document *doc, QWidget *parent = nullptr);
    ~PreviewWidget() override;

    // Still valid after the document is gone.
    QUrl documentUrl() const
    {
        return m_url;
    }

    void attachDocument(KTextEditor::Document *doc, KTextEditor::View *view);

    // A paused preview stops following the editor: nobody sees it, so rendering every
    // edit would be wasted work. Resuming renders the current text.
    void setPaused(bool paused);

    // Write the rendered document to a file, in GitHub's light look whatever the preview
    // shows: a dark page makes a poor printout. Both finish with exported(). One export
    // runs at a time; a second one asked meanwhile fails at once.
    void exportPdf(const QString &path);
    void exportHtml(const QString &path);
    bool isExporting() const
    {
        return !m_exportPath.isEmpty();
    }

    // The export actions belong to the window. They are offered in the bar above the
    // preview and in its context menu.
    void addExportActions(const QList<QAction *> &actions);

Q_SIGNALS:
    void exported(const QString &path, bool ok);

public Q_SLOTS:
    void applyTheme();

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private Q_SLOTS:
    void scheduleRender();
    void onDocumentUrlChanged();
    void snapshotSource();
    void syncFromEditor();
    void syncFromPreview();

private:
    // previewwidget.cpp
    void createBar();
    void createWebView();
    void onLoadFinished(bool ok);
    void render();
    void applyMacros();
    void applySyntax();
    void runJs(const QString &code);
    void openLink(const QUrl &url);

    // previewtheme.cpp
    void applyGithubLook(bool dark);

    // previewexport.cpp
    bool beginExport(const QString &path);
    void onPdfPrinted(const QString &path, bool ok);
    void endExport(bool ok);
    void whenSettled(std::function<void()> then, int checksLeft);

    // previewload.cpp
    void loadPage();
    void applyMediaPolicy();
    QUrl baseUrl() const;
    static QString buildHtml();

    // previewscroll.cpp
    void setView(KTextEditor::View *view);

    // previewinput.cpp. QWebEngineView's render widget swallows keys and mouse buttons
    // before Kate's shortcut machinery sees them: those the preview does not use go back
    // to Kate.
    void installInputFilter();
    bool forwardKeyEvent(QKeyEvent *event);
    bool forwardMouseEvent(QMouseEvent *event);
    QAction *kateActionFor(const QKeySequence &seq) const;
    void showContextMenu(const QPoint &position);

    QPointer<KTextEditor::MainWindow> m_mainWindow;
    QWebEngineView *m_web = nullptr;
    QToolBar *m_bar = nullptr;
    QCheckBox *m_githubOnlyBox = nullptr;
    QWebEngineProfile *m_profile = nullptr;
    LocalFileGuard *m_guard = nullptr;
    QTimer *m_debounce = nullptr;
    QPointer<KTextEditor::Document> m_doc;
    QPointer<KTextEditor::View> m_view;
    QPointer<QWidget> m_inputTarget;
    QPointer<QWidget> m_exportCover; // a picture of the page, over it during an export
    QString m_exportPath; // the file being written, empty when no export runs
    int m_exportRun = 0; // counts the exports: a cover is removed by the one that put it
    bool m_printsUnnumbered = false; // a PDF of one page is being printed again, without "1 / 1"
    QUrl m_url;
    QString m_text;
    bool m_paused = false;
    bool m_loaded = false;
    bool m_pageAllowsRemoteMedia = false; // the setting the page was loaded with
};
