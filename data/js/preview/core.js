// core.js
// The namespace shared by the files of the preview page, and the helpers that
// several of them use. Loaded first: every other file adds its functions to
// window.katemark.
// Author: Olivier Booklage
// Date: October 2026
// License: GPL-3.0-or-later
(function () {
  "use strict";

  var katemark = (window.katemark = {});

  // Raw HTML in a document may name an element "getElementById" or "createElement": that
  // name then hides the property of document ("DOM clobbering") and the next render
  // throws. Take what the page needs from document now, before any Markdown is in it.
  var root = document.documentElement;
  katemark.root = root;
  katemark.head = document.head;
  katemark.byId = document.getElementById.bind(document);
  katemark.createElement = document.createElement.bind(document);
  katemark.elementFromPoint = document.elementFromPoint.bind(document);

  // Titles of the alerts and callouts, translated: the host sends them before the first
  // render (see __setLabels in preview.js).
  katemark.labels = {};

  // The id a heading gets from its title, built as GitHub does: lower case, punctuation
  // dropped, each space turned into a hyphen. "What's new?" gives "whats-new", and
  // "Foo & Bar" gives "foo--bar": the two spaces around the "&" both count.
  // \p{L} and \p{N} are letters and digits of any alphabet, accented ones included.
  function slugify(text) {
    return text
      .trim()
      .toLowerCase()
      .replace(/[^\p{L}\p{N}\s_-]/gu, "")
      .replace(/\s/g, "-");
  }

  // Text of an element as the reader sees it, without the hidden MathML copy that KaTeX
  // writes beside each formula: "$E=mc^2$" reads "E=mc2", not "E=mc2E=mc^2E=mc2".
  function shownText(element) {
    var copy = element.cloneNode(true);
    copy.querySelectorAll(".katex-mathml").forEach(function (math) {
      math.remove();
    });
    return copy.textContent;
  }

  // Give every heading an id made from its title, so that #anchor links, wiki links to a
  // heading and the table of contents can point at it.
  function assignHeadingIds(article) {
    var headings = article.querySelectorAll("h1,h2,h3,h4,h5,h6");
    // Ids already taken, starting with the ones the author wrote in raw HTML: those stay.
    var taken = new Set();
    headings.forEach(function (heading) {
      if (heading.id) {
        taken.add(heading.id);
      }
    });
    headings.forEach(function (heading) {
      if (heading.id) {
        return;
      }
      var slug = slugify(shownText(heading)) || "section";
      // A repeated title gets "-1", "-2"... until the id is free: "Setup" twice and
      // "Setup 1" once must still give three different ids.
      var id = slug;
      for (var copy = 1; taken.has(id); copy++) {
        id = slug + "-" + copy;
      }
      taken.add(id);
      heading.id = id;
    });
  }

  function isDark() {
    return root.getAttribute("data-pv-scheme") === "dark";
  }

  katemark.escapeHtml = window.markdownit().utils.escapeHtml; // markdown-it's own
  katemark.slugify = slugify;
  katemark.shownText = shownText;
  katemark.assignHeadingIds = assignHeadingIds;
  katemark.isDark = isDark;
})();
