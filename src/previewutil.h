// previewutil.h
// Small helpers shared by the source files of the preview.
// Code by uwuclxdy, moved out of previewwidget.cpp.
// License: GPL-3.0-or-later

#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QUrl>

inline QString readAsset(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return QString();
    }
    return QString::fromUtf8(f.readAll());
}

// Make an arbitrary string safe to inline inside a <script> block.
inline QString shieldScript(QString js)
{
    return js.replace(QLatin1String("</script"), QLatin1String("<\\/script"), Qt::CaseInsensitive);
}

// Encode a string as a JavaScript string literal (incl. surrounding quotes).
inline QString jsLiteral(const QString &s)
{
    const QJsonDocument doc(QJsonArray{s});
    const QByteArray json = doc.toJson(QJsonDocument::Compact); // ["..."]
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}

inline QString compactJson(const QJsonObject &obj)
{
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

// The file a clicked link means. The page resolves every link against the folder of the
// document (base), so "~/notes" arrives as "<folder>/~/notes", and a wiki link written
// without its extension names a file that is not there.
inline QUrl localLinkTarget(const QUrl &url, const QUrl &base)
{
    QString path = url.toLocalFile();
    if (QFileInfo::exists(path)) {
        return url;
    }
    const QString tilde = base.toLocalFile() + QLatin1String("~/");
    if (path.startsWith(tilde)) {
        path = QDir::homePath() + QLatin1Char('/') + path.mid(tilde.size());
    }
    if (!QFileInfo::exists(path) && QFileInfo::exists(path + QLatin1String(".md"))) {
        path += QLatin1String(".md");
    }
    QUrl target = QUrl::fromLocalFile(path);
    target.setFragment(url.fragment());
    return target;
}

// What to show while the pointer is on a link: "#heading" for a heading of this page,
// the path of a local file, the address of anything else.
inline QString linkHint(const QUrl &url, const QUrl &base)
{
    if (url.hasFragment() && url.matches(base, QUrl::RemoveFragment)) {
        return QLatin1Char('#') + url.fragment();
    }
    if (url.isLocalFile()) {
        return localLinkTarget(url, base).toDisplayString(QUrl::PreferLocalFile);
    }
    return url.toDisplayString();
}
