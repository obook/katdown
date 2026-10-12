// pluginview.h
// PluginView: what the plugin adds to one Kate window.
// Code by uwuclxdy and Olivier Booklage.
// License: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointer>

#include <functional>

#include <KXMLGUIClient>

class PreviewWidget;
class QAction;
class QIcon;
class QKeySequence;

namespace KTextEditor
{
class Document;
class MainWindow;
class Plugin;
class View;
}

/**
 * One instance per Kate main window. Owns a tool view docked on the right, like Kate's
 * own document preview: the source stays in the editor area and the preview beside it
 * follows the active Markdown document.
 */
class PluginView : public QObject, public KXMLGUIClient
{
    Q_OBJECT
public:
    PluginView(KTextEditor::Plugin *plugin, KTextEditor::MainWindow *mainWindow);
    ~PluginView() override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    // What an editing action does to the view it is given.
    using EditFunction = std::function<void(KTextEditor::View *)>;

    enum class ExportFormat {
        Pdf,
        Html,
    };

    // pluginview.cpp
    static bool isMarkdown(KTextEditor::Document *doc);
    QAction *addAction(const QString &name, const QString &text, const QIcon &icon);
    void togglePreview();
    void syncPreview();
    void onViewChanged(KTextEditor::View *view);
    void exportPreview(ExportFormat format);
    void onExported(const QString &path, bool ok);
    void setExportsOffered(bool toolViewVisible);
    void applyToolViewVisibility(bool visible);

    // pluginviewedit.cpp
    KTextEditor::View *markdownView() const;
    void addEditAction(const QString &name, const QString &text, const QString &iconName, const QKeySequence &shortcut, const EditFunction &edit);
    void addEditActions();
    void showMessage(const QString &text, bool isError, const QString &fileToOpen = QString());
    void pasteImage();

    KTextEditor::MainWindow *m_mainWindow = nullptr;
    QPointer<QWidget> m_toolView;
    QPointer<PreviewWidget> m_preview;
    QPointer<QWidget> m_hint; // what the panel says until it has a preview
    QAction *m_exportPdf = nullptr;
    QAction *m_exportHtml = nullptr;
};
