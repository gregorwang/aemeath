param(
  [string]$ExePath = "out/package/windows-ninja-release/CyberCompanionCpp.exe",
  [string]$LogPath = "$env:LOCALAPPDATA\CyberCompanionCpp\logs\app.log",
  [string]$VersionFilePath = "out/package/windows-ninja-release/version.json",
  [int]$TimeoutSeconds = 6,
  [switch]$KeepRunning
)

$ErrorActionPreference = "Stop"

if ($TimeoutSeconds -lt 1) {
  throw "TimeoutSeconds must be >= 1."
}

$resolvedExePath = (Resolve-Path $ExePath).Path
$workingDir = Split-Path $resolvedExePath -Parent
$resolvedVersionFilePath = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $VersionFilePath))

if (-not (Test-Path $resolvedVersionFilePath)) {
  throw "Smoke check failed: version file not found at $resolvedVersionFilePath"
}

$versionManifest = Get-Content $resolvedVersionFilePath -Raw | ConvertFrom-Json
if ([string]::IsNullOrWhiteSpace($versionManifest.version)) {
  throw "Smoke check failed: version.json is missing a version."
}
if ([string]::IsNullOrWhiteSpace($versionManifest.phase)) {
  throw "Smoke check failed: version.json is missing a phase/version marker."
}
if ($versionManifest.phase -ne $versionManifest.version) {
  throw "Smoke check failed: version.json phase '$($versionManifest.phase)' does not match version '$($versionManifest.version)'."
}
if ([string]::IsNullOrWhiteSpace($versionManifest.commit_hash) -or $versionManifest.commit_hash -eq "unknown") {
  throw "Smoke check failed: version.json commit_hash was not resolved."
}

$exeVersion = (Get-Item $resolvedExePath).VersionInfo.FileVersion
if ([string]::IsNullOrWhiteSpace($exeVersion)) {
  throw "Smoke check failed: executable file version is missing."
}
$normalizedExeVersion = ($exeVersion -split '\s+')[0]
if ($normalizedExeVersion -ne $versionManifest.version) {
  throw "Smoke check failed: executable file version '$normalizedExeVersion' does not match version.json '$($versionManifest.version)'."
}

$existing = Get-Process CyberCompanionCpp -ErrorAction SilentlyContinue
if ($existing) {
  throw "CyberCompanionCpp is already running. Stop the existing process before smoke check."
}

if (Test-Path $LogPath) {
  Remove-Item $LogPath -Force
}

$process = Start-Process -FilePath $resolvedExePath -WorkingDirectory $workingDir -PassThru
Start-Sleep -Seconds $TimeoutSeconds

$isRunning = $null -ne (Get-Process -Id $process.Id -ErrorAction SilentlyContinue)
if (-not $isRunning) {
  throw "Smoke check failed: process exited before timeout."
}

if (-not (Test-Path $LogPath)) {
  if (-not $KeepRunning) {
    Stop-Process -Id $process.Id -Force
  }
  throw "Smoke check failed: log file not found at $LogPath"
}

$logLines = Get-Content $LogPath
$logText = $logLines | Out-String

if ($logText -notmatch "\[INFO\]\s+logger initialized") {
  if (-not $KeepRunning) {
    Stop-Process -Id $process.Id -Force
  }
  throw "Smoke check failed: startup log marker not found."
}

if ($logText -match "\[FATAL\]") {
  if (-not $KeepRunning) {
    Stop-Process -Id $process.Id -Force
  }
  throw "Smoke check failed: fatal log entry detected."
}

$forbiddenPatterns = @(
  "No QtMultimedia backends found",
  "Failed to initialize QMediaPlayer"
)

foreach ($pattern in $forbiddenPatterns) {
  if ($logText -match [Regex]::Escape($pattern)) {
    if (-not $KeepRunning) {
      Stop-Process -Id $process.Id -Force
    }
    throw "Smoke check failed: detected forbidden runtime log pattern: $pattern"
  }
}

if (-not $KeepRunning) {
  Stop-Process -Id $process.Id -Force
}

Write-Host "Smoke check passed."
Write-Host "Executable : $resolvedExePath"
Write-Host "Log file   : $LogPath"
Write-Host "Version    : $resolvedVersionFilePath"
Write-Host "--- log tail ---"
$logLines | Select-Object -Last 40
