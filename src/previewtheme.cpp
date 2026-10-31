// previewtheme.cpp
// Colors of the preview, and how PreviewWidget applies them to the page.
// Code by uwuclxdy, moved out of previewwidget.cpp.
// License: GPL-3.0-or-later

#include "previewtheme.h"
#include "previewutil.h"
#include "previewwidget.h"
#include "settings.h"

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QPalette>
#include <QWebEnginePage>
#include <QWebEngineView>

#include <KSyntaxHighlighting/Theme>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

using Theme = KSyntaxHighlighting::Theme;

namespace
{
QColor colorOr(QRgb rgb, const QColor &fallback)
{
    return qAlpha(rgb) == 0 ? fallback : QColor::fromRgba(rgb);
}

QString hex(const QColor &c)
{
    return c.name(QColor::HexRgb);
}

QString rgba(const QColor &c, qreal a)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a, 0, 'g', 3);
}

QColor blend(const QColor &a, const QColor &b, qreal t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

struct VarPair {
    const char *key;
    const char *value;
};

// The color Qt paints where the page shows none of its own: before the page loads, and in
// the margins of a PDF.
QColor background(const QJsonObject &vars)
{
    return QColor(vars.value(QLatin1String("--bgColor-default")).toString());
}
} // namespace

bool paletteIsDark()
{
    const QColor window = qApp->palette().color(QPalette::Active, QPalette::Window);
    return window.lightnessF() < 0.5;
}

bool githubLookIsDark()
{
    const Settings *settings = Settings::self();
    return settings->githubVariant() == Settings::Dark || (settings->githubVariant() == Settings::Auto && paletteIsDark());
}

QJsonObject githubVars(bool dark)
{
    static const VarPair darkVars[] = {
        {"--fgColor-default", "#f0f6fc"},     {"--fgColor-muted", "#9198a1"},
        {"--fgColor-accent", "#4493f8"},      {"--fgColor-danger", "#f85149"},
        {"--fgColor-success", "#3fb950"},     {"--fgColor-attention", "#d29922"},
        {"--fgColor-done", "#ab7df8"},        {"--bgColor-default", "#0d1117"},
        {"--bgColor-muted", "#151b23"},       {"--bgColor-neutral-muted", "#656c7633"},
        {"--bgColor-attention-muted", "#bb800926"},
        {"--borderColor-default", "#3d444d"}, {"--borderColor-muted", "#3d444db3"},
        {"--borderColor-neutral-muted", "#3d444db3"},
        {"--borderColor-accent-emphasis", "#1f6feb"},
        {"--borderColor-attention-emphasis", "#9e6a03"},
        {"--borderColor-danger-emphasis", "#da3633"},
        {"--borderColor-success-emphasis", "#238636"},
        {"--borderColor-done-emphasis", "#8957e5"},
    };
    static const VarPair lightVars[] = {
        {"--fgColor-default", "#1f2328"},     {"--fgColor-muted", "#59636e"},
        {"--fgColor-accent", "#0969da"},      {"--fgColor-danger", "#d1242f"},
        {"--fgColor-success", "#1a7f37"},     {"--fgColor-attention", "#9a6700"},
        {"--fgColor-done", "#8250df"},        {"--bgColor-default", "#ffffff"},
        {"--bgColor-muted", "#f6f8fa"},       {"--bgColor-neutral-muted", "#818b981f"},
        {"--bgColor-attention-muted", "#fff8c5"},
        {"--borderColor-default", "#d1d9e0"}, {"--borderColor-muted", "#d1d9e0b3"},
        {"--borderColor-neutral-muted", "#d1d9e0b3"},
        {"--borderColor-accent-emphasis", "#0969da"},
        {"--borderColor-attention-emphasis", "#9a6700"},
        {"--borderColor-danger-emphasis", "#cf222e"},
        {"--borderColor-success-emphasis", "#1a7f37"},
        {"--borderColor-done-emphasis", "#8250df"},
    };

    QJsonObject vars;
    const VarPair *table = dark ? darkVars : lightVars;
    const size_t count = sizeof(darkVars) / sizeof(darkVars[0]); // both tables are the same length
    for (size_t i = 0; i < count; ++i) {
        vars.insert(QString::fromLatin1(table[i].key), QString::fromLatin1(table[i].value));
    }
    return vars;
}

// Derive github-markdown CSS variables from the active editor theme + palette.
QJsonObject applicationVars(const Theme &theme, bool &outDark)
{
    const QColor bg = colorOr(theme.editorColor(Theme::BackgroundColor), qApp->palette().color(QPalette::Base));
    const QColor fg = colorOr(theme.textColor(Theme::Normal), qApp->palette().color(QPalette::Text));
    outDark = bg.lightnessF() < 0.5;

    QColor link = qApp->palette().color(QPalette::Active, QPalette::Link);
    if (!link.isValid()) {
        link = colorOr(theme.textColor(Theme::Function), fg);
    }
    const QColor border = blend(bg, fg, 0.18);
    const QColor danger = colorOr(theme.textColor(Theme::Error), QColor(0xCF, 0x22, 0x2E));
    const QColor attention = colorOr(theme.textColor(Theme::Warning), QColor(0x9A, 0x67, 0x00));
    const QColor success = colorOr(theme.textColor(Theme::String), QColor(0x1A, 0x7F, 0x37));
    const QColor done = colorOr(theme.textColor(Theme::Keyword), link);

    QJsonObject vars;
    vars.insert(QStringLiteral("--fgColor-default"), hex(fg));
    vars.insert(QStringLiteral("--fgColor-muted"), hex(blend(fg, bg, 0.45)));
    vars.insert(QStringLiteral("--fgColor-accent"), hex(link));
    vars.insert(QStringLiteral("--fgColor-danger"), hex(danger));
    vars.insert(QStringLiteral("--fgColor-success"), hex(success));
    vars.insert(QStringLiteral("--fgColor-attention"), hex(attention));
    vars.insert(QStringLiteral("--fgColor-done"), hex(done));
    vars.insert(QStringLiteral("--bgColor-default"), hex(bg));
    vars.insert(QStringLiteral("--bgColor-muted"), hex(blend(bg, fg, 0.04)));
    vars.insert(QStringLiteral("--bgColor-neutral-muted"), rgba(fg, outDark ? 0.14 : 0.08));
    vars.insert(QStringLiteral("--bgColor-attention-muted"), rgba(attention, 0.15));
    vars.insert(QStringLiteral("--borderColor-default"), hex(border));
    vars.insert(QStringLiteral("--borderColor-muted"), rgba(border, 0.7));
    vars.insert(QStringLiteral("--borderColor-neutral-muted"), rgba(border, 0.7));
    vars.insert(QStringLiteral("--borderColor-accent-emphasis"), hex(link));
    vars.insert(QStringLiteral("--borderColor-attention-emphasis"), hex(attention));
    vars.insert(QStringLiteral("--borderColor-danger-emphasis"), hex(danger));
    vars.insert(QStringLiteral("--borderColor-success-emphasis"), hex(success));
    vars.insert(QStringLiteral("--borderColor-done-emphasis"), hex(done));
    return vars;
}

// Build a highlight.js token stylesheet from the active editor theme so code
// blocks render with the same colors as the editor.
QString applicationCodeCss(const Theme &theme)
{
    const QColor fg = colorOr(theme.textColor(Theme::Normal), qApp->palette().color(QPalette::Text));
    auto tokenColor = [&](Theme::TextStyle style) {
        return hex(colorOr(theme.textColor(style), fg));
    };

    QString css;
    css += QStringLiteral(".hljs{color:%1;background:transparent}").arg(hex(fg));
    css += QStringLiteral(".hljs-comment,.hljs-quote{color:%1;font-style:italic}").arg(tokenColor(Theme::Comment));
    css += QStringLiteral(".hljs-doctag,.hljs-meta .hljs-doctag{color:%1}").arg(tokenColor(Theme::Documentation));
    css += QStringLiteral(".hljs-keyword,.hljs-selector-tag,.hljs-literal,.hljs-section,.hljs-name,.hljs-tag,.hljs-meta "
                          ".hljs-keyword{color:%1}")
               .arg(tokenColor(Theme::Keyword));
    css += QStringLiteral(".hljs-built_in,.hljs-builtin-name,.hljs-title.class_,.hljs-class .hljs-title{color:%1}").arg(tokenColor(Theme::BuiltIn));
    css += QStringLiteral(".hljs-type,.hljs-class{color:%1}").arg(tokenColor(Theme::DataType));
    css += QStringLiteral(".hljs-string,.hljs-regexp,.hljs-addition,.hljs-meta .hljs-string{color:%1}").arg(tokenColor(Theme::String));
    css += QStringLiteral(".hljs-number{color:%1}").arg(tokenColor(Theme::DecVal));
    css += QStringLiteral(".hljs-symbol,.hljs-bullet,.hljs-link,.hljs-char,.hljs-char.escape_{color:%1}").arg(tokenColor(Theme::Char));
    css += QStringLiteral(".hljs-title,.hljs-title.function_,.hljs-function .hljs-title{color:%1}").arg(tokenColor(Theme::Function));
    css += QStringLiteral(".hljs-variable,.hljs-template-variable,.hljs-attr,.hljs-attribute,.hljs-selector-attr,.hljs-selector-class,"
                          ".hljs-selector-id{color:%1}")
               .arg(tokenColor(Theme::Variable));
    css += QStringLiteral(".hljs-meta{color:%1}").arg(tokenColor(Theme::Preprocessor));
    css += QStringLiteral(".hljs-deletion{color:%1}").arg(tokenColor(Theme::Error));
    css += QStringLiteral(".hljs-emphasis{font-style:italic}.hljs-strong{font-weight:bold}");
    return css;
}

// The colors of the look in use, GitHub's or the editor's, and whether they are dark.
QJsonObject PreviewWidget::lookVars(bool &dark) const
{
    if (Settings::self()->colorSource() == Settings::GitHub) {
        dark = githubLookIsDark();
        return githubVars(dark);
    }
    return applicationVars(m_view ? m_view->theme() : KTextEditor::Editor::instance()->theme(), dark);
}

void PreviewWidget::applyTheme()
{
    // An export keeps GitHub's light look until it ends; endExport() calls this again.
    if (!m_loaded || isExporting()) {
        return;
    }
    bool dark = false;
    applyLook(lookVars(dark), dark);
}

// Colors of the page and of its code: GitHub's code theme with GitHub's colors (always
// during an export), one made from the editor's theme with the editor's colors.
void PreviewWidget::applyLook(const QJsonObject &vars, bool dark)
{
    m_web->page()->setBackgroundColor(background(vars));
    runJs(QStringLiteral("window.__setColorScheme(%1);").arg(dark ? QStringLiteral("true") : QStringLiteral("false")));
    runJs(QStringLiteral("window.__applyVars(%1);").arg(compactJson(vars)));
    if (isExporting() || Settings::self()->colorSource() == Settings::GitHub) {
        runJs(QStringLiteral("window.__useHljsTheme('%1');").arg(dark ? QStringLiteral("github-dark") : QStringLiteral("github")));
    } else {
        const Theme theme = m_view ? m_view->theme() : KTextEditor::Editor::instance()->theme();
        runJs(QStringLiteral("window.__setCodeCss(%1);").arg(jsLiteral(applicationCodeCss(theme))));
    }
}

// The background of the look in use, known before the page is: its scripts set the colors
// only once it has loaded.
QColor PreviewWidget::pageBackground() const
{
    bool dark = false;
    return background(lookVars(dark));
}

void PreviewWidget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ThemeChange) {
        applyTheme();
    }
}
