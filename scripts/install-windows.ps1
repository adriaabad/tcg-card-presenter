param(
  [string]$ObsRoot = "C:\Program Files\obs-studio"
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$BuildDll = Join-Path $RepoRoot "build_x64\RelWithDebInfo\tcg-card-presenter.dll"
$SourceData = Join-Path $RepoRoot "data"
$QtTlsPlugins = Join-Path $RepoRoot ".deps\obs-deps-qt6-2025-07-11-x64\plugins\tls"
$PluginDll = Join-Path $ObsRoot "obs-plugins\64bit\tcg-card-presenter.dll"
$PluginData = Join-Path $ObsRoot "data\obs-plugins\tcg-card-presenter"
$ObsBin = Join-Path $ObsRoot "bin\64bit"
$ObsTlsPlugins = Join-Path $ObsBin "tls"

if (!(Test-Path -LiteralPath $BuildDll)) {
  throw "Plugin DLL not found. Build it first: $BuildDll"
}

if (!(Test-Path -LiteralPath $SourceData)) {
  throw "Plugin data not found: $SourceData"
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $PluginDll) | Out-Null
New-Item -ItemType Directory -Force -Path $PluginData | Out-Null

Copy-Item -LiteralPath $BuildDll -Destination $PluginDll -Force
Copy-Item -LiteralPath (Join-Path $SourceData "effects") -Destination $PluginData -Recurse -Force
Copy-Item -LiteralPath (Join-Path $SourceData "locale") -Destination $PluginData -Recurse -Force

if (Test-Path -LiteralPath $QtTlsPlugins) {
  New-Item -ItemType Directory -Force -Path $ObsTlsPlugins | Out-Null
  Copy-Item -LiteralPath (Join-Path $QtTlsPlugins "qcertonlybackend.dll") -Destination $ObsTlsPlugins -Force
  Copy-Item -LiteralPath (Join-Path $QtTlsPlugins "qschannelbackend.dll") -Destination $ObsTlsPlugins -Force
}

Write-Host "Installed TCG Card Presenter to $ObsRoot"
