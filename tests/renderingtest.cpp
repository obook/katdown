// renderingtest.cpp
// What the page makes of each syntax: the markdown-it plugins, CodiMD, Obsidian, math written
// for MathJax, and the ordinary Markdown these additions must leave alone.
// Author: Olivier Booklage
// Date: October 2026
// License: GPL-3.0-or-later

#include "testhelpers.h"

#include "settings.h"

#include <QCheckBox>
#include <QLabel>
#include <QScopeGuard>

class RenderingTest : public PreviewTest
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        createFixture();
    }
    void rendersCodimdExtensions();
    void rendersObsidianSyntax();
    void rendersMathLikeMathJax();
    void keepsOrdinaryMarkdownIntact();
    void keepsFoldingAndUniqueIds();
    void rendersMathExtras();
    void rendersMkdocsAdmonitions();
    void rendersGithubOnly();
    void agreesWithGithubOnDatesAndAnchors();
    void keepsMathOnItsFeet();
    void saysWhenARenderTakesLong();
};

// The libraries tested here come from qrc rather than the inlined page: the markdown-it plugins
// and KaTeX at page start, Mermaid on demand. A library that failed to load leaves its
// markup unrendered, which is what each selector here would miss.
void RenderingTest::rendersCodimdExtensions()
{
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));

    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    doc->setText(QStringLiteral(":::info\nthe boxed line\n:::\n\n==marked== and $a^2$\n\n> :warning: mind `:smile:` in code\n\n```mermaid\ngraph LR\n  A --> B\n```\n"));
    QVERIFY(waitForPageText(preview.get(), QLatin1String("the boxed line")));
    QCOMPARE(evalJs(preview.get(), QStringLiteral("document.querySelectorAll('.alert.alert-info, mark, .katex').length")), QStringLiteral("3"));
    // An emoji shortcode becomes the emoji, except inside code.
    const QString quote = evalJs(preview.get(), QStringLiteral("document.querySelector('blockquote').textContent"));
    QVERIFY2(quote.contains(QChar(0x26A0)) && !quote.contains(QLatin1String(":warning:")), qPrintable(quote));
    QVERIFY2(quote.contains(QLatin1String(":smile:")), qPrintable(quote));
    QTRY_COMPARE_WITH_TIMEOUT(evalJs(preview.get(), QStringLiteral("document.querySelectorAll('pre.mermaid svg').length")), QStringLiteral("1"), PageTimeoutMs);

    // Ctrl + wheel over the diagram zooms it.
    const QString width = QStringLiteral("document.querySelector('pre.mermaid svg').getBoundingClientRect().width");
    const double before = evalJs(preview.get(), width).toDouble();
    evalJs(preview.get(),
           QStringLiteral("document.querySelector('pre.mermaid svg').dispatchEvent("
                          "new WheelEvent('wheel', { deltaY: -100, ctrlKey: true, bubbles: true, cancelable: true })); 1"));
    QVERIFY(evalJs(preview.get(), width).toDouble() > before);

    delete doc;
}

// Obsidian's additions: callouts with a type, a title and folding, wiki links, image
// embeds with a width, and comments that stay out of the page.
void RenderingTest::rendersObsidianSyntax()
{
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    doc->setText(QStringLiteral("> [!faq]- Why *now*?\n> the folded answer\n\n> [!bug]\n> the bug line\n\n"
                                "See [[Other Note|the other]] and ![[%1|40]] %%hidden words%% end.\n\n%%\na hidden block\n%%\n\nlast line\n")
                     .arg(ImageName));
    QVERIFY(waitForPageText(preview.get(), QLatin1String("last line")));
    QCOMPARE(countElements(preview.get(), "details.markdown-alert:not([open]) > summary.markdown-alert-title > em"), 1);
    QCOMPARE(countElements(preview.get(), "div.markdown-alert.markdown-alert-caution > p.markdown-alert-title"), 1);
    QCOMPARE(countElements(preview.get(), "a.wikilink[href=\"Other%20Note.md\"]"), 1);
    QCOMPARE(countElements(preview.get(), "img[width=\"40\"]"), 1);
    QCOMPARE(waitForImageWidth(preview.get()), ImageWidth);
    const QString text = evalJs(preview.get(), QStringLiteral("document.getElementById('content').textContent"));
    QVERIFY(text.contains(QLatin1String("Bug")));
    QVERIFY(text.contains(QLatin1String("the other")));
    QVERIFY2(!text.contains(QLatin1String("hidden")), qPrintable(text));

    delete doc;
}

// Formulas taken from documents written for CodiMD, whose MathJax accepts what KaTeX
// rejects: \require, inline math over several lines, a stray "$" inside display math.
void RenderingTest::rendersMathLikeMathJax()
{
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    doc->setText(QStringLiteral("struck $\\require{cancel} \\cancel{5}$\n\n"
                                "$\\begin{matrix}\n1 & 2 & 3\\\\\na & b & c\n\\end{matrix}$\n\n"
                                "$$\n\\begin{align*}\n&D=2x+6$ \\qquad & E=25x+10 \\\\\n&F=x^2-3x \\qquad & G=x^2\n\\end{align*}\n$$\n\nthe last line\n"));
    QVERIFY(waitForPageText(preview.get(), QLatin1String("the last line")));
    QCOMPARE(countElements(preview.get(), ".katex"), 3);
    QCOMPARE(countElements(preview.get(), ".katex-error, .katex [style*=\"color:#cc0000\"]"), 0);
    QCOMPARE(countElements(preview.get(), ".katex svg line"), 1); // the stroke of \\cancel
    QCOMPARE(countElements(preview.get(), ".katex .mtable"), 2);
    QCOMPARE(countElements(preview.get(), ".katex-display"), 1);
    // Nothing of the formulas may be left over as plain text around them.
    const QString around = evalJs(preview.get(),
                                  QStringLiteral("(function () { var c = document.getElementById('content').cloneNode(true);"
                                                 " c.querySelectorAll('.katex').forEach(function (e) { e.remove(); }); return c.textContent; })()"));
    QVERIFY2(!around.contains(QLatin1Char('$')) && !around.contains(QLatin1Char('\\')), qPrintable(around));

    delete doc;
}

// The added syntaxes must not eat ordinary Markdown: a line in parentheses under a
// formula, a link whose text is in brackets, a heading id chosen by the author, a callout
// marker ending in a hard line break, a code block without a language, a comment block
// right under a paragraph.
void RenderingTest::keepsOrdinaryMarkdownIntact()
{
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    doc->setText(QStringLiteral("$$\nx = 1\n$$\n\n(a) the kept line\n\n"
                                "See [[1]](https://example.com/ref) here.\n\n"
                                "<h2 id=\"install\">Installation</h2>\n\n## What's new?\n\n"
                                "> [!NOTE]  \n> the callout body\n\n"
                                "```\nindex.html   Point d'accès HTML\nstate.js   Gestion de l'application\n"
                                "color-mode.js   Gestion des 10 modes couleur\nui.js   interface utilisateur\n```\n\n"
                                "```js\nvar a = 'b';\n```\n\n"
                                "visible para\n%%\nhidden one\n\nhidden two\n%%\n\nafter all\n"));
    QVERIFY(waitForPageText(preview.get(), QLatin1String("after all")));
    const QString text = pageText(preview.get());
    QVERIFY2(text.contains(QLatin1String("(a) the kept line")), qPrintable(text));
    QVERIFY2(!text.contains(QLatin1String("hidden")), qPrintable(text));
    QCOMPARE(evalJs(preview.get(), QStringLiteral("document.querySelector('a[href=\"https://example.com/ref\"]').textContent")), QStringLiteral("[1]"));
    QCOMPARE(countElements(preview.get(), "h2#install"), 1);
    QCOMPARE(countElements(preview.get(), "h2#whats-new"), 1);
    QCOMPARE(evalJs(preview.get(), QStringLiteral("document.querySelector('.markdown-alert-title').textContent.trim()")), QStringLiteral("Note"));
    QCOMPARE(evalJs(preview.get(), QStringLiteral("document.querySelector('.markdown-alert > p:not(.markdown-alert-title)').textContent.trim()")),
             QStringLiteral("the callout body"));
    // A code block that names no language is left uncolored; one that names it is not.
    QCOMPARE(evalJs(preview.get(), QStringLiteral("document.querySelectorAll('pre.hljs')[0].querySelectorAll('span').length")), QStringLiteral("0"));
    QVERIFY(evalJs(preview.get(), QStringLiteral("document.querySelectorAll('pre.hljs')[1].querySelectorAll('span').length")).toInt() > 0);

    delete doc;
}

// Two things a re-render must not lose: a heading keeps an id of its own even when titles
// repeat or collide with a numbered one, and a callout the reader opened stays open.
void RenderingTest::keepsFoldingAndUniqueIds()
{
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    const QString source = QStringLiteral("## Setup\n\n## Setup\n\n## Setup 1\n\n> [!faq]- Folded\n> the answer\n\nfirst version\n");
    doc->setText(source);
    QVERIFY(waitForPageText(preview.get(), QLatin1String("first version")));
    QCOMPARE(evalJs(preview.get(), QStringLiteral("new Set([].map.call(document.querySelectorAll('h2'), function (h) { return h.id; })).size")), QStringLiteral("3"));

    QCOMPARE(countElements(preview.get(), "details[open]"), 0);
    evalJs(preview.get(), QStringLiteral("document.querySelector('details').open = true; 1"));
    doc->setText(source + QStringLiteral("\nsecond version\n"));
    QVERIFY(waitForPageText(preview.get(), QLatin1String("second version")));
    QCOMPARE(countElements(preview.get(), "details[open]"), 1);

    delete doc;
}

// Macros defined in the document or in the settings, LaTeX's own delimiters, and an
// equation numbered on the line of its closing delimiter.
void RenderingTest::rendersMathExtras()
{
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    evalJs(preview.get(), QStringLiteral("window.__setMacros('\\\\newcommand{\\\\N}{\\\\mathbb{N}}'); 1"));
    doc->setText(QStringLiteral("$\\newcommand{\\R}{\\mathbb{R}}$ then $x \\in \\R$ and $n \\in \\N$\n\n"
                                "inline \\(a+b\\) and\n\n\\[ c = d \\]\n\n$$ e = mc^2 $$ (1)\n\nthe last line\n"));
    QVERIFY(waitForPageText(preview.get(), QLatin1String("the last line")));
    // A macro KaTeX does not know is shown in red: none may be.
    QCOMPARE(countElements(preview.get(), ".katex-error, .katex [style*=\"color:#cc0000\"]"), 0);
    QCOMPARE(countElements(preview.get(), ".katex .mathbb"), 2);
    QCOMPARE(countElements(preview.get(), ".katex"), 6);
    QCOMPARE(evalJs(preview.get(), QStringLiteral("document.querySelector('section.eqno > span').textContent")), QStringLiteral("(1)"));

    delete doc;
}

// "!!!" blocks with their indented body, a folded "???" one, and "" for no title.
void RenderingTest::rendersMkdocsAdmonitions()
{
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    doc->setText(QStringLiteral("intro\n!!! note \"Custom *title*\"\n    inside the note\n\n    second paragraph\n\n"
                                "??? tip\n    folded tip body\n\n!!! danger \"\"\n    no title here\n\nthe last line\n"));
    QVERIFY(waitForPageText(preview.get(), QLatin1String("the last line")));
    QCOMPARE(countElements(preview.get(), "div.markdown-alert-note > p.markdown-alert-title > em"), 1);
    QCOMPARE(countElements(preview.get(), "div.markdown-alert-note > p:not(.markdown-alert-title)"), 2);
    QCOMPARE(countElements(preview.get(), "pre"), 0); // the indented body is not a code block
    QCOMPARE(evalJs(preview.get(), QStringLiteral("document.querySelector('details.markdown-alert-tip:not([open]) > summary').textContent.trim()")),
             QStringLiteral("Tip"));
    QCOMPARE(countElements(preview.get(), "div.markdown-alert-caution"), 1);
    QCOMPARE(countElements(preview.get(), "div.markdown-alert-caution > .markdown-alert-title"), 0);
    QCOMPARE(countElements(preview.get(), ".markdown-alert + p"), 1); // "the last line" is outside

    delete doc;
}

// "GitHub only" leaves what GitHub does not know as it is written, keeps what GitHub
// renders, and strikes text between single tildes as GitHub does. The check box of the
// bar drives it through the settings.
void RenderingTest::rendersGithubOnly()
{
    // The check box writes the setting: put it back for the tests that follow.
    const auto restore = qScopeGuard([wasGithubOnly = Settings::self()->githubOnly()]() {
        Settings::self()->setGithubOnly(wasGithubOnly);
    });

    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    doc->setText(QStringLiteral("# Title\n\n[TOC]\n\n:::info\nboxed\n:::\n\n==marked== ++inserted++ H~2~O [[Note]]\n\n"
                                "!!! note\n    admonition body\n\n> [!faq] Obsidian title\n> callout body\n\n"
                                "> [!NOTE]\n> alert body\n\n~struck out~ and $a^2$\n\nthe last line\n"));
    QVERIFY(waitForPageText(preview.get(), QLatin1String("the last line")));
    QCOMPARE(countElements(preview.get(), "ul.toc, .alert-info, mark, ins, sub"), 5);
    QCOMPARE(countElements(preview.get(), ".markdown-alert"), 3);
    QCOMPARE(countElements(preview.get(), "s"), 0);

    auto *githubOnly = preview->findChild<QCheckBox *>();
    QVERIFY(githubOnly && !githubOnly->isChecked());
    githubOnly->click();
    QVERIFY(Settings::self()->githubOnly());
    QTRY_COMPARE(countElements(preview.get(), "ul.toc, .alert-info, mark, ins, sub"), 0);
    QCOMPARE(countElements(preview.get(), ".markdown-alert"), 1); // the GitHub alert alone
    QCOMPARE(countElements(preview.get(), "s"), 2); // "2" and "struck out"
    QCOMPARE(countElements(preview.get(), ".katex"), 1);
    QCOMPARE(countElements(preview.get(), "h1#title"), 1); // anchors still work
    const QString text = pageText(preview.get());
    QVERIFY2(text.contains(QLatin1String("[TOC]")) && text.contains(QLatin1String("[[Note]]")) && text.contains(QLatin1String("==marked==")), qPrintable(text));

    // Changed from elsewhere, the setting is followed and the check box shows it.
    Settings::self()->setGithubOnly(false);
    QTRY_COMPARE(countElements(preview.get(), "mark"), 1);
    QVERIFY(!githubOnly->isChecked());

    delete doc;
}

// Two details where the page has to agree with GitHub: a date of the front matter shows as
// it is written, and every space of a title counts in the id of its heading.
void RenderingTest::agreesWithGithubOnDatesAndAnchors()
{
    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    doc->setText(QStringLiteral("---\ndate: 2026-10-03\n---\n\n## Foo & Bar\n\nthe last line\n"));
    QVERIFY(waitForPageText(preview.get(), QLatin1String("the last line")));
    QCOMPARE(evalJs(preview.get(), QStringLiteral("document.querySelector('table td:last-child').textContent")), QStringLiteral("2026-10-03"));
    QCOMPARE(evalJs(preview.get(), QStringLiteral("document.querySelector('h2').id")), QStringLiteral("foo--bar"));

    delete doc;
}

// A "$" inside \text{} opens a formula again, as in LaTeX. And a macro of the settings
// that KaTeX cannot read costs that macro alone, not the whole document.
void RenderingTest::keepsMathOnItsFeet()
{
    const auto restore = qScopeGuard([macros = Settings::self()->mathMacros()]() {
        Settings::self()->setMathMacros(macros);
    });

    KTextEditor::Document *doc = openDocument();
    QTRY_VERIFY(doc->text().contains(SampleBodyLine));
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc);
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));

    doc->setText(QStringLiteral("$$ f(x) = \\begin{cases} 1 & \\text{si $x>0$} \\\\ 0 & \\text{sinon} \\end{cases} $$\n\nthe last line\n"));
    QVERIFY(waitForPageText(preview.get(), QLatin1String("the last line")));
    QCOMPARE(countElements(preview.get(), ".katex-display"), 1);
    QCOMPARE(countElements(preview.get(), ".katex-error, .katex [style*=\"color:#cc0000\"]"), 0);
    QCOMPARE(evalJs(preview.get(), QStringLiteral("document.querySelector('.katex-html').textContent.indexOf('$')")), QStringLiteral("-1"));

    Settings::self()->setMathMacros(QStringLiteral("\\newcommand{}{x}"));
    doc->setText(QStringLiteral("still $a^2$ here\n\nthe closing line\n"));
    QVERIFY(waitForPageText(preview.get(), QLatin1String("the closing line")));
    QCOMPARE(countElements(preview.get(), ".katex"), 1);

    delete doc;
}

// While the page is busy for more than a moment the bar says so, "Rendering" and dots that
// move, and it is quiet again once the page has caught up.
void RenderingTest::saysWhenARenderTakesLong()
{
    const std::unique_ptr<KTextEditor::Document> doc(openDocument());
    auto preview = std::make_unique<PreviewWidget>(nullptr, nullptr, doc.get());
    QVERIFY(waitForPageText(preview.get(), SampleBodyLine));
    auto *busy = preview->findChild<QLabel *>(QStringLiteral("busy"));
    QVERIFY(busy);
    QTRY_VERIFY(busy->text().isEmpty());

    // The page is kept busy for two seconds, whatever the speed of the machine: a long
    // document would render in no time on a fast one. The new text waits behind that.
    const QString twoBusySeconds = QStringLiteral("var end = Date.now() + 2000; while (Date.now() < end) {}");
    preview->findChild<QWebEngineView *>()->page()->runJavaScript(twoBusySeconds);
    doc->setText(QStringLiteral("rendered late\n"));
    QTRY_COMPARE_WITH_TIMEOUT(busy->text(), QStringLiteral("Rendering."), ReactionTimeoutMs);
    QTRY_COMPARE_WITH_TIMEOUT(busy->text(), QStringLiteral("Rendering.."), ReactionTimeoutMs);
    QVERIFY(waitForPageText(preview.get(), QLatin1String("rendered late")));
    QTRY_VERIFY_WITH_TIMEOUT(busy->text().isEmpty(), ReactionTimeoutMs);
}

QTEST_MAIN(RenderingTest)

#include "renderingtest.moc"
