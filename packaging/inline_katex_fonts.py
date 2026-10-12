#!/usr/bin/env python3
"""
File: inline_katex_fonts.py
Description: Rewrite KaTeX's stylesheet so that its fonts are embedded
    as data: URIs. The preview page runs on the document's file://
    origin and Chromium refuses to load fonts from another origin
    (qrc:), so the fonts cannot ship as separate resources.
Author: Olivier Booklage
Date: October 2026
License: GPL-3.0-or-later

Usage: inline_katex_fonts.py <katex/dist> > data/css/katex.min.css
"""
import base64
import pathlib
import re
import sys

# KaTeX declares each font three times (woff2, woff, ttf). Every browser
# engine Qt ships reads woff2, so only that one is kept.
FONT_DECLARATION = r"src:url\((fonts/[^)]+\.woff2)\)[^;}]*"


def main():
    """Print KaTeX's stylesheet with its fonts inlined."""
    katex_dist = pathlib.Path(sys.argv[1])

    # Defined here because it needs katex_dist, and re.subn only hands it
    # the match.
    def embed_font(match):
        """Return the declaration of one font with its file inlined."""
        font_path = katex_dist / match.group(1)
        encoded = base64.b64encode(font_path.read_bytes()).decode()
        return 'src:url(data:font/woff2;base64,%s) format("woff2")' % encoded

    stylesheet = (katex_dist / "katex.min.css").read_text(encoding="utf-8")
    stylesheet, font_count = re.subn(FONT_DECLARATION, embed_font, stylesheet)

    # Stop rather than write a stylesheet that would still point at font
    # files.
    if font_count == 0 or "url(fonts/" in stylesheet:
        sys.exit("KaTeX changed how it declares its fonts")

    sys.stdout.write(stylesheet)


if __name__ == "__main__":
    main()
