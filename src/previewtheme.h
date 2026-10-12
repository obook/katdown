// previewtheme.h
// Colors of the preview: GitHub's palettes, or values derived from the editor theme.
// Code by uwuclxdy, moved out of previewwidget.cpp.
// License: GPL-3.0-or-later

#pragma once

#include <QJsonObject>
#include <QString>

namespace KSyntaxHighlighting
{
class Theme;
}

// True when the application's palette is a dark one.
bool paletteIsDark();

// True when the settings ask for GitHub's dark palette, or leave it to a dark application.
bool githubLookIsDark();

// GitHub's own CSS variables, for its light or its dark palette.
QJsonObject githubVars(bool dark);

// The same CSS variables derived from an editor theme; outDark tells which kind it is.
QJsonObject applicationVars(const KSyntaxHighlighting::Theme &theme, bool &outDark);

// A highlight.js stylesheet with the token colors of an editor theme.
QString applicationCodeCss(const KSyntaxHighlighting::Theme &theme);
