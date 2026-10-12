// Render glue for the Katemark plugin.
// Exposes a small API the C++ side drives via runJavaScript():
//   __setMarkdown(text)      render markdown source
//   __setLabels(obj)         translated titles of the alerts and callouts
//   __setMacros(tex)         math macros from the settings
//   __setGithubOnly(on)      render only what GitHub renders
//   __setRemoteMediaHint(text)   name of the setting that keeps web pictures out, "" when allowed
//   __applyVars(obj)         set CSS custom properties on <html>
//   __useHljsTheme(name)     enable one bundled hljs <style>, disable the rest
//   __setCodeCss(css)        inject a generated hljs theme (Application mode)
//   __setColorScheme(dark)   set color-scheme + data attribute on <html>
//   __scrollToLine(line, atEnd) / __topLine()   scroll sync (preview/scroll.js)
//   __settled() / __exportBody()                used by the PDF and HTML exports
//
// The syntax extensions live in preview/*.js, which add themselves to window.katemark
// before this file runs.

(function () {
  "use strict";

  var katemark = window.katemark;
  var escapeHtml = katemark.escapeHtml;

  // GitHub strikes text between single tildes too; markdown-it only knows "~~".
  // ponytail: the closing tilde is the next one, without the flanking rules of the
  // specification. Write a delimiter rule if a real document trips on it.
  function singleTildeStrike(md) {
    var TILDE = 0x7e;
    md.inline.ruler.after("strikethrough", "single_tilde", function (state, silent) {
      var src = state.src;
      var start = state.pos;
      var max = state.posMax;
      if (src.charCodeAt(start) !== TILDE || src.charCodeAt(start + 1) === TILDE || src.charCodeAt(start - 1) === TILDE) {
        return false;
      }
      var end = src.indexOf("~", start + 1);
      if (end < 0 || end >= max || src.charCodeAt(end + 1) === TILDE) {
        return false;
      }
      var inner = src.slice(start + 1, end);
      if (inner === "" || /^\s|\s$|\n/.test(inner)) {
        return false;
      }
      if (!silent) {
        state.pos = start + 1;
        state.posMax = end;
        state.push("s_open", "s", 1).markup = "~";
        state.md.inline.tokenize(state);
        state.push("s_close", "s", -1).markup = "~";
      }
      state.pos = end + 1;
      state.posMax = max;
      return true;
    });
  }

  // One parser per mode: the full one, and the one that reads GitHub's syntax alone.
  function buildParser(githubOnly) {
    var md = window.markdownit({
      html: true,
      linkify: true,
      typographer: false,
      breaks: false,
      highlight: function (str, lang) {
        if (lang === "mermaid") {
          return '<pre class="mermaid">' + escapeHtml(str) + "</pre>";
        }
        var hljs = window.hljs;
        var body;
        if (hljs && lang && hljs.getLanguage(lang)) {
          try {
            body = hljs.highlight(str, { language: lang, ignoreIllegals: true }).value;
          } catch (e) {
            body = escapeHtml(str);
          }
        } else {
          // No language named, or one hljs does not know: plain text. Guessing a
          // language colors prose, where an apostrophe opens a string.
          body = escapeHtml(str);
        }
        return '<pre class="hljs"><code>' + body + "</code></pre>";
      },
    });

    md.use(katemark.taskLists);
    md.use(katemark.callouts, { githubOnly: githubOnly });
    if (githubOnly) {
      md.use(singleTildeStrike);
    } else {
      md.use(katemark.wikiLinks);
      md.use(katemark.comments);
      // markdown-it plugins: ==mark==, ++ins++, H~2~O, 19^th^.
      md.use(window.markdownitMark);
      md.use(window.markdownitIns);
      md.use(window.markdownitSub);
      md.use(window.markdownitSup);
    }
    // GitHub renders these too: footnotes, emoji, $math$.
    md.use(window.markdownitFootnote);
    // Emoji written by name: ":warning:". The plugin's "shortcuts" would also turn ":)" or
    // ":/" into emoji, which bites in ordinary text, so they are off.
    md.use(window.markdownitEmoji, { shortcuts: {} });
    // Math between $...$ and $$...$$, and between \(...\) and \[...\] as in LaTeX.
    md.use(window.texmath, { engine: katemark.lenientMath(), delimiters: ["dollars", "brackets"], katexOptions: katemark.mathOptions });
    if (!githubOnly) {
      md.use(katemark.codimdContainers);
      md.use(katemark.admonitions);
    }
    md.use(katemark.sourceLines);
    return md;
  }

  var parsers = { all: buildParser(false), github: null };
  var githubOnly = false;
  var current = "";

  // Which <details> blocks (foldable callouts, spoilers) are open, in document order.
  function foldStates(article) {
    return Array.prototype.map.call(article.querySelectorAll("details"), function (block) {
      return block.open;
    });
  }

  // Put back the folding the reader chose. Skipped when the number of blocks changed:
  // the states would no longer line up with the right blocks.
  function restoreFoldStates(article, states) {
    var blocks = article.querySelectorAll("details");
    if (blocks.length !== states.length) {
      return;
    }
    blocks.forEach(function (block, index) {
      block.open = states[index];
    });
  }

  function rerender() {
    var el = document.getElementById("content");
    if (!el) {
      return;
    }
    var folds = foldStates(el);
    katemark.resetMath();
    var tableHtml = "";
    var body = current;
    var lineOffset = 0;
    var frontMatter = katemark.frontMatterTable(current);
    if (frontMatter) {
      tableHtml = frontMatter.html;
      body = frontMatter.body;
      lineOffset = frontMatter.lines;
    }
    if (githubOnly && !parsers.github) {
      parsers.github = buildParser(true);
    }
    var md = githubOnly ? parsers.github : parsers.all;
    el.innerHTML = tableHtml + md.render(body, { lineOffset: lineOffset });
    katemark.assignHeadingIds(el);
    if (!githubOnly) {
      katemark.fillToc(el);
    }
    katemark.drawMermaid(el);
    katemark.explainBlockedMedia(el);
    restoreFoldStates(el, folds);
  }

  // The rendered article for an HTML export, cleaned on a copy: the page is left as is.
  window.__exportBody = function () {
    var copy = document.getElementById("content").cloneNode(true);
    // Raw HTML of the source may hold scripts, a <meta> that sends the reader elsewhere
    // or a <base> that moves every link. None acts in the preview; all would in the file.
    copy.querySelectorAll("script, meta, base").forEach(function (element) {
      element.remove();
    });
    copy.querySelectorAll("*").forEach(function (element) {
      Array.prototype.slice.call(element.attributes).forEach(function (attribute) {
        var isEventHandler = attribute.name.slice(0, 2) === "on"; // onclick, onerror...
        var servesScrollSync = attribute.name === "data-line" || attribute.name === "data-line-end";
        if (isEventHandler || servesScrollSync) {
          element.removeAttribute(attribute.name);
        }
      });
    });
    // Full addresses, so that the host can find the pictures and embed them.
    copy.querySelectorAll("img").forEach(function (picture) {
      picture.setAttribute("src", picture.src);
    });
    return copy.innerHTML;
  };

  // \newcommand lines from the settings, defined before every document.
  window.__setMacros = function (tex) {
    if (katemark.setGlobalMacros(tex)) {
      rerender();
    }
  };

  // Name of the setting that blocks pictures from the web, or "" when they are allowed.
  window.__setRemoteMediaHint = function (text) {
    katemark.setRemoteMediaHint(text);
    rerender();
  };

  // The host sends the setting after every change of any setting: only a real change
  // is worth a render.
  window.__setGithubOnly = function (on) {
    if (on === githubOnly) {
      return;
    }
    githubOnly = on;
    document.body.classList.toggle("github-only", on); // base.css narrows it
    rerender();
  };

  window.__setLabels = function (labels) {
    katemark.labels = labels;
  };

  // True once nothing is still being drawn or loaded: an export waits for this.
  window.__settled = function () {
    return (
      !katemark.mermaidBusy() &&
      Array.prototype.every.call(document.images, function (img) {
        return img.complete;
      })
    );
  };

  window.__setMarkdown = function (text) {
    current = text;
    rerender();
  };

  window.__applyVars = function (vars) {
    var root = document.documentElement.style;
    for (var k in vars) {
      if (Object.prototype.hasOwnProperty.call(vars, k)) {
        root.setProperty(k, vars[k]);
      }
    }
  };

  window.__useHljsTheme = function (name) {
    var keep = null;
    if (name === "github-dark") {
      keep = "hljs-ghd";
    } else if (name === "github") {
      keep = "hljs-gh";
    }
    ["hljs-gh", "hljs-ghd"].forEach(function (id) {
      var sheet = document.getElementById(id);
      if (sheet) {
        sheet.disabled = id !== keep;
      }
    });
    var generated = document.getElementById("hljs-generated");
    if (generated) {
      generated.textContent = ""; // bundled theme active, clear generated
    }
  };

  window.__setCodeCss = function (css) {
    ["hljs-gh", "hljs-ghd"].forEach(function (id) {
      var sheet = document.getElementById(id);
      if (sheet) {
        sheet.disabled = true; // generated theme takes over
      }
    });
    var generated = document.getElementById("hljs-generated");
    if (!generated) {
      generated = document.createElement("style");
      generated.id = "hljs-generated";
      document.head.appendChild(generated);
    }
    generated.textContent = css;
  };

  window.__setColorScheme = function (dark) {
    var changed = dark !== katemark.isDark();
    document.documentElement.setAttribute("data-pv-scheme", dark ? "dark" : "light");
    document.documentElement.style.setProperty("color-scheme", dark ? "dark" : "light");
    // Diagrams bake their colors into the SVG, so a scheme change redraws them.
    if (changed && katemark.resetMermaid()) {
      rerender();
    }
  };
})();
