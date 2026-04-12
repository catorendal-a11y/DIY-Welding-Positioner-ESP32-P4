# Patch PlatformIO IDE: remove ms-vscode.cpptools from extensionDependencies so the
# extension can load when the Microsoft C/C++ dependency is unavailable in the host.
#
# Default extensions folder is VS Code under %USERPROFILE%\.vscode\extensions.
# If your editor stores extensions elsewhere, pass -ExtensionsRoot "C:\path\to\extensions".
#
# How to run:
#   powershell -ExecutionPolicy Bypass -File ".\scripts\fix_platformio_extensions.ps1"
# Then reload the editor window (Command Palette: Developer: Reload Window).

#Requires -Version 5.1
param(
  [switch]$WhatIf,
  [string]$ExtensionsRoot = ""
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($ExtensionsRoot)) {
  $extensionsRoot = Join-Path $env:USERPROFILE ".vscode\extensions"
} else {
  $extensionsRoot = $ExtensionsRoot
}

if (-not (Test-Path -LiteralPath $extensionsRoot)) {
  Write-Error "Extensions folder not found: $extensionsRoot`nUse -ExtensionsRoot if extensions are installed elsewhere."
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
  Where-Object { $_.Name -like "davidgomes.platformio-ide-*" }
if ($davidGomes) {
  Write-Host ""
  Write-Host "=== MERK ===" -ForegroundColor Yellow
  Write-Host "Du har en uoffisiell PlatformIO-pakke ($($davidGomes[0].Name))."
  Write-Host "Den er ikke den offisielle utvidelsen. Versjon 0.0.1 henger ofte pa 'Initializing Core'."
  Write-Host "Anbefaling:"
  Write-Host "  1. Avinstaller den uoffisielle PlatformIO-pakken (davidgomes)"
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

Installer 'PlatformIO IDE' (ID: platformio.platformio-ide), fjern uoffisielle varianter,
og kjor skriptet pa nytt (evt. med -ExtensionsRoot).
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
Write-Host "Ferdig. Last inn editoren pa nytt (Reload Window)." -ForegroundColor Green
