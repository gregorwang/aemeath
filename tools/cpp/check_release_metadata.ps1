param(
  [string]$ConfigHeaderPath = "src_cpp/runtime/app_config.h",
  [string]$InstallerScriptPath = "installer/cybercompanioncpp.iss",
  [string]$VersionFilePath = "out/package/windows-ninja-release/version.json"
)

$ErrorActionPreference = "Stop"

function Read-FileText([string]$path) {
  $resolved = (Resolve-Path $path).Path
  return Get-Content $resolved -Raw
}

function Read-AppVersion([string]$path) {
  $content = Read-FileText $path
  $match = [regex]::Match($content, 'version\s*=\s*QStringLiteral\("([^"]+)"\)')
  if (-not $match.Success) {
    throw "Failed to resolve app version from $path"
  }
  return $match.Groups[1].Value
}

function Read-InstallerVersion([string]$path) {
  $content = Read-FileText $path
  $match = [regex]::Match($content, '#define\s+MyAppVersion\s+"([^"]+)"')
  if (-not $match.Success) {
    throw "Failed to resolve installer version from $path"
  }
  return $match.Groups[1].Value
}

$appVersion = Read-AppVersion $ConfigHeaderPath
$installerVersion = Read-InstallerVersion $InstallerScriptPath
$versionManifest = Get-Content (Resolve-Path $VersionFilePath).Path -Raw | ConvertFrom-Json

if ($installerVersion -ne $appVersion) {
  throw "Installer version '$installerVersion' does not match app version '$appVersion'."
}
if ($versionManifest.version -ne $appVersion) {
  throw "version.json version '$($versionManifest.version)' does not match app version '$appVersion'."
}
if ($versionManifest.phase -ne $appVersion) {
  throw "version.json phase '$($versionManifest.phase)' does not match app version '$appVersion'."
}
if ([string]::IsNullOrWhiteSpace($versionManifest.commit_hash) -or $versionManifest.commit_hash -eq "unknown") {
  throw "version.json commit_hash is missing or unknown."
}

Write-Host "Release metadata check passed."
Write-Host "App version      : $appVersion"
Write-Host "Installer version: $installerVersion"
Write-Host "Commit hash      : $($versionManifest.commit_hash)"
