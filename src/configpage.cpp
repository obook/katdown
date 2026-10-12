// configpage.cpp
// ConfigPage: the widgets of the settings page, and the check for a newer version.
// Code by uwuclxdy and Olivier Booklage.
// License: GPL-3.0-or-later

#include "configpage.h"
#include "settings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QVersionNumber>

#include <KLocalizedString>

namespace
{
const QString RepoUrl = QStringLiteral("https://github.com/obook/katemark");
const QString LatestReleaseApiUrl = QStringLiteral("https://api.github.com/repos/obook/katemark/releases/latest");
const QString InstalledVersion = QStringLiteral(KATEMARK_VERSION);

// Four lines are enough to show a few macros; the field scrolls beyond that.
constexpr int MacroLinesShown = 4;
// Height added to those lines for the margins and the frame of the field.
constexpr int MacroFieldPaddingPx = 12;
} // namespace

ConfigPage::ConfigPage(QWidget *parent)
    : KTextEditor::ConfigPage(parent)
{
    auto *layout = new QVBoxLayout(this);
    addColorSettings(layout);
    addContentSettings(layout);
    addUpdateCheck(layout);
    layout->addStretch();

    reset();

    connect(m_remoteMedia, &QCheckBox::toggled, this, [this]() {
        Q_EMIT changed();
    });
    connect(m_macros, &QPlainTextEdit::textChanged, this, [this]() {
        Q_EMIT changed();
    });
    connect(m_colorSource, &QComboBox::currentIndexChanged, this, [this]() {
        syncEnabled();
        Q_EMIT changed();
    });
    connect(m_variant, &QComboBox::currentIndexChanged, this, [this]() {
        Q_EMIT changed();
    });
    connect(m_checkButton, &QPushButton::clicked, this, &ConfigPage::checkForUpdates);
}

// Where the colors come from, and which of GitHub's two palettes when they are GitHub's.
void ConfigPage::addColorSettings(QVBoxLayout *layout)
{
    auto *form = new QFormLayout();
    layout->addLayout(form);

    m_colorSource = new QComboBox(this);
    m_colorSource->addItem(i18n("GitHub"), Settings::GitHub);
    m_colorSource->addItem(i18n("Match editor / system theme"), Settings::Application);
    form->addRow(i18n("Style:"), m_colorSource);

    m_variant = new QComboBox(this);
    m_variant->addItem(i18n("Follow system (auto)"), Settings::Auto);
    m_variant->addItem(i18n("Light"), Settings::Light);
    m_variant->addItem(i18n("Dark"), Settings::Dark);
    form->addRow(i18n("GitHub variant:"), m_variant);

    auto *hint = new QLabel(
        i18n("\"GitHub\" uses GitHub's own colors. \"Match editor / system theme\" recolors the "
             "same layout from the active editor theme, so the preview blends with the rest of Kate."),
        this);
    hint->setWordWrap(true);
    hint->setEnabled(false);
    layout->addWidget(hint);
}

// Pictures from the web, and the math macros.
void ConfigPage::addContentSettings(QVBoxLayout *layout)
{
    // src/previewwidget.cpp shows the same text in place of a picture that is kept out.
    m_remoteMedia = new QCheckBox(i18n("Load media previews from remote URLs"), this);
    layout->addWidget(m_remoteMedia);

    auto *remoteHint = new QLabel(i18n("Off by default so the preview stays fully offline. Enable to let images load over http(s)."), this);
    remoteHint->setWordWrap(true);
    remoteHint->setEnabled(false);
    layout->addWidget(remoteHint);

    layout->addWidget(new QLabel(i18n("Math macros:"), this));
    m_macros = new QPlainTextEdit(this);
    m_macros->setPlaceholderText(QStringLiteral("\\newcommand{\\vect}[1]{\\overrightarrow{#1}}"));
    m_macros->setFixedHeight(MacroLinesShown * m_macros->fontMetrics().lineSpacing() + MacroFieldPaddingPx);
    layout->addWidget(m_macros);

    auto *macrosHint = new QLabel(i18n("The \\newcommand lines written here are available in the formulas of every document."), this);
    macrosHint->setWordWrap(true);
    macrosHint->setEnabled(false);
    layout->addWidget(macrosHint);
}

// The button that looks for a newer version, and the text that tells what it found.
void ConfigPage::addUpdateCheck(QVBoxLayout *layout)
{
    m_net = new QNetworkAccessManager(this);

    auto *updateRow = new QHBoxLayout();
    m_checkButton = new QPushButton(i18n("Check for updates"), this);
    m_checkButton->setIcon(QIcon::fromTheme(QStringLiteral("system-software-update")));
    updateRow->addWidget(m_checkButton);
    m_updateStatus = new QLabel(i18n("Installed version: %1", InstalledVersion), this);
    m_updateStatus->setTextFormat(Qt::RichText);
    m_updateStatus->setOpenExternalLinks(true);
    m_updateStatus->setTextInteractionFlags(Qt::TextBrowserInteraction);
    updateRow->addWidget(m_updateStatus, 1);
    layout->addLayout(updateRow);
}

QString ConfigPage::name() const
{
    return i18n("Katemark");
}

QString ConfigPage::fullName() const
{
    return i18n("Katemark");
}

QIcon ConfigPage::icon() const
{
    return QIcon::fromTheme(QStringLiteral("text-markdown"), QIcon::fromTheme(QStringLiteral("view-preview")));
}

void ConfigPage::syncEnabled()
{
    const bool github = m_colorSource->currentData().toInt() == Settings::GitHub;
    m_variant->setEnabled(github);
}

void ConfigPage::checkForUpdates()
{
    m_checkButton->setEnabled(false);
    m_updateStatus->setText(i18n("Checking..."));

    QNetworkRequest request{QUrl(LatestReleaseApiUrl)};
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "katemark");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_checkButton->setEnabled(true);

        if (reply->error() != QNetworkReply::NoError) {
            m_updateStatus->setText(i18n("Update check failed: %1", reply->errorString()));
            return;
        }

        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = release.value(QStringLiteral("tag_name")).toString();
        if (tag.isEmpty()) {
            m_updateStatus->setText(i18n("Update check failed: no published release found."));
            return;
        }

        // /releases/latest never names a pre-release, so the tag is a plain "v1.2.3".
        const QString latest = tag.startsWith(QLatin1Char('v')) ? tag.mid(1) : tag;
        if (QVersionNumber::fromString(latest) > QVersionNumber::fromString(InstalledVersion)) {
            QString url = release.value(QStringLiteral("html_url")).toString();
            if (url.isEmpty()) {
                url = RepoUrl + QStringLiteral("/releases/latest");
            }
            m_updateStatus->setText(i18n("Update available: %1 (installed %2). <a href=\"%3\">Release notes</a>", tag, InstalledVersion, url));
        } else {
            m_updateStatus->setText(i18n("Up to date (installed %1).", InstalledVersion));
        }
    });
}

void ConfigPage::apply()
{
    Settings *s = Settings::self();
    s->setColorSource(static_cast<Settings::ColorSource>(m_colorSource->currentData().toInt()));
    s->setGithubVariant(static_cast<Settings::GithubVariant>(m_variant->currentData().toInt()));
    s->setLoadRemoteMedia(m_remoteMedia->isChecked());
    s->setMathMacros(m_macros->toPlainText());
}

void ConfigPage::reset()
{
    Settings *s = Settings::self();
    m_colorSource->setCurrentIndex(m_colorSource->findData(s->colorSource()));
    m_variant->setCurrentIndex(m_variant->findData(s->githubVariant()));
    m_remoteMedia->setChecked(s->loadRemoteMedia());
    m_macros->setPlainText(s->mathMacros());
    syncEnabled();
}

void ConfigPage::defaults()
{
    m_colorSource->setCurrentIndex(m_colorSource->findData(Settings::GitHub));
    m_variant->setCurrentIndex(m_variant->findData(Settings::Auto));
    m_remoteMedia->setChecked(false);
    m_macros->clear();
    syncEnabled();
}
