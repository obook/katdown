// obsidian.js
// Obsidian syntax: wiki links, image embeds with a size, %%comments%%.
// The callouts are in callouts.js, with the GitHub alerts they extend.
//
// Part of the preview page: adds its functions to window.katemark (see core.js).
// Author: Olivier Booklage
// Date: October 2026
// License: GPL-3.0-or-later
(function () {
  "use strict";

  var katemark = window.katemark;
  var slugify = katemark.slugify;

  // Obsidian wiki links: [[Note]], [[Note|label]], [[Note#Heading]], and image embeds
  // ![[image.png|300]] or ![[image.png|300x200]].
  // ponytail: targets resolve against the document's folder, not across a whole vault,
  // and embedding a note shows a link to it. Resolve by file name on the host side if
  // vault-wide links are needed.
  function wikiLinks(md) {
    var IMAGE = /\.(png|jpe?g|gif|svg|webp|bmp|avif)$/i;
    var escapeAttribute = md.utils.escapeHtml; // also escapes the double quote

    // <img> for an embedded picture. What follows "|" is a size when it is made of
    // digits ("300" or "300x200"), an alternative text otherwise.
    function renderImage(target, label) {
      var size = /^(\d+)(?:x(\d+))?$/.exec(label);
      var alt = target;
      if (label && !size) {
        alt = label;
      }
      var html = '<img src="' + escapeAttribute(encodeURI(target)) + '" alt="' + escapeAttribute(alt) + '"';
      if (size) {
        html += ' width="' + size[1] + '"';
        if (size[2]) {
          html += ' height="' + size[2] + '"';
        }
      }
      return html + ">";
    }

    // The id of a heading written in Markdown. Ids come from the rendered title (core.js):
    // "_Big_ deal" gives "big-deal". A <template> keeps raw HTML in it inert.
    function headingSlug(heading) {
      var holder = katemark.createElement("template");
      holder.innerHTML = md.renderInline(heading);
      return slugify(katemark.shownText(holder.content));
    }

    // <a> for a link to a note, to one of its headings, or to a heading of this page.
    function renderLink(target, label) {
      var file = target;
      var heading = "";
      var hash = target.indexOf("#");
      if (hash >= 0) {
        file = target.slice(0, hash);
        heading = target.slice(hash + 1);
      }

      var href = "";
      if (file) {
        // A target written without an extension is a Markdown note.
        var hasExtension = /\.\w+$/.test(file);
        href = encodeURI(hasExtension ? file : file + ".md");
      }
      if (heading) {
        href += "#" + encodeURIComponent(headingSlug(heading));
      }

      var text = label;
      if (!text && file && heading) {
        text = file + " > " + heading;
      } else if (!text) {
        text = file || heading;
      }
      return '<a class="wikilink" href="' + escapeAttribute(href) + '">' + escapeAttribute(text) + "</a>";
    }

    md.inline.ruler.before("link", "wikilink", function (state, silent) {
      var isEmbed = state.src.charAt(state.pos) === "!";
      var open = isEmbed ? state.pos + 1 : state.pos;
      if (state.src.slice(open, open + 2) !== "[[") {
        return false;
      }
      var close = state.src.indexOf("]]", open + 2);
      if (close < 0) {
        return false;
      }
      // "[[1]](url)" and "[[1]][ref]" are ordinary links whose text is in brackets.
      var next = state.src.charAt(close + 2);
      if (next === "(" || next === "[") {
        return false;
      }
      var body = state.src.slice(open + 2, close);
      if (!body.trim() || /[\[\]\n]/.test(body)) {
        return false;
      }
      // markdown-it calls a rule in "silent" mode to ask whether it matches here,
      // without wanting the token.
      if (!silent) {
        var token = state.push("wikilink", "", 0);
        token.content = body;
        token.meta = { embed: isEmbed };
      }
      state.pos = close + 2;
      return true;
    });

    md.renderer.rules.wikilink = function (tokens, idx) {
      var content = tokens[idx].content;
      var target = content;
      var label = "";
      var bar = content.indexOf("|");
      if (bar >= 0) {
        target = content.slice(0, bar);
        label = content.slice(bar + 1).trim();
      }
      target = target.trim();
      if (tokens[idx].meta.embed && IMAGE.test(target)) {
        return renderImage(target, label);
      }
      return renderLink(target, label);
    };
  }

  // Obsidian comments: %%hidden%% inside a line, or a block opened and closed by a line
  // holding %%. They stay in the source and leave the preview and the exports.
  function comments(md) {
    // Inside a paragraph: skip from "%%" to the next "%%" without producing anything.
    md.inline.ruler.before("emphasis", "comment", function (state) {
      if (state.src.slice(state.pos, state.pos + 2) !== "%%") {
        return false;
      }
      var close = state.src.indexOf("%%", state.pos + 2);
      // Within one line only, so that two stray "%%" far apart do not hide the text
      // between them. The block form below is for comments of several lines.
      var lineEnd = state.src.indexOf("\n", state.pos);
      if (close < 0 || (lineEnd >= 0 && close > lineEnd)) {
        return false;
      }
      state.pos = close + 2;
      return true;
    });

    // Over several lines: a line starting with "%%" and not closed on that same line
    // opens a block that runs to the next line holding "%%".
    // "alt" lists the blocks this rule may interrupt: without it, a "%%" line directly
    // under a paragraph would be read as the rest of that paragraph.
    var interrupts = { alt: ["paragraph", "reference", "blockquote", "list"] };
    md.block.ruler.before("paragraph", "comment", hideCommentBlock, interrupts);

    function hideCommentBlock(state, startLine, endLine, silent) {
      var firstLine = state.src.slice(state.bMarks[startLine] + state.tShift[startLine], state.eMarks[startLine]);
      if (firstLine.slice(0, 2) !== "%%" || firstLine.indexOf("%%", 2) >= 0) {
        return false;
      }
      for (var line = startLine + 1; line < endLine; line++) {
        var text = state.src.slice(state.bMarks[line], state.eMarks[line]);
        if (text.indexOf("%%") >= 0) {
          if (!silent) {
            state.line = line + 1; // resume parsing after the closing line
          }
          return true;
        }
      }
      return false;
    }
  }

  katemark.wikiLinks = wikiLinks;
  katemark.comments = comments;
})();
