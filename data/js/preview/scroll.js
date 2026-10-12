// scroll.js
// Scroll sync with the editor: blocks carry their source lines, and the host asks the
// page to scroll to a line (__scrollToLine) or which line is at its top (__topLine).
//
// Part of the preview page: adds its functions to window.katemark (see core.js).
// Author: Olivier Booklage
// Date: October 2026
// License: GPL-3.0-or-later
(function () {
  "use strict";

  var katemark = window.katemark;

  // markdown-it plugin: tag every block with the span of source lines it comes from.
  function sourceLines(md) {
    // The front matter is cut off before parsing, so its line count comes back in through
    // the env.
    md.core.ruler.push("source-lines", function (state) {
      var offset = (state.env && state.env.lineOffset) || 0;
      state.tokens.forEach(function (token) {
        if (token.map && token.nesting !== -1 && token.type !== "inline") {
          token.attrSet("data-line", String(token.map[0] + offset));
          token.attrSet("data-line-end", String(token.map[1] + offset));
        }
      });
    });

    // The highlighter returns its own <pre>, which drops the token's attributes.
    var renderFence = md.renderer.rules.fence;
    md.renderer.rules.fence = function (tokens, idx, options, env, slf) {
      return renderFence(tokens, idx, options, env, slf).replace(/^<pre/, "<pre" + slf.renderAttrs(tokens[idx]));
    };
  }

  // Footnotes are left out: they render at the end of the page whatever line they were
  // written on.
  var BLOCKS = "#content [data-line]:not(.footnotes *)";

  // Distance between two probes when looking for the block at the top of the viewport.
  var PROBE_STEP_PX = 8;

  // Where the last scroll asked by the host left the page. A scroll that ends anywhere
  // else was made by the user.
  var hostScrollY = null;

  function maxScroll() {
    return document.documentElement.scrollHeight - window.innerHeight;
  }

  function scrollForHost(y) {
    window.scrollTo(0, y);
    hostScrollY = window.scrollY; // read back: the page clamps y to its own height
  }

  // ponytail: the top line of one side maps to the top of the other and the two ends are
  // pinned together, so the last screenful snaps instead of gliding. Map on the scroll
  // fraction of both viewports if that jump gets in the way.
  window.__scrollToLine = function (line, atEnd) {
    if (atEnd) {
      scrollForHost(maxScroll());
      return;
    }
    if (line <= 0) {
      scrollForHost(0);
      return;
    }
    // The last block that starts at or before the line; blocks come in source order.
    // ponytail: every block is looked at on each scroll of the editor. Keep the list
    // from one render to the next and search it by halves if long documents lag.
    var block = null;
    document.querySelectorAll(BLOCKS).forEach(function (candidate) {
      if (Number(candidate.dataset.line) <= line) {
        block = candidate;
      }
    });
    if (!block) {
      return;
    }
    // Go into the block as far as the line is into the block's span of source lines.
    var firstLine = Number(block.dataset.line);
    var lineCount = Math.max(1, Number(block.dataset.lineEnd) - firstLine);
    var progress = Math.min(1, (line - firstLine) / lineCount);
    var box = block.getBoundingClientRect();
    scrollForHost(window.scrollY + box.top + progress * box.height);
  };

  // Source line at the top of the viewport: fractional, -1 at the end of the page. Null
  // when the page is where the host last put it (the editor must not follow its own
  // echo) or when no block is in reach.
  window.__topLine = function () {
    if (hostScrollY !== null && Math.abs(window.scrollY - hostScrollY) < 1) {
      return null;
    }
    if (window.scrollY <= 0) {
      return 0;
    }
    if (window.scrollY >= maxScroll() - 1) {
      return -1;
    }
    // The top edge can fall in the margin between two blocks; probe downwards for one.
    for (var y = 0; y < window.innerHeight; y += PROBE_STEP_PX) {
      var element = document.elementFromPoint(window.innerWidth / 2, y);
      var block = element && element.closest(BLOCKS);
      if (block) {
        var firstLine = Number(block.dataset.line);
        var lineCount = Number(block.dataset.lineEnd) - firstLine;
        var box = block.getBoundingClientRect();
        // Share of the block already scrolled above the top edge, between 0 and 1.
        var progress = Math.min(1, Math.max(0, -box.top / box.height));
        return firstLine + progress * lineCount;
      }
    }
    return null;
  };

  katemark.sourceLines = sourceLines;
})();
