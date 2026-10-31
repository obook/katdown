// codimd.js
// CodiMD syntax: :::success / :::info / :::warning / :::danger alert areas, :::spoiler
// and the [TOC] table of contents.
//
// Part of the preview page: adds its functions to window.katemark (see core.js).
// Author: Olivier Booklage
// Date: October 2026
// License: GPL-3.0-or-later
(function () {
  "use strict";

  var katemark = window.katemark;

  // The alert areas answer to their Docusaurus names too and take an optional title
  // (":::info Title" or ":::info[Title]"); :::spoiler is a collapsible block.
  function codimdContainers(md) {
    // Name written in the document -> one of the four alert colors.
    var KINDS = {
      success: "success",
      tip: "success",
      info: "info",
      note: "info",
      warning: "warning",
      caution: "warning",
      danger: "danger",
    };
    // A name, then the title either in brackets or after a space.
    var ALERT = /^(\w+)(?:\[(.*)\]|\s+(.*))?$/;
    var SPOILER = /^spoiler(?:\s+(.*))?$/;

    // Split what follows ":::" into the alert's kind and title, or return null when it
    // is not one of the alert names.
    function parseAlert(info) {
      var match = ALERT.exec(info.trim());
      if (!match || !Object.prototype.hasOwnProperty.call(KINDS, match[1])) {
        return null;
      }
      return { kind: KINDS[match[1]], title: match[2] || match[3] || "" };
    }

    // markdown-it-container calls a render function twice per block: with nesting 1
    // for the opening ":::" line and with nesting -1 for the closing one.
    function renderAlert(tokens, idx) {
      if (tokens[idx].nesting !== 1) {
        return "</div>\n";
      }
      var alert = parseAlert(tokens[idx].info);
      var html = '<div class="alert alert-' + alert.kind + '" role="alert">\n';
      if (alert.title) {
        html += '<p class="alert-title">' + md.renderInline(alert.title) + "</p>\n";
      }
      return html;
    }

    function renderSpoiler(tokens, idx) {
      if (tokens[idx].nesting !== 1) {
        return "</details>\n";
      }
      var summary = SPOILER.exec(tokens[idx].info.trim())[1];
      var html = "<details>";
      if (summary) {
        html += "<summary>" + md.renderInline(summary) + "</summary>";
      }
      return html + "\n";
    }

    md.use(window.markdownitContainer, "alert", {
      validate: function (params) {
        return parseAlert(params) !== null;
      },
      render: renderAlert,
    });
    md.use(window.markdownitContainer, "spoiler", {
      validate: function (params) {
        return SPOILER.test(params.trim());
      },
      render: renderSpoiler,
    });
  }

  // CodiMD's [TOC] / [TOC maxLevel=2]: a paragraph holding only that marker becomes a
  // list of the document's headings.
  var TOC = /^\[TOC(?:\s+maxLevel=(\d))?\]$/i;

  // Indentation of a table-of-contents entry, per heading level below the first.
  var TOC_INDENT_EM = 1.5;

  function buildToc(headings, maxLevel) {
    var list = katemark.createElement("ul");
    list.className = "toc";
    headings.forEach(function (heading) {
      var level = Number(heading.tagName.charAt(1)); // "H2" -> 2
      if (level > maxLevel) {
        return;
      }
      var link = katemark.createElement("a");
      link.href = "#" + encodeURIComponent(heading.id);
      link.textContent = katemark.shownText(heading);
      var item = katemark.createElement("li");
      item.style.marginLeft = (level - 1) * TOC_INDENT_EM + "em";
      item.appendChild(link);
      list.appendChild(item);
    });
    return list;
  }

  // Replace each [TOC] by the list of the headings, which have their ids by then (see
  // assignHeadingIds in core.js).
  function fillToc(article) {
    var headings = article.querySelectorAll("h1,h2,h3,h4,h5,h6");
    article.querySelectorAll("p").forEach(function (paragraph) {
      var match = TOC.exec(paragraph.textContent.trim());
      if (match) {
        var maxLevel = match[1] ? Number(match[1]) : 6;
        paragraph.replaceWith(buildToc(headings, maxLevel));
      }
    });
  }

  katemark.codimdContainers = codimdContainers;
  katemark.fillToc = fillToc;
})();
