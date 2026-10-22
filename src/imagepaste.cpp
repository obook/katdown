// imagepaste.cpp
// Paste a clipboard image into a Markdown document.
// Author: Olivier Booklage
// Date: October 2026
// License: GPL-3.0-or-later

#include "imagepaste.h"

#include <QClipboard>
#include <QDateTime>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QUrl>

#include <KLocalizedString>
#include <KTextEditor/Document>
#include <KTextEditor/View>

bool pasteClipboardImage(KTextEditor::View *view, QString *error)
{
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    // A clipboard with text too is Kate's to paste, and a read-only document takes nothing.
    if (!mime || !mime->hasImage() || mime->hasText() || !view->document()->isReadWrite()) {
        return false;
    }
    const QUrl url = view->document()->url();
    if (!url.isLocalFile()) {
        *error = i18n("Save the document before pasting an image: the image is stored beside it.");
        return false;
    }
    // The time down to the millisecond makes each name distinct: image-20261003-104501123.png
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmsszzz"));
    const QString name = QStringLiteral("image-%1.png").arg(timestamp);
    const QString path = QFileInfo(url.toLocalFile()).absolutePath() + QLatin1Char('/') + name;
    QImage image = qvariant_cast<QImage>(mime->imageData());
    // A full-screen capture from a high-density screen weighs megabytes for nothing. Only
    // the width is capped: a tall capture of a whole page must stay readable.
    constexpr int MaxWidth = 1920;
    if (image.width() > MaxWidth) {
        image = image.scaledToWidth(MaxWidth, Qt::SmoothTransformation);
    }
    if (!image.save(path, "PNG")) {
        *error = i18n("Could not save the image to %1", path);
        return false;
    }
    // Like any paste, the link replaces the selected text.
    if (view->selection()) {
        view->removeSelectionText();
    }
    // Leave the cursor between the brackets, ready for the alternative text.
    const KTextEditor::Cursor start = view->cursorPosition();
    view->insertText(QStringLiteral("![](%1)").arg(name));
    view->setCursorPosition(KTextEditor::Cursor(start.line(), start.column() + 2));
    return true;
}
