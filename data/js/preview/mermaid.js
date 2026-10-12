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

  // Finished SVGs by diagram source. The preview re-renders on every edit; without this
  // each diagram would drop back to its source text and be laid out again every time.
  var svgBySource = new Map();

  // Number given to the next diagram: Mermaid wants a distinct id for each drawing.
  var diagramNumber = 0;

  // Diagrams being drawn right now; an export waits until there are none.
  var mermaidPending = 0;
  // Counts the changes of color scheme. A drawing started under an earlier scheme has
  // the wrong colors and is dropped when it arrives.
  var schemeChangeCount = 0;

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
  }

  // Load the library on first use. The Promise is kept, so later calls wait for the same
  // download instead of starting another.
  function loadMermaid() {
    if (!mermaidLoading) {
      mermaidLoading = new Promise(function (resolve, reject) {
        var script = document.createElement("script");
        script.src = "qrc:/katemark/js/mermaid.min.js";
        script.onload = resolve;
        script.onerror = reject;
        document.head.appendChild(script);
      }).then(configureMermaid);
    }
    return mermaidLoading;
  }

  // Draw one diagram in the background, then put the SVG in its block, or the error
  // under the source when Mermaid rejects it. The block may be gone by then, replaced
  // by a newer render: isConnected tells whether it is still in the page.
  function renderDiagram(block, source) {
    var changesAtStart = schemeChangeCount;
    mermaidPending++;
    loadMermaid()
      .then(function () {
        return window.mermaid.render("katemark-mermaid-" + ++diagramNumber, source);
      })
      .finally(function () {
        mermaidPending--;
      })
      .then(
        function (diagram) {
          if (changesAtStart !== schemeChangeCount) {
            return;
          }
          svgBySource.set(source, diagram.svg);
          if (block.isConnected) {
            block.innerHTML = diagram.svg;
          }
        },
        function (error) {
          if (block.isConnected) {
            block.classList.add("mermaid-error");
            block.textContent = source + "\n" + ((error && error.message) || error);
          }
        }
      );
  }

  function drawMermaid(article) {
    var previous = svgBySource;
    svgBySource = new Map(); // keep only the diagrams still in the document
    article.querySelectorAll("pre.mermaid").forEach(function (block) {
      var source = block.textContent;
      if (previous.has(source)) {
        svgBySource.set(source, previous.get(source));
        block.innerHTML = previous.get(source);
      } else {
        renderDiagram(block, source);
      }
    });
  }

  // True while a diagram is still being drawn: an export waits for the end.
  function mermaidBusy() {
    return mermaidPending > 0;
  }

  // Diagrams bake their colors into the SVG. After a change of color scheme, forget the
  // ones already drawn. Returns false when Mermaid was never loaded: nothing to redraw.
  function resetMermaid() {
    if (!window.mermaid) {
      return false;
    }
    configureMermaid();
    svgBySource.clear();
    schemeChangeCount++;
    return true;
  }

  // Ctrl + wheel over a diagram zooms it; its block then scrolls to move around, and a
  // double click puts the diagram back. A new render also does: the zoom is not kept.
  var ZOOM_STEP = 1.15;
  var ZOOM_MIN = 0.25;
  var ZOOM_MAX = 8;

  function diagramUnder(event) {
    var block = event.target.closest ? event.target.closest("pre.mermaid") : null;
    return block ? block.querySelector("svg") : null;
  }

  function zoomDiagram(event) {
    var svg = diagramUnder(event);
    if (!svg || !event.ctrlKey) {
      return;
    }
    event.preventDefault(); // otherwise Ctrl + wheel zooms the whole page
    var factor = event.deltaY < 0 ? ZOOM_STEP : 1 / ZOOM_STEP;
    var zoom = Math.min(ZOOM_MAX, Math.max(ZOOM_MIN, Number(svg.dataset.zoom || 1) * factor));
    svg.dataset.zoom = zoom;
    // Mermaid sizes a diagram with "max-width"; the drawing's own width is in viewBox.
    svg.style.maxWidth = "none";
    svg.style.width = svg.viewBox.baseVal.width * zoom + "px";
  }

  function resetDiagramZoom(event) {
    var svg = diagramUnder(event);
    if (svg && svg.dataset.zoom) {
      delete svg.dataset.zoom;
      svg.style.width = "";
      svg.style.maxWidth = svg.viewBox.baseVal.width + "px";
    }
  }

  // "passive: false" is what allows preventDefault() in a wheel listener.
  document.addEventListener("wheel", zoomDiagram, { passive: false });
  document.addEventListener("dblclick", resetDiagramZoom);

  katemark.drawMermaid = drawMermaid;
  katemark.mermaidBusy = mermaidBusy;
  katemark.resetMermaid = resetMermaid;
})();
