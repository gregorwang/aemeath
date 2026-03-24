param(
  [string]$BuildDir = "out/build/windows-ninja-release",
  [string]$Configuration = "",
  [ValidateSet("Debug", "Release")]
  [string]$DeployMode = "Release",
  [string]$OutputDir = "out/package/windows-ninja-release"
)

$ErrorActionPreference = "Stop"

function Resolve-QtTool([string]$toolName) {
  if ($env:QT_ROOT) {
    $candidate = Join-Path $env:QT_ROOT "bin\$toolName"
    if (Test-Path $candidate) { return $candidate }
  }
  $cmd = Get-Command $toolName -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  throw "$toolName not found. Set QT_ROOT or add Qt bin to PATH."
}

function Resolve-CppAppVersion {
  $configHeader = Join-Path (Get-Location) "src_cpp\runtime\app_config.h"
  if (-not (Test-Path $configHeader)) {
    return "0.1.0"
  }
  $match = Select-String -Path $configHeader -Pattern 'version\s*=\s*QStringLiteral\("([^"]+)"\)' | Select-Object -First 1
  if ($match -and $match.Matches.Count -gt 0) {
    return $match.Matches[0].Groups[1].Value
  }
  return "0.1.0"
}

function Read-VersionMetadata {
  $versionPath = Join-Path (Get-Location) "version.json"
  if (-not (Test-Path $versionPath)) {
    return @{}
  }
  try {
    return Get-Content $versionPath -Raw | ConvertFrom-Json -AsHashtable
  } catch {
    return @{}
  }
}

$windeployqt = Resolve-QtTool "windeployqt.exe"
$candidateTargets = @()
if ($Configuration) {
  $candidateTargets += (Join-Path $BuildDir "$Configuration\CyberCompanionCpp.exe")
}
$candidateTargets += (Join-Path $BuildDir "CyberCompanionCpp.exe")

$target = $candidateTargets | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $target) {
  throw "Executable not found. Checked: $($candidateTargets -join ', ')"
}

$resolvedBuildDir = (Resolve-Path $BuildDir).Path
$resolvedOutputDir = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDir))
if (Test-Path $resolvedOutputDir) {
  Remove-Item -Recurse -Force $resolvedOutputDir
}
New-Item -ItemType Directory -Path $resolvedOutputDir | Out-Null

$stagedExe = Join-Path $resolvedOutputDir "CyberCompanionCpp.exe"
Copy-Item $target $stagedExe -Force

foreach ($resourceDir in @("assets", "characters", "recorded_paths")) {
  $sourcePath = Join-Path (Get-Location) $resourceDir
  if (Test-Path $sourcePath) {
    Copy-Item $sourcePath (Join-Path $resolvedOutputDir $resourceDir) -Recurse -Force
  }
}

$existingVersion = Read-VersionMetadata
$appVersion = Resolve-CppAppVersion
$stagedVersion = [ordered]@{
  version = $appVersion
  update_url = if ($existingVersion.Contains("update_url")) { [string]$existingVersion["update_url"] } else { "https://api.github.com/repos/gregorwang/aemeath/releases/latest" }
  build_date = Get-Date -Format "yyyy-MM-dd"
  python_version = if ($existingVersion.Contains("python_version")) { [string]$existingVersion["python_version"] } else { "" }
  commit_hash = if ($existingVersion.Contains("commit_hash")) { [string]$existingVersion["commit_hash"] } else { "unknown" }
  phase = $appVersion
}
$stagedVersion | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $resolvedOutputDir "version.json") -Encoding UTF8

$deployArgs = @()
if ($DeployMode -eq "Debug") {
  $deployArgs += "--debug"
} else {
  $deployArgs += "--release"
}
$deployArgs += "--compiler-runtime"
$deployArgs += "--add-plugin-types"
$deployArgs += "multimedia"
$deployArgs += "--include-plugins"
$deployArgs += "ffmpegmediaplugin,windowsmediaplugin"
$deployArgs += $stagedExe

& $windeployqt @deployArgs

Write-Host "Staged deployment to: $resolvedOutputDir"
