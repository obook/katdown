// previewexport.cpp
// PreviewWidget: exporting the rendered document to PDF or to a standalone HTML file.
// Author: Olivier Booklage
// Date: October 2026
// License: GPL-3.0-or-later

#include "previewpage.h"
#include "previewtheme.h"
#include "previewutil.h"
#include "previewwidget.h"
#include "settings.h"

#include <QFileInfo>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QMimeDatabase>
#include <QPageLayout>
#include <QPageSize>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>
#include <QWebEnginePage>
#include <QWebEngineView>

#include <utility>

namespace
{
// How often an export checks whether the page has finished drawing, and how many times:
// it waits at most 5 s.
constexpr int SettleIntervalMs = 100;
constexpr int MaxSettleChecks = 50;

// Margin around an exported PDF page.
constexpr qreal ExportMarginMm = 15;

// Replace every picture of the document folder by its content, so that the exported file
// can be moved or sent on its own. A picture that cannot be read keeps its address, and so
// does one stored elsewhere: the guard keeps it out of the page, and the export must not
// carry a file the reader never saw.
QString embedLocalImages(const QString &html, const LocalFileGuard &guard)
{
    static const QRegularExpression localPicture(QStringLiteral("(<img\\b[^>]*?\\bsrc=\")(file://[^\"]+)(\")"));
    QString result;
    qsizetype copied = 0;
    QRegularExpressionMatchIterator pictures = localPicture.globalMatch(html);
    while (pictures.hasNext()) {
        const QRegularExpressionMatch picture = pictures.next();
        // The page wrote the address as HTML: "&" comes as "&amp;".
        const QString address = picture.captured(2).replace(QLatin1String("&amp;"), QLatin1String("&"));
        const QString path = QUrl(address).toLocalFile();
        QFile file(path);
        if (!guard.allows(path) || !file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QString type = QMimeDatabase().mimeTypeForFile(path).name();
        const QString content = QString::fromLatin1(file.readAll().toBase64());
        result += html.mid(copied, picture.capturedStart(2) - copied);
        result += QStringLiteral("data:%1;base64,%2").arg(type, content);
        copied = picture.capturedEnd(2);
    }
    return result + html.mid(copied);
}

// Print the page to a PDF file, on A4 or Letter by the locale. The page answers with
// pdfPrintingFinished.
void printPdf(QWebEnginePage *page, const QString &path)
{
    const QPageSize size(QLocale().measurementSystem() == QLocale::MetricSystem ? QPageSize::A4 : QPageSize::Letter);
    page->printToPdf(path, QPageLayout(size, QPageLayout::Portrait, QMarginsF(ExportMarginMm, ExportMarginMm, ExportMarginMm, ExportMarginMm), QPageLayout::Millimeter));
}

// How many pages a PDF written by Chromium has, 0 when the file cannot be read. Chromium
// writes "/Type /Page" once per page and "/Type /Pages" once per node of the page tree,
// both in clear.
int pdfPageCount(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    const QByteArray pdf = file.readAll();
    return pdf.count("/Type /Page") - pdf.count("/Type /Pages");
}

// A page that stands on its own: the rendered article with the stylesheets it needs and
// no script. Its policy refuses what the raw HTML of a document could still bring:
// javascript: links, frames, forms, plugins.
QString standaloneHtml(const QString &title, const QString &body)
{
    const QString base = QStringLiteral(":/katemark/css/");
    QString css = readAsset(base + QStringLiteral("github-markdown.css")) + readAsset(base + QStringLiteral("base.css"))
        + readAsset(base + QStringLiteral("hljs-github.min.css"));
    if (body.contains(QLatin1String("class=\"katex"))) {
        css += readAsset(base + QStringLiteral("katex.min.css"));
    }
    QString vars = QStringLiteral("color-scheme:light;");
    const QJsonObject light = githubVars(false);
    for (auto it = light.begin(); it != light.end(); ++it) {
        vars += it.key() + QLatin1Char(':') + it.value().toString() + QLatin1Char(';');
    }
    return QStringLiteral("<!DOCTYPE html>\n<html data-pv-scheme=\"light\" style=\"%1\">\n<head>\n<meta charset=\"utf-8\">\n"
                          "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; img-src data: http: https: file:; "
                          "media-src http: https: file:; style-src 'unsafe-inline'; font-src data:; base-uri 'none'; form-action 'none'\">\n"
                          "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<title>%2</title>\n<style>%3</style>\n"
                          "</head>\n<body>\n<article class=\"markdown-body\" id=\"content\">%4</article>\n</body>\n</html>\n")
        .arg(vars, title.toHtmlEscaped(), css, body);
}
} // namespace

// Run then once the page has finished drawing: switching look redraws the diagrams, and
// an export taken before that would catch them half done. Gives up waiting when
// checksLeft runs out.
void PreviewWidget::whenSettled(std::function<void()> then, int checksLeft)
{
    // The answer arrives later, when this widget may be gone: QPointer turns null then.
    const QPointer<PreviewWidget> self(this);
    m_web->page()->runJavaScript(QStringLiteral("window.__settled()"), [self, then, checksLeft](const QVariant &settled) {
        if (!self) {
            return;
        }
        if (settled.toBool() || checksLeft <= 0) {
            then();
        } else {
            QTimer::singleShot(SettleIntervalMs, self, [self, then, checksLeft]() {
                self->whenSettled(then, checksLeft - 1);
            });
        }
    });
}

// Start an export, or refuse it with exported(path, false): one runs at a time, because
// the first to end gives the page its own look back while the other still writes.
// An export writes the page in GitHub's light look. The reader keeps seeing a picture of
// the preview as it was: on a dark preview the switch would show as a white flash.
bool PreviewWidget::beginExport(const QString &path)
{
    if (!m_loaded || isExporting()) {
        Q_EMIT exported(path, false);
        return false;
    }
    m_exportPath = path;
    ++m_exportRun; // a cover the export before has not removed yet now serves this one
    const bool showsExportLook = Settings::self()->colorSource() == Settings::GitHub && !githubLookIsDark();
    if (!showsExportLook && !m_exportCover) {
        auto *cover = new QLabel(m_web);
        cover->setPixmap(m_web->grab());
        cover->setScaledContents(true); // follows the view when it is resized (eventFilter)
        cover->setAttribute(Qt::WA_TransparentForMouseEvents); // the page keeps the wheel and the clicks
        cover->resize(m_web->size());
        cover->show();
        m_exportCover = cover;
    }
    applyGithubLook(false);
    return true;
}

// End the running export: give the page its own look back, uncover it once it shows that
// look again, and report. Does nothing when no export runs.
void PreviewWidget::endExport(bool ok)
{
    if (!isExporting()) {
        return;
    }
    if (std::exchange(m_printsUnnumbered, false)) {
        runJs(QStringLiteral("window.__setPageNumbers(true);"));
    }
    const QString path = std::exchange(m_exportPath, QString());
    if (m_loaded) {
        applyTheme();
        whenSettled(
            [this, run = m_exportRun]() {
                // ponytail: settled is not yet painted, so the picture stays one interval
                // more. A slow machine could still show a light frame; watch the page's
                // pixels if it does.
                QTimer::singleShot(SettleIntervalMs, this, [this, run]() {
                    if (run == m_exportRun) {
                        delete m_exportCover;
                    }
                });
            },
            MaxSettleChecks);
    } else {
        delete m_exportCover; // the page is gone: the next one comes with its own look
    }
    Q_EMIT exported(path, ok);
}

void PreviewWidget::exportPdf(const QString &path)
{
    if (!beginExport(path)) {
        return;
    }
    whenSettled(
        [this, path, run = m_exportRun]() {
            if (run != m_exportRun || !isExporting()) {
                return; // ended meanwhile, by a reload of the page
            }
            printPdf(m_web->page(), path);
        },
        MaxSettleChecks);
}

// The PDF is written. A single page needs no "1 / 1" at its foot: that file is printed
// once more, without the numbers.
void PreviewWidget::onPdfPrinted(const QString &path, bool ok)
{
    if (path != m_exportPath) {
        return;
    }
    if (ok && !m_printsUnnumbered && pdfPageCount(path) == 1) {
        m_printsUnnumbered = true;
        m_web->page()->runJavaScript(QStringLiteral("window.__setPageNumbers(false)"), [self = QPointer<PreviewWidget>(this), path, run = m_exportRun](const QVariant &) {
            if (self && run == self->m_exportRun && self->isExporting()) {
                printPdf(self->m_web->page(), path);
            }
        });
        return;
    }
    endExport(ok);
}

void PreviewWidget::exportHtml(const QString &path)
{
    if (!beginExport(path)) {
        return;
    }
    whenSettled(
        [this, path, run = m_exportRun]() {
            m_web->page()->runJavaScript(QStringLiteral("window.__exportBody()"), [self = QPointer<PreviewWidget>(this), path, run](const QVariant &body) {
                if (!self || run != self->m_exportRun || !self->isExporting()) {
                    return;
                }
                const QString title = QFileInfo(self->m_url.fileName()).completeBaseName();
                const QString article = body.toString();
                bool ok = false;
                // An empty article means the page did not answer: nothing worth a file.
                if (!article.isEmpty()) {
                    const QByteArray html = standaloneHtml(title, embedLocalImages(article, *self->m_guard)).toUtf8();
                    // QSaveFile writes to a temporary file and replaces the target only on
                    // commit(), so a failed export never leaves a half-written file behind.
                    QSaveFile file(path);
                    if (file.open(QIODevice::WriteOnly)) {
                        file.write(html);
                        ok = file.commit();
                    }
                }
                self->endExport(ok);
            });
        },
        MaxSettleChecks);
}
