// settings.h
// Settings: the settings of the plugin, shared by every preview.
// Code by uwuclxdy and Olivier Booklage.
// License: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

/**
 * Plugin-wide settings, persisted in the application's KSharedConfig under the
 * group "Katemark". A single instance broadcasts changed() so every open
 * preview re-themes at once.
 */
class Settings : public QObject
{
    Q_OBJECT
public:
    enum ColorSource {
        GitHub,      // GitHub's own light/dark palette
        Application, // match the active editor/system theme
    };
    enum GithubVariant {
        Auto,  // follow the application palette (dark vs light)
        Light,
        Dark,
    };

    static Settings *self();

    ColorSource colorSource() const
    {
        return m_colorSource;
    }
    GithubVariant githubVariant() const
    {
        return m_githubVariant;
    }
    bool loadRemoteMedia() const
    {
        return m_loadRemoteMedia;
    }
    // Render only what GitHub renders: no CodiMD, Obsidian or MkDocs syntax.
    bool githubOnly() const
    {
        return m_githubOnly;
    }
    // \newcommand lines defined before every document.
    QString mathMacros() const
    {
        return m_mathMacros;
    }

    void setColorSource(ColorSource source);
    void setGithubVariant(GithubVariant variant);
    void setLoadRemoteMedia(bool enabled);
    void setMathMacros(const QString &macros);
    void setGithubOnly(bool enabled);

    void load();
    void save() const;

Q_SIGNALS:
    void changed();

private:
    explicit Settings(QObject *parent = nullptr);

    // What every setter does: keep a new value, save it and announce the change.
    template<typename T>
    void store(T &member, const T &value);

    ColorSource m_colorSource = GitHub;
    GithubVariant m_githubVariant = Auto;
    bool m_loadRemoteMedia = false;
    QString m_mathMacros;
    bool m_githubOnly = false;
};
