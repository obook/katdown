// callouts.js
// GitHub alerts (> [!NOTE]) and Obsidian callouts (> [!faq]- Title), and the look they
// share with the MkDocs admonitions of mkdocs.js.
//
// Part of the preview page: adds its functions to window.katemark (see core.js).
// GitHub alerts by uwuclxdy; Obsidian callouts by Olivier Booklage, October 2026.
// License: GPL-3.0-or-later
(function () {
  "use strict";

  var katemark = window.katemark;
  var escapeHtml = katemark.escapeHtml;

  var ICONS = {
    note: '<svg class="octicon" viewBox="0 0 16 16" width="16" height="16" aria-hidden="true" style="fill:currentColor;margin-right:8px"><path d="M0 8a8 8 0 1 1 16 0A8 8 0 0 1 0 8Zm8-6.5a6.5 6.5 0 1 0 0 13 6.5 6.5 0 0 0 0-13ZM6.5 7.75A.75.75 0 0 1 7.25 7h1a.75.75 0 0 1 .75.75v2.75h.25a.75.75 0 0 1 0 1.5h-2a.75.75 0 0 1 0-1.5h.25v-2h-.25a.75.75 0 0 1-.75-.75ZM8 6a1 1 0 1 1 0-2 1 1 0 0 1 0 2Z"/></svg>',
    tip: '<svg class="octicon" viewBox="0 0 16 16" width="16" height="16" aria-hidden="true" style="fill:currentColor;margin-right:8px"><path d="M8 1.5c-2.363 0-4 1.69-4 3.75 0 .984.424 1.625.984 2.304l.214.253c.223.264.47.556.673.848.284.411.537.896.621 1.49a.75.75 0 0 1-1.484.211c-.04-.282-.163-.547-.37-.847a8.456 8.456 0 0 0-.542-.68c-.084-.1-.173-.205-.268-.32C3.201 7.75 2.5 6.766 2.5 5.25 2.5 2.31 4.863 0 8 0s5.5 2.31 5.5 5.25c0 1.516-.701 2.5-1.328 3.259-.095.115-.184.22-.268.319-.207.245-.383.453-.541.681-.208.3-.33.565-.37.847a.751.751 0 0 1-1.485-.212c.084-.593.337-1.078.621-1.489.203-.292.45-.584.673-.848.075-.088.147-.173.213-.253.561-.679.985-1.32.985-2.304 0-2.06-1.637-3.75-4-3.75ZM5.75 12h4.5a.75.75 0 0 1 0 1.5h-4.5a.75.75 0 0 1 0-1.5ZM6 15.25a.75.75 0 0 1 .75-.75h2.5a.75.75 0 0 1 0 1.5h-2.5a.75.75 0 0 1-.75-.75Z"/></svg>',
    important: '<svg class="octicon" viewBox="0 0 16 16" width="16" height="16" aria-hidden="true" style="fill:currentColor;margin-right:8px"><path d="M0 1.75C0 .784.784 0 1.75 0h12.5C15.216 0 16 .784 16 1.75v9.5A1.75 1.75 0 0 1 14.25 13H8.06l-2.573 2.573A1.458 1.458 0 0 1 3 14.543V13H1.75A1.75 1.75 0 0 1 0 11.25Zm1.75-.25a.25.25 0 0 0-.25.25v9.5c0 .138.112.25.25.25h2a.75.75 0 0 1 .75.75v2.19l2.72-2.72a.749.749 0 0 1 .53-.22h6.5a.25.25 0 0 0 .25-.25v-9.5a.25.25 0 0 0-.25-.25Zm7 2.25v2.5a.75.75 0 0 1-1.5 0v-2.5a.75.75 0 0 1 1.5 0ZM9 9a1 1 0 1 1-2 0 1 1 0 0 1 2 0Z"/></svg>',
    warning: '<svg class="octicon" viewBox="0 0 16 16" width="16" height="16" aria-hidden="true" style="fill:currentColor;margin-right:8px"><path d="M6.457 1.047c.659-1.234 2.427-1.234 3.086 0l6.082 11.378A1.75 1.75 0 0 1 14.082 15H1.918a1.75 1.75 0 0 1-1.543-2.575Zm1.763.707a.25.25 0 0 0-.44 0L1.698 13.132a.25.25 0 0 0 .22.368h12.164a.25.25 0 0 0 .22-.368Zm.53 3.996v2.5a.75.75 0 0 1-1.5 0v-2.5a.75.75 0 0 1 1.5 0ZM9 11a1 1 0 1 1-2 0 1 1 0 0 1 2 0Z"/></svg>',
    caution: '<svg class="octicon" viewBox="0 0 16 16" width="16" height="16" aria-hidden="true" style="fill:currentColor;margin-right:8px"><path d="M4.47.22A.749.749 0 0 1 5 0h6c.199 0 .389.079.53.22l4.25 4.25c.141.14.22.331.22.53v6a.749.749 0 0 1-.22.53l-4.25 4.25A.749.749 0 0 1 11 16H5a.749.749 0 0 1-.53-.22L.22 11.53A.749.749 0 0 1 0 11V5c0-.199.079-.389.22-.53Zm.84 1.28L1.5 5.31v5.38l3.81 3.81h5.38l3.81-3.81V5.31L10.69 1.5ZM8 4a.75.75 0 0 1 .75.75v3.5a.75.75 0 0 1-1.5 0v-3.5A.75.75 0 0 1 8 4Zm0 8a1 1 0 1 1 0-2 1 1 0 0 1 0 2Z"/></svg>',
  };
  // Each type takes one of GitHub's five alert looks (color and icon), or "quote", so
  // every callout follows the active theme.
  var TONES = {
    note: "note",
    info: "note",
    todo: "note",
    abstract: "note",
    tip: "tip",
    success: "tip",
    important: "important",
    example: "important",
    warning: "warning",
    question: "warning",
    caution: "caution",
    failure: "caution",
    danger: "caution",
    bug: "caution",
    quote: "quote",
  };
  var ALIASES = {
    summary: "abstract",
    tldr: "abstract",
    hint: "tip",
    check: "success",
    done: "success",
    help: "question",
    faq: "question",
    attention: "warning",
    fail: "failure",
    missing: "failure",
    error: "danger",
    cite: "quote",
  };

  // What a block of a given type looks like: its color family, its icon, and the title
  // it shows when the author wrote none. An unknown type looks like a note, as in
  // Obsidian, and is titled with its own name.
  function calloutLook(name) {
    var type = name;
    if (Object.prototype.hasOwnProperty.call(ALIASES, name)) {
      type = ALIASES[name];
    }
    if (!Object.prototype.hasOwnProperty.call(TONES, type)) {
      return { tone: "note", icon: ICONS.note, title: name.charAt(0).toUpperCase() + name.slice(1) };
    }
    var tone = TONES[type];
    return { tone: tone, icon: ICONS[tone] || ICONS.note, title: katemark.labels[type] };
  }

  // The title line of a block: <p> for a plain one, <summary> for one that folds.
  function calloutTitle(look, titleHtml, tag) {
    return "<" + tag + ' class="markdown-alert-title">' + look.icon + titleHtml + "</" + tag + ">\n";
  }

  // GitHub alerts and Obsidian callouts are the same blockquote syntax. Obsidian adds
  // more types, an optional title after the marker and folding with "-" (collapsed) or
  // "+" (expanded).
  function callouts(md, options) {
    // "[!type]", then "-" or "+" when the callout folds, then the title if there is one.
    var OBSIDIAN_MARKER = /^\[!([\w-]+)\]([+-]?)(?:[ \t]+|$)/;
    // GitHub knows five types, alone on their line: no folding and no title.
    var GITHUB_MARKER = /^\[!(note|tip|important|warning|caution)\]()[ \t]*$/i;
    var markerPattern = options && options.githubOnly ? GITHUB_MARKER : OBSIDIAN_MARKER;

    // Read the marker that opens the blockquote starting at tokens[i] and remove it from
    // the text. Returns null when the blockquote is an ordinary quote.
    function takeMarker(tokens, i) {
      var paragraph = tokens[i + 1];
      var inline = tokens[i + 2];
      if (!paragraph || paragraph.type !== "paragraph_open" || !inline || inline.type !== "inline") {
        return null;
      }
      var firstChild = inline.children && inline.children[0];
      if (!firstChild || firstChild.type !== "text") {
        return null;
      }
      var match = markerPattern.exec(firstChild.content);
      if (!match) {
        return null;
      }
      firstChild.content = firstChild.content.slice(match[0].length);
      return { name: match[1].toLowerCase(), fold: match[2] };
    }

    // Change the tag of the blockquote that opens at tokens[i], and of its closing token.
    // Blockquotes nest, hence the depth count to find the matching close.
    function retagBlockquote(tokens, i, tag) {
      var depth = 0;
      for (var j = i; j < tokens.length; j++) {
        if (tokens[j].type === "blockquote_open") {
          depth++;
        } else if (tokens[j].type === "blockquote_close") {
          depth--;
          if (depth === 0) {
            tokens[j].tag = tag;
            break;
          }
        }
      }
      tokens[i].tag = tag;
    }

    // Cut the rest of the marker line out of the first paragraph and render it: that is
    // the custom title. What follows the line break stays in place as the body.
    function takeTitle(tokens, i, env) {
      var paragraph = tokens[i + 1];
      var inline = tokens[i + 2];
      var lineBreak = inline.children.findIndex(function (child) {
        return child.type === "softbreak" || child.type === "hardbreak";
      });
      var titleTokens;
      if (lineBreak < 0) {
        titleTokens = inline.children.splice(0, inline.children.length);
      } else {
        titleTokens = inline.children.splice(0, lineBreak + 1);
        titleTokens.pop(); // the line break itself
      }
      if (inline.children.length === 0) {
        // Nothing is left of the marker paragraph: do not render an empty <p>.
        paragraph.hidden = true;
        tokens[i + 3].hidden = true;
      }
      return md.renderer.renderInline(titleTokens, md.options, env).trim();
    }

    // A core rule runs once on the whole token list, after the Markdown is parsed.
    md.core.ruler.after("inline", "callouts", function (state) {
      var tokens = state.tokens;
      for (var i = 0; i < tokens.length; i++) {
        if (tokens[i].type !== "blockquote_open") {
          continue;
        }
        var marker = takeMarker(tokens, i);
        if (!marker) {
          continue;
        }
        var look = calloutLook(marker.name);
        var foldable = marker.fold !== "";

        retagBlockquote(tokens, i, foldable ? "details" : "div");
        tokens[i].attrSet("class", "markdown-alert markdown-alert-" + look.tone);
        if (marker.fold === "+") {
          tokens[i].attrSet("open", "");
        }

        var titleHtml = takeTitle(tokens, i, state.env) || escapeHtml(look.title);
        var title = new state.Token("html_block", "", 0);
        title.block = true;
        title.content = calloutTitle(look, titleHtml, foldable ? "summary" : "p");
        tokens.splice(i + 1, 0, title);
      }
    });
  }

  katemark.callouts = callouts;
  katemark.calloutLook = calloutLook;
  katemark.calloutTitle = calloutTitle;
})();
