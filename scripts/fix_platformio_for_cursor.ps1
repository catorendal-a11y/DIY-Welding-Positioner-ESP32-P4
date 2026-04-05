# Patch PlatformIO IDE for Cursor: removes ms-vscode.cpptools from extensionDependencies
# so the OFFICIAL extension can activate (Microsoft C/C++ is blocked in Cursor).
#
# This does NOT fix "DavidGomes PlatformIO for Cursor" (different package; use official PIO below).
#
# How to run:
#   powershell -ExecutionPolicy Bypass -File ".\scripts\fix_platformio_for_cursor.ps1"
# Then: Cursor -> Developer: Reload Window

#Requires -Version 5.1
param([switch]$WhatIf)

$ErrorActionPreference = "Stop"
$extensionsRoot = Join-Path $env:USERPROFILE ".cursor\extensions"
if (-not (Test-Path -LiteralPath $extensionsRoot)) {
  Write-Error "Cursor extensions folder not found: $extensionsRoot"
}

function Test-PackageJsonHasMsCppTools([string]$jsonPath) {
  $t = Get-Content -LiteralPath $jsonPath -Raw -Encoding UTF8
  return $t -match '"extensionDependencies"\s*:\s*\[[^\]]*ms-vscode\.cpptools'
}

function Patch-PackageJson([string]$packageJsonPath) {
  $obj = Get-Content -LiteralPath $packageJsonPath -Raw -Encoding UTF8 | ConvertFrom-Json
  if ($null -eq $obj.extensionDependencies) { return $false }
  $before = @($obj.extensionDependencies)
  $after = @($before | Where-Object { $_ -ne "ms-vscode.cpptools" })
  if ($after.Count -eq $before.Count) { return $false }

  if ($after.Count -eq 0) {
    $obj.PSObject.Properties.Remove("extensionDependencies")
  } else {
    $obj.extensionDependencies = $after
  }

  if (-not $WhatIf) {
    Copy-Item -LiteralPath $packageJsonPath -Destination "$packageJsonPath.bak" -Force
    $out = $obj | ConvertTo-Json -Depth 100
    $utf8 = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($packageJsonPath, $out, $utf8)
  }
  return $true
}

function Patch-VsixManifest([string]$manifestPath) {
  if (-not (Test-Path -LiteralPath $manifestPath)) { return $false }
  $m = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8
  $pattern = '\r?\n\s*<Property Id="Microsoft\.VisualStudio\.Code\.ExtensionDependencies" Value="ms-vscode\.cpptools"\s*/>'
  if ($m -notmatch $pattern) { return $false }
  if (-not $WhatIf) {
    Copy-Item -LiteralPath $manifestPath -Destination "$manifestPath.bak" -Force
    $m2 = $m -replace $pattern, ""
    $utf8 = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($manifestPath, $m2, $utf8)
  }
  return $true
}

$davidGomes = Get-ChildItem -LiteralPath $extensionsRoot -Directory -ErrorAction SilentlyContinue |
  Where-Object { $_.Name -like "davidgomes.platformio-ide-cursor-*" }
if ($davidGomes) {
  Write-Host ""
  Write-Host "=== MERK ===" -ForegroundColor Yellow
  Write-Host "Du har 'DavidGomes PlatformIO IDE for Cursor' ($($davidGomes[0].Name))."
  Write-Host "Det er IKKE den offisielle utvidelsen. Versjon 0.0.1 henger ofte pa 'Initializing Core'."
  Write-Host "Anbefaling:"
  Write-Host "  1. Extensions -> avinstaller 'PlatformIO IDE for Cursor' (DavidGomes)"
  Write-Host "  2. Installer 'PlatformIO IDE' fra PlatformIO (platformio.platformio-ide)"
  Write-Host "  3. Kjor dette skriptet igjen, deretter Reload Window"
  Write-Host ""
}

$allDirs = @(Get-ChildItem -LiteralPath $extensionsRoot -Directory -ErrorAction SilentlyContinue)
$dirsToPatch = New-Object System.Collections.Generic.List[string]

foreach ($d in $allDirs) {
  $pj = Join-Path $d.FullName "package.json"
  if (-not (Test-Path -LiteralPath $pj)) { continue }
  if ($d.Name -like "platformio.platformio-ide-*") {
    $dirsToPatch.Add($d.FullName) | Out-Null
    continue
  }
  if ($d.Name -like "platformio.*" -and (Test-PackageJsonHasMsCppTools $pj)) {
    $dirsToPatch.Add($d.FullName) | Out-Null
  }
}

if ($dirsToPatch.Count -eq 0) {
  Write-Error @"
Fant ingen offisiell PlatformIO-mappe (platformio.platformio-ide-*) under:
  $extensionsRoot

Installer 'PlatformIO IDE' fra PlatformIO (ID: platformio.platformio-ide), avinstaller DavidGomes-varianten,
og kjor skriptet pa nytt.
"@
}

foreach ($pioDir in ($dirsToPatch | Select-Object -Unique)) {
  Write-Host "Patcher: $pioDir"

  $packageJsonPath = Join-Path $pioDir "package.json"
  $pkgChanged = Patch-PackageJson $packageJsonPath
  if ($pkgChanged) {
    Write-Host "  package.json: oppdatert (backup: package.json.bak)"
  } else {
    Write-Host "  package.json: ingen ms-vscode.cpptools i extensionDependencies (allerede OK?)"
  }

  $manifestPath = Join-Path $pioDir ".vsixmanifest"
  if (Patch-VsixManifest $manifestPath) {
    Write-Host "  .vsixmanifest: oppdatert (backup: .vsixmanifest.bak)"
  }
}

Write-Host ""
Write-Host "Ferdig. I Cursor: Developer: Reload Window" -ForegroundColor Green
