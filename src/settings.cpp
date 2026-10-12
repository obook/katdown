// settings.cpp
// Settings: reading the settings from Kate's configuration file and writing them back.
// Code by uwuclxdy and Olivier Booklage.
// License: GPL-3.0-or-later

#include "settings.h"

#include <KConfigGroup>
#include <KSharedConfig>

namespace
{
// The group of the configuration file that holds the settings.
const QString GroupName = QStringLiteral("Katemark");
} // namespace

Settings::Settings(QObject *parent)
    : QObject(parent)
{
    load();
}

Settings *Settings::self()
{
    static Settings instance;
    return &instance;
}

void Settings::load()
{
    KConfigGroup cfg(KSharedConfig::openConfig(), GroupName);
    const QString source = cfg.readEntry("Theme", QStringLiteral("github"));
    m_colorSource = (source == QLatin1String("application")) ? Application : GitHub;

    const QString variant = cfg.readEntry("GithubVariant", QStringLiteral("auto"));
    if (variant == QLatin1String("light")) {
        m_githubVariant = Light;
    } else if (variant == QLatin1String("dark")) {
        m_githubVariant = Dark;
    } else {
        m_githubVariant = Auto;
    }
    m_loadRemoteMedia = cfg.readEntry("LoadRemoteMedia", false);
    m_mathMacros = cfg.readEntry("MathMacros", QString());
    m_githubOnly = cfg.readEntry("GithubOnly", false);
}

void Settings::save() const
{
    KConfigGroup cfg(KSharedConfig::openConfig(), GroupName);
    cfg.writeEntry("Theme", m_colorSource == Application ? QStringLiteral("application") : QStringLiteral("github"));
    QString variant = QStringLiteral("auto");
    if (m_githubVariant == Light) {
        variant = QStringLiteral("light");
    } else if (m_githubVariant == Dark) {
        variant = QStringLiteral("dark");
    }
    cfg.writeEntry("GithubVariant", variant);
    cfg.writeEntry("LoadRemoteMedia", m_loadRemoteMedia);
    cfg.writeEntry("MathMacros", m_mathMacros);
    cfg.writeEntry("GithubOnly", m_githubOnly);
    cfg.sync();
}

template<typename T>
void Settings::store(T &member, const T &value)
{
    if (member == value) {
        return;
    }
    member = value;
    save();
    Q_EMIT changed();
}

void Settings::setColorSource(ColorSource source)
{
    store(m_colorSource, source);
}

void Settings::setGithubVariant(GithubVariant variant)
{
    store(m_githubVariant, variant);
}

void Settings::setLoadRemoteMedia(bool enabled)
{
    store(m_loadRemoteMedia, enabled);
}

void Settings::setMathMacros(const QString &macros)
{
    store(m_mathMacros, macros);
}

void Settings::setGithubOnly(bool enabled)
{
    store(m_githubOnly, enabled);
}
