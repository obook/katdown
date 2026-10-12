// configpage.h
// ConfigPage: the page of the plugin in Kate's configuration dialog.
// Code by uwuclxdy and Olivier Booklage.
// License: GPL-3.0-or-later

#pragma once

#include <KTextEditor/ConfigPage>

class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QNetworkAccessManager;
class QVBoxLayout;

/**
 * Settings page shown under Kate's plugin configuration: the colors of the preview,
 * pictures from the web, the math macros and the check for a newer version.
 */
class ConfigPage : public KTextEditor::ConfigPage
{
    Q_OBJECT
public:
    explicit ConfigPage(QWidget *parent = nullptr);

    QString name() const override;
    QString fullName() const override;
    QIcon icon() const override;

    void apply() override;
    void reset() override;
    void defaults() override;

private:
    void addColorSettings(QVBoxLayout *layout);
    void addContentSettings(QVBoxLayout *layout);
    void addUpdateCheck(QVBoxLayout *layout);
    void syncEnabled();
    void checkForUpdates();

    QComboBox *m_colorSource = nullptr;
    QComboBox *m_variant = nullptr;
    QCheckBox *m_remoteMedia = nullptr;
    QPlainTextEdit *m_macros = nullptr;
    QPushButton *m_checkButton = nullptr;
    QLabel *m_updateStatus = nullptr;
    QNetworkAccessManager *m_net = nullptr;
};
