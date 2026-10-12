// github.js
// GitHub task-list checkboxes and the metadata table made from leading YAML front matter.
//
// Part of the preview page: adds its functions to window.katemark (see core.js).
// Code by uwuclxdy, moved out of preview.js.
// License: GPL-3.0-or-later
(function () {
  "use strict";

  var katemark = window.katemark;
  var escapeHtml = katemark.escapeHtml;

  // GitHub-flavored task list checkboxes (adapted from markdown-it-task-lists).
  function taskLists(md) {
    md.core.ruler.after("inline", "github-task-lists", function (state) {
      var tokens = state.tokens;
      for (var i = 2; i < tokens.length; i++) {
        if (isTodoItem(tokens, i)) {
          todoify(tokens[i], state.Token);
          attrSet(tokens[i - 2], "class", "task-list-item");
          var list = parentList(tokens, i - 2);
          if (list >= 0) {
            attrSet(tokens[list], "class", "contains-task-list");
          }
        }
      }
    });
    function attrSet(token, name, value) {
      var index = token.attrIndex(name);
      if (index < 0) {
        token.attrPush([name, value]);
      } else {
        token.attrs[index][1] = value;
      }
    }
    function parentList(tokens, index) {
      var target = tokens[index].level - 1;
      for (var i = index - 1; i >= 0; i--) {
        if (tokens[i].level === target) {
          return i;
        }
      }
      return -1;
    }
    function isTodoItem(tokens, i) {
      return (
        tokens[i].type === "inline" &&
        tokens[i - 1].type === "paragraph_open" &&
        tokens[i - 2].type === "list_item_open" &&
        /^\[[ xX]\] /.test(tokens[i].content)
      );
    }
    function todoify(token, Token) {
      var checked = /^\[[xX]\] /.test(token.content);
      var box = new Token("html_inline", "", 0);
      box.content =
        '<input class="task-list-item-checkbox"' +
        (checked ? ' checked=""' : "") +
        ' disabled="" type="checkbox"> ';
      token.children.unshift(box);
      token.children[1].content = token.children[1].content.replace(/^\[[ xX]\] /, "");
    }
  }

  // Leading YAML frontmatter (very first line "---" through a closing "---")
  // renders as a GitHub-style metadata table instead of an <hr> plus raw text.
  var FRONT_MATTER = /^---[ \t]*\r?\n([\s\S]*?\n)?---[ \t]*(?:\r?\n|$)/;

  function frontMatterTable(src) {
    var match = FRONT_MATTER.exec(src);
    if (!match) {
      return null;
    }
    var data;
    try {
      // The core schema reads no dates: "date: 2026-10-03" stays the text it is, as on
      // GitHub, where the default schema would make it a Date shown with its time.
      data = window.jsyaml ? window.jsyaml.load(match[1] || "", { schema: window.jsyaml.CORE_SCHEMA }) : null;
    } catch (e) {
      return null;
    }
    if (!data || typeof data !== "object" || Array.isArray(data)) {
      return null;
    }
    var rows = "";
    Object.keys(data).forEach(function (key) {
      var value = data[key];
      var text = "";
      if (value !== null && typeof value === "object") {
        text = JSON.stringify(value); // a list or a nested table
      } else if (value != null) {
        text = String(value);
      }
      rows += "<tr><td>" + escapeHtml(key) + "</td><td>" + escapeHtml(text) + "</td></tr>";
    });
    if (!rows) {
      return null;
    }
    return { html: "<table>" + rows + "</table>\n", body: src.slice(match[0].length), lines: match[0].split("\n").length - 1 };
  }

  katemark.taskLists = taskLists;
  katemark.frontMatterTable = frontMatterTable;
})();
