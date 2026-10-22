// media.js
// Says why a picture from the web is missing when the settings keep it out, and reads
// Pandoc's size written after a picture.
//
// Part of the preview page: adds its functions to window.katemark (see core.js).
// Author: Olivier Booklage
// Date: October 2026
// License: GPL-3.0-or-later
(function () {
  "use strict";

  var katemark = window.katemark;

  // Translated name of the setting that lets pictures from the web in. The host sends it
  // while that setting is off, and an empty text while it is on.
  var blockedHint = "";

  function setRemoteMediaHint(text) {
    blockedHint = text;
  }

  // A picture the settings keep out would show as a broken image, with nothing to tell
  // why. Put a note in its place: the name of the setting to turn on, and the address.
  function explainBlockedMedia(article) {
    if (!blockedHint) {
      return;
    }
    article.querySelectorAll("img").forEach(function (picture) {
      // "src" as a property is the full address, whatever the document wrote.
      if (!/^https?:/i.test(picture.src)) {
        return;
      }
      var settingName = document.createElement("em");
      settingName.textContent = blockedHint;
      var note = document.createElement("span");
      note.className = "blocked-media";
      note.appendChild(settingName);
      note.appendChild(document.createElement("br"));
      note.appendChild(document.createTextNode(picture.src));
      picture.replaceWith(note);
    });
  }

  // Pandoc's attribute block, "{...}" at the start of the text after a picture.
  var ATTRIBUTE_BLOCK = /^\{([^{}\n]*)\}/;
  // One size in it: width=50%, height="300" (a number alone is in pixels).
  var SIZE_ATTRIBUTE = /^(width|height)=(["']?)(\d+(?:\.\d+)?)(%|px|em|rem|cm|mm|in)?\2$/;

  // The CSS of the sizes in the attribute block that opens text, and the length of that
  // block. Other attributes (#id, .class) are dropped. Null when the block has no size.
  function readSize(text) {
    var block = ATTRIBUTE_BLOCK.exec(text);
    if (!block) {
      return null;
    }
    var style = "";
    block[1].split(/\s+/).forEach(function (part) {
      var size = SIZE_ATTRIBUTE.exec(part);
      if (size) {
        style += size[1] + ":" + size[3] + (size[4] || "px") + ";";
      }
    });
    if (!style) {
      return null;
    }
    return { style: style, length: block[0].length };
  }

  // Pandoc's size after a picture: ![](figure.png){width=50%} or {#fig:a width=300}.
  // Braces with no size stay as plain text.
  function imageSize(md) {
    // After text_join, which merges an escaped character back into its text.
    md.core.ruler.after("text_join", "image_size", function (state) {
      state.tokens.forEach(function (token) {
        var children = token.type === "inline" ? token.children : null;
        for (var i = 0; children && i + 1 < children.length; i++) {
          var textAfter = children[i + 1];
          if (children[i].type !== "image" || textAfter.type !== "text") {
            continue;
          }
          var size = readSize(textAfter.content);
          if (size) {
            children[i].attrSet("style", size.style);
            textAfter.content = textAfter.content.slice(size.length);
          }
        }
      });
    });
  }

  katemark.imageSize = imageSize;
  katemark.setRemoteMediaHint = setRemoteMediaHint;
  katemark.explainBlockedMedia = explainBlockedMedia;
})();
