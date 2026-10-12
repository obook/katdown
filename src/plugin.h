// plugin.h
// KatemarkPlugin: the object Kate loads.
// Code by uwuclxdy.
// License: GPL-3.0-or-later

#pragma once

#include <KTextEditor/Plugin>

/**
 * Entry point for the Katemark plugin. Creates a per-main-window view and
 * exposes one configuration page.
 */
class KatemarkPlugin : public KTextEditor::Plugin
{
    Q_OBJECT
public:
    explicit KatemarkPlugin(QObject *parent, const QVariantList &args = QVariantList());

    QObject *createView(KTextEditor::MainWindow *mainWindow) override;

    int configPages() const override;
    KTextEditor::ConfigPage *configPage(int number, QWidget *parent) override;
};
