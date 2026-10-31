// zoom.js
// Zoom of the preview: Ctrl + wheel enlarges or shrinks the whole article, diagrams
// included, and Ctrl+0 or __resetZoom() puts it back to 100%.
//
// A CSS zoom on the article, not the page zoom of Chromium: Mermaid measures its labels
// outside the article, so they keep their 100% size. Under a page zoom of 90% or 110%
// it finds them a fraction of a pixel too narrow and clips their text.
//
// Part of the preview page.
// Author: Olivier Booklage
// Date: October 2026
// License: GPL-3.0-or-later
(function () {
  "use strict";

  var katemark = window.katemark;

  var ZOOM_STEP = 1.1;
  var LEVEL_MIN = -7; // about 50%
  var LEVEL_MAX = 12; // about 300%

  // Chromium counts 120 per notch of a wheel, whatever the wheel. A touchpad or a fine
  // wheel sends many smaller events: their distance adds up to a notch before a step.
  var WHEEL_NOTCH = 120;
  var wheelPending = 0;

  // Steps away from 100%: a whole number, so coming back lands on exactly 1.
  var level = 0;

  // The page reloads when the document changes folder or a setting needs it. The zoom
  // stays: the page's session storage lasts as long as the preview.
  var STORE_KEY = "katemark-zoom";

  // The zoom goes on <html>, which exists from the start; base.css applies it to the article.
  function setLevel(value) {
    level = Math.min(LEVEL_MAX, Math.max(LEVEL_MIN, value));
    katemark.root.style.setProperty("--pv-zoom", String(Math.pow(ZOOM_STEP, level)));
    try {
      sessionStorage.setItem(STORE_KEY, String(level));
    } catch (e) {
      // No storage: the zoom lasts until the page reloads.
    }
  }

  try {
    setLevel(Number(sessionStorage.getItem(STORE_KEY)) || 0);
  } catch (e) {
    // No storage: the preview starts at 100%.
  }

  // "passive: false" is what allows preventDefault() in a wheel listener.
  document.addEventListener(
    "wheel",
    function (event) {
      if (!event.ctrlKey) {
        return;
      }
      event.preventDefault(); // otherwise Chromium zooms the whole page
      wheelPending += event.wheelDeltaY;
      var steps = Math.trunc(wheelPending / WHEEL_NOTCH);
      if (steps !== 0) {
        wheelPending -= steps * WHEEL_NOTCH;
        setLevel(level + steps);
      }
    },
    { passive: false }
  );

  // "code" names the key whatever the layout: on AZERTY, Ctrl+0 gives the key "à". Ctrl and
  // no other modifier: AltGr, which Windows sends as Ctrl + Alt, types "@" on that key.
  document.addEventListener("keydown", function (event) {
    var ctrlAlone = event.ctrlKey && !event.altKey && !event.shiftKey && !event.metaKey;
    if (ctrlAlone && (event.code === "Digit0" || event.key === "0")) {
      event.preventDefault();
      setLevel(0);
    }
  });

  window.__resetZoom = function () {
    setLevel(0);
  };

  katemark.zoomFactor = function () {
    return Math.pow(ZOOM_STEP, level);
  };
})();
