# Roadmap

Ideas and open work, not scheduled yet.

## Distribution packages

Today CPack makes the `.deb` files and they sit on the GitHub release page,
so nobody gets an update through `apt`.

### Ubuntu, through Debian

Ubuntu copies Debian unstable into `universe`, so the way in is a Debian
package. A release that is already out never gets it: 26.04 is too late and
27.04 needs the package in unstable before the Debian import freeze, usually
in late February.

- [ ] Sort out the bundled JavaScript (next section).
- [ ] Write a `debian/` directory and get it clean under `lintian`. The
      CPack package does not count: Debian builds from a source package.
- [ ] File an ITP bug against `wnpp` (`reportbug wnpp`).
- [ ] Find a sponsor, since only a Debian developer can upload. Ask the
      Debian Qt/KDE team first (`debian-qt-kde` list, salsa.debian.org).
      Otherwise upload to mentors.debian.net and file an RFS bug.
- [ ] Get through the NEW queue, where the licenses are reviewed.

Ubuntu has its own door, a `needs-packaging` bug on Launchpad and a MOTU
sponsor, but it recommends going through Debian.

### Bundled JavaScript

Debian rejects a minified file that comes without its source and `data/js`
holds 12 of them. Checked against Debian unstable on 2026-10-06:

| Library | Bundled | Debian unstable |
|---|---|---|
| KaTeX | 0.19.0 | `libjs-katex`, `fonts-katex` 0.16.10 |
| highlight.js | 11.10.0 | `libjs-highlight.js` 10.7.3 |
| markdown-it | 14.1.0 | `node-markdown-it` 10.0.0, no `libjs` package |
| markdown-it-sub | 2.0.0 | `libjs-markdown-it-sub` 1.0.0 |
| markdown-it-sup | 2.0.0 | none |
| js-yaml | 4.1.0 | `node-js-yaml` 4.3.2 |
| markdown-it-container, -footnote, -ins, -mark | 4.0.0 | none |
| markdown-it-emoji | 3.1.0 | none |
| Mermaid | 12.1.0 | `node-mermaid` 9.2.2 |

Debian has an older version of KaTeX, highlight.js, markdown-it,
markdown-it-sub and Mermaid, and Katemark has only been tested with the
bundled ones. The `node-markdown-it` package is numbered 22.2.3 but its
changelog stops at markdown-it 10.0.0. Ubuntu 24.04 still had
`libjs-markdown-it` and `libjs-markdown-it-sup`; Debian has dropped both
since.

- [x] Run the same check against Debian unstable and compare the packaged
      versions with the bundled ones.
- [ ] For each library with no package or an older one, either ship the
      unminified source next to the file or get the bundled version
      packaged first. Mermaid is the largest and brings many dependencies
      of its own.

### KDE neon

neon builds KDE's own software, hosted on invent.kde.org, and takes no
outside project. Katemark would have to come in through KDE:

- [ ] Ask the Kate maintainers whether the plugin could live in `addons/`
      of `utilities/kate`. It would then ship with Kate everywhere.
      `kate.git` takes LGPL-2.0-or-later or MIT, and Katemark is
      GPL-3.0-or-later through Katdown. On 2026-10-12 the author of Katdown
      agreed to LGPL-2.0-or-later for the code Katemark uses
      ([issue](https://github.com/uwuclxdy/katdown/issues/3)). The question
      to the maintainers is open on
      [KDE Discuss](https://discuss.kde.org/t/katemark-a-markdown-preview-for-kate-beside-the-editor-with-scroll-sync/50674/21).
- [ ] Otherwise apply to the
      [KDE incubator](https://community.kde.org/Incubator). Katemark
      becomes a KDE project and neon builds it.

### In the meantime

- [ ] Open a Launchpad PPA so that Ubuntu 26.04 users get updates through
      `apt`. It cannot serve neon while neon sits on 24.04: that archive has
      Qt 6.4 and no KF6 TextEditor, where Katemark needs Qt 6.5 and KF6.
