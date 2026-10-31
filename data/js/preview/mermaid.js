// mermaid.js
// Mermaid diagrams: loads the library on first use and draws the ```mermaid blocks.
//
// Part of the preview page: adds its functions to window.katemark (see core.js).
// Author: Olivier Booklage
// Date: October 2026
// License: GPL-3.0-or-later
(function () {
  "use strict";

  var katemark = window.katemark;

  // Mermaid weighs about 5 MB, so it loads only once a document has a ```mermaid block.
  var mermaidLoading = null;
  var mermaidReady = false;

  // Drawings by diagram source (and copy, see drawMermaid): the promise of the SVG, from
  // the start of the drawing on.
  // The preview re-renders on every edit; a diagram drawn or still being drawn is not
  // drawn again, and one Mermaid rejected keeps its error.
  var drawings = new Map();

  // Number given to the next diagram: Mermaid wants a distinct id for each drawing.
  var diagramNumber = 0;

  // Diagrams being drawn right now; an export waits until there are none.
  var mermaidPending = 0;

  // Mermaid measures labels on document.body and draws them in the article:
  // same font both sides, or the text is clipped. base.css sets it on
  // pre.mermaid. Local fonts, no network.
  var FONT = '"trebuchet ms", verdana, arial, sans-serif';

  function configureMermaid() {
    window.mermaid.initialize({
      startOnLoad: false,
      securityLevel: "strict",
      suppressErrorRendering: true,
      theme: katemark.isDark() ? "dark" : "default",
      fontFamily: FONT,
    });
    mermaidReady = true;
  }

  // Load the library on first use. The Promise is kept, so later calls wait for the same
  // download instead of starting another.
  function loadMermaid() {
    if (!mermaidLoading) {
      mermaidLoading = new Promise(function (resolve, reject) {
        var script = katemark.createElement("script");
        script.src = "qrc:/katemark/js/mermaid.min.js";
        script.onload = resolve;
        script.onerror = reject;
        katemark.head.appendChild(script);
      }).then(configureMermaid);
    }
    return mermaidLoading;
  }

  // Draw one diagram in the background: the promise of its SVG.
  function draw(source) {
    mermaidPending++;
    return loadMermaid()
      .then(function () {
        return window.mermaid.render("katemark-mermaid-" + ++diagramNumber, source);
      })
      .then(function (diagram) {
        return diagram.svg;
      })
      .finally(function () {
        mermaidPending--;
      });
  }

  // Put each diagram in its block once drawn, or the error under the source when Mermaid
  // rejects it. The block may be gone by then, replaced by a newer render: isConnected
  // tells whether it is still in the page.
  function drawMermaid(article) {
    var previous = drawings;
    drawings = new Map(); // keep only the diagrams still in the document
    article.querySelectorAll("pre.mermaid").forEach(function (block) {
      var source = block.textContent;
      // A diagram written twice gets a drawing of its own each time: its SVG ids must
      // differ, or the arrowheads of the second copy point into the first.
      var copy = 0;
      while (drawings.has(copy + "\n" + source)) {
        copy++;
      }
      var key = copy + "\n" + source;
      var drawing = previous.get(key) || draw(source);
      drawings.set(key, drawing);
      drawing.then(
        function (svg) {
          if (block.isConnected) {
            block.innerHTML = svg;
          }
        },
        function (error) {
          if (block.isConnected) {
            block.classList.add("mermaid-error");
            block.textContent = source + "\n" + ((error && error.message) || error);
          }
        }
      );
    });
  }

  // True while a diagram is still being drawn: an export waits for the end.
  function mermaidBusy() {
    return mermaidPending > 0;
  }

  // Diagrams bake their colors into the SVG. After a change of color scheme, forget the
  // ones drawn or being drawn; the blocks they were meant for go with the next render.
  // Returns false before Mermaid is ready: its first drawings take the new scheme.
  function resetMermaid() {
    if (!mermaidReady) {
      return false;
    }
    configureMermaid();
    drawings.clear();
    return true;
  }

  katemark.drawMermaid = drawMermaid;
  katemark.mermaidBusy = mermaidBusy;
  katemark.resetMermaid = resetMermaid;
})();
