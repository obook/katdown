// previewpage.h
// The two guards around the web page: which requests it may make, and what a click
// on a link does.
// Code by uwuclxdy, moved out of previewwidget.cpp.
// License: GPL-3.0-or-later

#pragma once

#include "previewutil.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QUrl>
#include <QWebEnginePage>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestInterceptor>

#include <functional>

// Confine the page to its document folder: a malicious markdown (raw HTML + JS on a
// file:// origin) could otherwise read any local file via fetch/img/etc. Only assets
// under the canonical document root pass; remote requests are limited to images/media
// and only when the user opted in.
class LocalFileGuard : public QWebEngineUrlRequestInterceptor
{
public:
    using QWebEngineUrlRequestInterceptor::QWebEngineUrlRequestInterceptor;

    void setRoot(const QString &dir)
    {
        m_root = dir.isEmpty() ? QString() : QDir(dir).canonicalPath();
    }

    void setAllowRemote(bool allow)
    {
        m_allowRemote = allow;
    }

    // Whether a local file lies in the document folder. The HTML export asks too, before
    // it copies a picture into the exported file.
    bool allows(const QString &localFile) const
    {
        // Refused before any access to the disk, which would reach the other machine.
        // shortcut: a document opened from a network path loses its pictures too. Allow
        // the share of the document folder if that is ever needed.
        if (isNetworkPath(localFile)) {
            return false;
        }
        const QString path = QFileInfo(localFile).canonicalFilePath();
        return !m_root.isEmpty() && !path.isEmpty() && (path == m_root || path.startsWith(m_root + QLatin1Char('/')));
    }

    void interceptRequest(QWebEngineUrlRequestInfo &info) override
    {
        const QUrl url = info.requestUrl();
        const QString scheme = url.scheme();
        if (scheme == QLatin1String("qrc") || scheme == QLatin1String("data") || scheme == QLatin1String("about") || scheme == QLatin1String("blob")) {
            return;
        }
        if (scheme == QLatin1String("file")) {
            if (!allows(url.toLocalFile())) {
                info.block(true);
            }
            return;
        }
        const bool isMedia = info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeImage
            || info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMedia;
        if (!(m_allowRemote && isMedia)) {
            info.block(true);
        }
    }

private:
    QString m_root;
    bool m_allowRemote = false;
};

// Keep the preview from turning into a browser: same-document anchors scroll in place,
// other clicked links are handed back to the host via onLinkActivated, and nothing else
// may take the page elsewhere.
class PreviewPage : public QWebEnginePage
{
public:
    using QWebEnginePage::QWebEnginePage;

    std::function<void(const QUrl &)> onLinkActivated;

    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override
    {
        Q_UNUSED(isMainFrame);
        if (type == NavigationTypeLinkClicked) {
            if (url.hasFragment() && url.matches(this->url(), QUrl::RemoveFragment)) {
                return true;
            }
            if (onLinkActivated) {
                QMetaObject::invokeMethod(
                    this,
                    [callback = onLinkActivated, target = url]() {
                        callback(target);
                    },
                    Qt::QueuedConnection);
            }
            return false;
        }
        // Typed is the page itself, loaded by setHtml(). The rest (a redirect, a form, a
        // reload) is not asked by the preview: <meta http-equiv="refresh"> in a document is.
        return type == NavigationTypeTyped;
    }
};
