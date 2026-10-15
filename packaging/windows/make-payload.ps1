<#
.SYNOPSIS
Assemble the Windows distribution: the plugin plus the Qt WebEngine runtime Kate does not ship.

.DESCRIPTION
The file list is explicit, because windeployqt deploys the closure a generic Qt app might need (QML
debug tooling, geolocation backends, a PDF module, a bundled VC redist installer), roughly 42 MB of
which katemark never touches. It also fails part way on a Craft layout, because it looks for the
WebEngine .pak files under a resources\ subdirectory that Craft does not create.

DLLs are settled by import analysis: kept when they are a static import of Qt6WebEngineCore or
Qt6WebEngineWidgets, dropped when the plugin still resolved every import with the file absent, on a
real Kate install.

The D3D shader compilers windeployqt offers (d3dcompiler_47.dll, dxcompiler.dll, dxil.dll) are
deliberately not here. Windows ships d3dcompiler_47.dll in System32, so Chromium finds it without
help; dxcompiler.dll and dxil.dll exist in neither Craft nor Kate nor a stock Windows install, so
there is nothing to ship. Chromium falls back to software rendering when it cannot compile D3D
shaders, which is already what happens on hardware that fails GPU init.

Chromium opens the data files by name at runtime, so a link, `dumpbin /dependents`, and
`LoadLibraryExW` all succeed with one missing: only a render test tells. Two were missing once, the
locale pak (wrong directory) and `v8_context_snapshot.bin` (never copied: the hand-written list did
not have it). Run a render test before adding or removing a data file.

Anything Kate already ships is deliberately absent: install.ps1 refuses to overwrite files it did
not put there, so a payload that duplicated Kate's own Qt would refuse to install.

.PARAMETER CraftRoot
Craft installation the files come from (the one that built the plugin).

.PARAMETER PluginDll
Path to the built katemark.dll.

.PARAMETER LocaleDir
The build's compiled translations (build\locale, one <lang>\LC_MESSAGES\katemark.mo per language).

.PARAMETER OutDir
Directory to assemble into. Created if absent, cleared if it already holds a payload.

.PARAMETER Zip
Optional path for a zip of the assembled tree.

.EXAMPLE
.\make-payload.ps1 -CraftRoot C:\CraftRoot -PluginDll build\bin\kf6\ktexteditor\katemark.dll -LocaleDir build\locale -OutDir dist
#>
#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CraftRoot,
    [Parameter(Mandatory)][string]$PluginDll,
    [Parameter(Mandatory)][string]$LocaleDir,
    [Parameter(Mandatory)][string]$OutDir,
    [string]$Zip
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

# source path relative to CraftRoot -> destination path relative to the Kate install root
$Files = [ordered]@{
    # WebEngine itself, and the two Qt modules Qt6WebEngineCore statically imports that Kate lacks
    'bin\Qt6WebEngineCore.dll'              = 'bin\Qt6WebEngineCore.dll'
    'bin\Qt6WebEngineWidgets.dll'           = 'bin\Qt6WebEngineWidgets.dll'
    'bin\Qt6WebChannel.dll'                 = 'bin\Qt6WebChannel.dll'
    'bin\Qt6Positioning.dll'                = 'bin\Qt6Positioning.dll'
    # Chromium's render process. WebEngine spawns this as a separate executable.
    'bin\QtWebEngineProcess.exe'            = 'bin\QtWebEngineProcess.exe'
    # Chromium data files, opened by name at runtime. None appears in an import table, so a link or
    # a LoadLibrary succeeds without them.
    'bin\icudtl.dat'                        = 'bin\icudtl.dat'
    'bin\qtwebengine_resources.pak'         = 'bin\qtwebengine_resources.pak'
    'bin\qtwebengine_resources_100p.pak'    = 'bin\qtwebengine_resources_100p.pak'
    'bin\qtwebengine_resources_200p.pak'    = 'bin\qtwebengine_resources_200p.pak'
    # Without this one V8 aborts at gin/v8_initializer.cc ("Error loading V8 startup snapshot file")
    # ~34 ms in: pages navigate, renderers respawn, and no script runs. The preview looks empty with
    # nothing on stdout.
    'bin\v8_context_snapshot.bin'           = 'bin\v8_context_snapshot.bin'
    # English locale only. The full set is 53 files and 45 MB for strings a markdown preview never
    # shows; Chromium falls back to en-US when a locale is missing.
    # It goes under bin\translations\, not bin\, measured: at bin\qtwebengine_locales\ Chromium
    # logged "locale resources are not loaded" 166 times per run and found nothing.
    'translations\qtwebengine_locales\en-US.pak' = 'bin\translations\qtwebengine_locales\en-US.pak'
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

if (-not (Test-Path $CraftRoot)) {
    throw "No Craft root at '$CraftRoot'."
}
if (-not (Test-Path $PluginDll)) {
    throw "No plugin at '$PluginDll'."
}

$payloadDir = Join-Path $OutDir 'payload'
if (Test-Path $payloadDir) {
    Remove-Item -LiteralPath $payloadDir -Recurse -Force
}
New-Item -ItemType Directory -Path $payloadDir -Force | Out-Null

# List every missing source before failing: after a Qt layout change, one run shows them all.
$missing = @()
foreach ($src in $Files.Keys) {
    if (-not (Test-Path (Join-Path $CraftRoot $src))) {
        $missing += $src
    }
}
if ($missing.Count -gt 0) {
    throw "Missing from '$CraftRoot':`n  $($missing -join "`n  ")"
}

$total = 0
foreach ($src in $Files.Keys) {
    $from = Join-Path $CraftRoot $src
    $to = Join-Path $payloadDir $Files[$src]
    $toDir = Split-Path -Parent $to
    if (-not (Test-Path $toDir)) {
        New-Item -ItemType Directory -Path $toDir -Force | Out-Null
    }
    Copy-Item -LiteralPath $from -Destination $to -Force
    $total += (Get-Item $to).Length
}

$pluginDest = Join-Path $payloadDir 'bin\kf6\ktexteditor\katemark.dll'
New-Item -ItemType Directory -Path (Split-Path -Parent $pluginDest) -Force | Out-Null
Copy-Item -LiteralPath $PluginDll -Destination $pluginDest -Force
$total += (Get-Item $pluginDest).Length

# Kate for Windows keeps its catalogs under bin\data\locale (ECM's LOCALEDIR on WIN32), and KI18n
# looks there. Without them the plugin stays in English in a translated Kate.
$catalogs = @(Get-ChildItem -Path (Join-Path $LocaleDir '*\LC_MESSAGES\katemark.mo') -File -ErrorAction SilentlyContinue)
if ($catalogs.Count -eq 0) {
    throw "No katemark.mo under '$LocaleDir'."
}
foreach ($mo in $catalogs) {
    $lang = $mo.Directory.Parent.Name
    $to = Join-Path $payloadDir "bin\data\locale\$lang\LC_MESSAGES\katemark.mo"
    New-Item -ItemType Directory -Path (Split-Path -Parent $to) -Force | Out-Null
    Copy-Item -LiteralPath $mo.FullName -Destination $to -Force
    $total += $mo.Length
}

Copy-Item -LiteralPath (Join-Path $ScriptDir 'install.ps1') -Destination (Join-Path $OutDir 'install.ps1') -Force
Copy-Item -LiteralPath (Join-Path $ScriptDir 'README.txt') -Destination (Join-Path $OutDir 'README.txt') -Force

$count = $Files.Count + 1 + $catalogs.Count
Write-Host ("Assembled {0} files, {1:N0} bytes, into {2}" -f $count, $total, $payloadDir)

if ($Zip) {
    $zipDir = Split-Path -Parent $Zip
    if ($zipDir -and -not (Test-Path $zipDir)) {
        New-Item -ItemType Directory -Path $zipDir -Force | Out-Null
    }
    if (Test-Path $Zip) {
        Remove-Item -LiteralPath $Zip -Force
    }
    # Compress into a temp file: the usual call site puts the zip inside $OutDir, the directory
    # being zipped, and an archive that is its own input comes out corrupt.
    $staging = Join-Path ([System.IO.Path]::GetTempPath()) ("katemark-" + [System.IO.Path]::GetRandomFileName() + ".zip")
    Compress-Archive -Path (Join-Path $OutDir '*') -DestinationPath $staging -CompressionLevel Optimal
    Move-Item -LiteralPath $staging -Destination $Zip -Force
    Write-Host ("Wrote {0} ({1:N0} bytes)" -f $Zip, (Get-Item $Zip).Length)
}
