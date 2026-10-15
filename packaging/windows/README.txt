Katemark for Kate on Windows
============================

Katemark is a Kate plugin that renders a GitHub-styled preview of the Markdown file you are
editing, in a panel beside the editor.


What is in here
---------------

  install.ps1   installs and uninstalls
  payload\      the plugin, its translations and the Qt WebEngine runtime it needs

Kate for Windows ships no Qt WebEngine and the preview is a web view, so the runtime comes with the
plugin. It installs next to kate.exe, where Windows looks for the libraries a plugin needs.


Requirements
------------

  * Kate for Windows from the installer at https://kate-editor.org/get-it/
  * Kate built on Qt 6.11.x. install.ps1 refuses another version: Kate would not load the plugin
    and would not say why.

The Microsoft Store version of Kate is not supported: Windows locks its install directory.


Installing
----------

Close Kate first, then in an ELEVATED PowerShell (Kate normally lives under Program Files):

    powershell -ExecutionPolicy Bypass -File install.ps1

It finds Kate on its own. If it cannot, or you have several installs, name one:

    powershell -ExecutionPolicy Bypass -File install.ps1 -KateDir "D:\Kate"

Add -WhatIf to see what it would do without touching anything.

Then start Kate and turn the plugin on: Settings, then Configure Kate, then Plugins, then check
Katemark. Open a Markdown file and press Ctrl+Shift+M.


Uninstalling
------------

    powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall

It removes exactly the files it installed, tracked in a manifest written at install time, and
leaves Kate's own files alone.


If the plugin does not appear in Kate's plugin list
---------------------------------------------------

Kate does not report a plugin that failed to load. Check that bin\kf6\ktexteditor\katemark.dll
exists under your Kate install, that bin\Qt6WebEngineCore.dll is there next to kate.exe, and that
Kate's bin\Qt6Core.dll is version 6.11.x.


https://github.com/obook/katemark
