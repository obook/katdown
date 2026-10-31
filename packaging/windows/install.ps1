<#
.SYNOPSIS
Install or remove Katemark in a Kate for Windows installation.

.DESCRIPTION
Katemark is a KTextEditor plugin whose preview is a QWebEngineView, and Kate for Windows ships no
Qt WebEngine. A plugin DLL's transitive imports resolve from the host process directory, so the
WebEngine runtime has to sit next to kate.exe rather than next to the plugin. This script copies
the payload tree over the Kate install and records every file it wrote, so -Uninstall removes only
those.

It refuses a Kate whose Qt differs from the payload's: Kate would not load the plugin and would not
report it.

.PARAMETER KateDir
Root of the Kate installation (the directory containing bin\kate.exe). Auto-detected when omitted.

.PARAMETER Uninstall
Remove the files listed in the manifest written by a previous install.

.PARAMETER Force
Overwrite destination files that this script did not install. Off by default, so the script does not
replace the WebEngine a future Kate may ship.

.EXAMPLE
.\install.ps1
.EXAMPLE
.\install.ps1 -KateDir 'D:\Kate' -WhatIf
.EXAMPLE
.\install.ps1 -Uninstall
#>
#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess, ConfirmImpact = 'Medium')]
param(
    [string]$KateDir,
    [switch]$Uninstall,
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

# Qt minor series the payload was built against. A plugin built for 6.11 loads fine against a
# 6.11.x Kate; across a minor bump Qt makes no such promise, and WebEngine is the strictest of all.
$RequiredQtMinor = '6.11'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PayloadDir = Join-Path $ScriptDir 'payload'
$ManifestRel = 'bin\kf6\ktexteditor\katemark-manifest.txt'

function Fail([string]$Message) {
    throw $Message
}

function Test-Elevated {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    return ([Security.Principal.WindowsPrincipal]$id).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Find-KateDir {
    $candidates = @()
    foreach ($key in @(
            'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*',
            'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*')) {
        foreach ($entry in (Get-ItemProperty $key -ErrorAction SilentlyContinue)) {
            if (($entry.PSObject.Properties.Name -contains 'DisplayName') -and $entry.DisplayName -like 'Kate*') {
                if ($entry.PSObject.Properties.Name -contains 'InstallLocation' -and $entry.InstallLocation) {
                    $candidates += $entry.InstallLocation
                }
            }
        }
    }
    $candidates += "$env:ProgramFiles\Kate"
    $candidates += "$env:LOCALAPPDATA\Programs\Kate"

    foreach ($c in $candidates) {
        if ($c -and (Test-Path (Join-Path $c 'bin\kate.exe'))) {
            return (Resolve-Path $c).Path
        }
    }
    return $null
}

function Assert-KateDir([string]$Dir) {
    if (-not (Test-Path (Join-Path $Dir 'bin\kate.exe'))) {
        Fail "No bin\kate.exe under '$Dir'. Point -KateDir at the folder that contains bin\kate.exe."
    }
    if ($Dir -like '*\WindowsApps\*') {
        Fail "'$Dir' is a Microsoft Store install. Its directory is ACL-locked and cannot take a plugin. Install Kate from the installer at kate-editor.org instead."
    }
}

function Assert-QtVersion([string]$Dir) {
    $qtCore = Join-Path $Dir 'bin\Qt6Core.dll'
    if (-not (Test-Path $qtCore)) {
        Fail "No bin\Qt6Core.dll under '$Dir'; this does not look like a Kate installation."
    }
    $version = (Get-Item $qtCore).VersionInfo.FileVersion
    if (-not $version) {
        Fail "Could not read a version from '$qtCore'."
    }
    $parts = $version.Split('.')
    $minor = "$($parts[0]).$($parts[1])"
    if ($minor -ne $RequiredQtMinor) {
        Fail @"
Kate at '$Dir' ships Qt $version, and this package was built against Qt $RequiredQtMinor.x.
Installing it would produce a plugin that never loads, with no error shown in Kate.
Grab the katemark build matching your Kate, or update Kate.
"@
    }
    return $version
}

function Get-PayloadFiles {
    if (-not (Test-Path $PayloadDir)) {
        Fail "No payload directory next to this script (expected '$PayloadDir')."
    }
    $files = Get-ChildItem -Path $PayloadDir -Recurse -File
    if (-not $files) {
        Fail "Payload directory '$PayloadDir' is empty."
    }
    return $files
}

function Get-RelativePath([string]$Root, [string]$FullPath) {
    $rootFull = (Resolve-Path $Root).Path.TrimEnd('\')
    return $FullPath.Substring($rootFull.Length + 1)
}

# A file that was Kate's before -Force overwrote it is listed as "kate:<path>": an update
# keeps it without asking again, and -Uninstall leaves it, or Kate would break.
$KatePrefix = 'kate:'

function Read-Manifest([string]$Dir) {
    $path = Join-Path $Dir $ManifestRel
    if (-not (Test-Path $path)) {
        return @()
    }
    return @(Get-Content -LiteralPath $path | Where-Object { $_.Trim() -ne '' })
}

function Invoke-Install([string]$Dir) {
    $qtVersion = Assert-QtVersion $Dir
    $payload = Get-PayloadFiles
    $previous = Read-Manifest $Dir
    $known = @($previous | ForEach-Object { if ($_.StartsWith($KatePrefix)) { $_.Substring($KatePrefix.Length) } else { $_ } })

    # Never overwrite a file that is not ours: a later Kate could start shipping WebEngine itself,
    # and replacing its copy with ours would break Kate.
    $collisions = @()
    foreach ($file in $payload) {
        $rel = Get-RelativePath $PayloadDir $file.FullName
        $dest = Join-Path $Dir $rel
        if ((Test-Path $dest) -and ($known -notcontains $rel)) {
            $collisions += $rel
        }
    }
    if ($collisions.Count -gt 0 -and -not $Force) {
        $list = ($collisions | Select-Object -First 10) -join "`n  "
        Fail @"
These files already exist in '$Dir' and were not put there by Katemark:
  $list
$(if ($collisions.Count -gt 10) { "  ... and $($collisions.Count - 10) more`n" })Re-run with -Force to overwrite them.
"@
    }

    Write-Host "Installing Katemark into $Dir (Kate's Qt $qtVersion)"
    $written = New-Object System.Collections.Generic.List[string]
    foreach ($file in $payload) {
        $rel = Get-RelativePath $PayloadDir $file.FullName
        if (($collisions -contains $rel) -or ($previous -contains "$KatePrefix$rel")) {
            $written.Add("$KatePrefix$rel")
        } else {
            $written.Add($rel)
        }
    }

    # Write the manifest before copying: if a copy fails partway (disk full, a locked DLL),
    # -Uninstall still knows what to remove. It tolerates entries that never reached the disk.
    $manifestPath = Join-Path $Dir $ManifestRel
    if ($PSCmdlet.ShouldProcess($manifestPath, 'Write manifest')) {
        $manifestDir = Split-Path -Parent $manifestPath
        if (-not (Test-Path $manifestDir)) {
            New-Item -ItemType Directory -Path $manifestDir -Force | Out-Null
        }
        Set-Content -LiteralPath $manifestPath -Value $written -Encoding UTF8
    }

    foreach ($file in $payload) {
        $rel = Get-RelativePath $PayloadDir $file.FullName
        $dest = Join-Path $Dir $rel
        $destDir = Split-Path -Parent $dest
        if ($PSCmdlet.ShouldProcess($dest, 'Copy')) {
            if (-not (Test-Path $destDir)) {
                New-Item -ItemType Directory -Path $destDir -Force | Out-Null
            }
            Copy-Item -LiteralPath $file.FullName -Destination $dest -Force
        }
    }

    # Remove what an older payload shipped and this one does not: an uninstall would miss it.
    $orphans = @($previous | Where-Object { ($written -notcontains $_) -and -not $_.StartsWith($KatePrefix) })
    foreach ($rel in $orphans) {
        $path = Join-Path $Dir $rel
        if ((Test-Path $path) -and $PSCmdlet.ShouldProcess($path, 'Remove (left over from a previous Katemark)')) {
            Remove-Item -LiteralPath $path -Force
        }
    }

    Write-Host "Installed $($written.Count) files."
    if ($orphans.Count -gt 0) {
        Write-Host "Removed $($orphans.Count) files left over from a previous version."
    }
    Write-Host "Enable it in Kate: Settings, then Configure Kate, then Plugins, then check Katemark."
}

function Invoke-Uninstall([string]$Dir) {
    $manifestPath = Join-Path $Dir $ManifestRel
    $entries = Read-Manifest $Dir
    if ($entries.Count -eq 0) {
        Fail "No Katemark manifest at '$manifestPath'. Nothing recorded as installed in '$Dir'."
    }

    $removed = 0
    $dirs = New-Object System.Collections.Generic.HashSet[string]
    foreach ($rel in $entries) {
        if ($rel.StartsWith($KatePrefix)) {
            continue
        }
        $path = Join-Path $Dir $rel
        if (Test-Path $path) {
            if ($PSCmdlet.ShouldProcess($path, 'Remove')) {
                Remove-Item -LiteralPath $path -Force
            }
            $removed++
        }
        [void]$dirs.Add((Split-Path -Parent $path))
    }
    if ($PSCmdlet.ShouldProcess($manifestPath, 'Remove manifest')) {
        Remove-Item -LiteralPath $manifestPath -Force
    }

    # Only directories we may have created, deepest first, and only while they are empty.
    foreach ($d in ($dirs | Sort-Object -Property Length -Descending)) {
        if ((Test-Path $d) -and -not (Get-ChildItem -LiteralPath $d -Force)) {
            if ($PSCmdlet.ShouldProcess($d, 'Remove empty directory')) {
                Remove-Item -LiteralPath $d -Force
            }
        }
    }

    Write-Host "Removed $removed files from $Dir"
}

if (-not $KateDir) {
    $KateDir = Find-KateDir
    if (-not $KateDir) {
        Fail "Could not find Kate. Pass -KateDir with the folder that contains bin\kate.exe."
    }
    Write-Host "Found Kate at $KateDir"
} else {
    if (-not (Test-Path $KateDir)) {
        Fail "'$KateDir' does not exist."
    }
    $KateDir = (Resolve-Path $KateDir).Path
}

Assert-KateDir $KateDir

if (-not (Test-Elevated) -and $KateDir.StartsWith($env:ProgramFiles, [StringComparison]::OrdinalIgnoreCase)) {
    Fail "'$KateDir' is under Program Files, so this needs an elevated PowerShell. Re-run it as administrator."
}

$kateRunning = @(Get-Process -Name 'kate' -ErrorAction SilentlyContinue)
if ($kateRunning.Count -gt 0) {
    Fail "Kate is running (pid $($kateRunning[0].Id)). Close it first: Windows keeps loaded DLLs locked."
}

if ($Uninstall) {
    Invoke-Uninstall $KateDir
} else {
    Invoke-Install $KateDir
}
