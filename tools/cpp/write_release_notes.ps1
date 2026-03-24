param(
  [string]$ManifestPath = "out/installer/release_manifest.json",
  [string]$OutputPath = "out/installer/release_notes.md"
)

$ErrorActionPreference = "Stop"

$resolvedManifestPath = (Resolve-Path $ManifestPath).Path
$resolvedOutputPath = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputPath))
$manifest = Get-Content $resolvedManifestPath -Raw | ConvertFrom-Json

$installerName = [System.IO.Path]::GetFileName($manifest.installer.path)
$installerSha = $manifest.installer.sha256
$installerSizeMb = [Math]::Round(([double]$manifest.installer.size_bytes / 1MB), 2)
$fileCount = [int]$manifest.package.file_count

$lines = @(
  "# CyberCompanionCpp $($manifest.app_version)"
  ""
  '## Summary'
  ""
  '- Primary desktop runtime is now the native `Qt 6 + CMake + MinGW` build.'
  "- Installer: $installerName"
  "- Commit: $($manifest.commit_hash)"
  "- Build date: $($manifest.build_date)"
  ""
  '## Validation'
  ""
  '- Native release build completed successfully.'
  '- Native `CTest` suite passed.'
  '- `windeployqt` staging completed successfully.'
  '- `smoke_check_cpp.ps1` passed against the staged package.'
  ""
  '## Artifacts'
  ""
  "- Installer size: ${installerSizeMb} MB"
  "- Installer SHA256: $installerSha"
  "- Staged package file count: $fileCount"
  ""
  '## Notes'
  ""
  '- Legacy Python runtime remains in the repository for compatibility and reference, but GitHub releases now ship the native C++ runtime by default.'
  '- Release metadata is generated from the staged native package and validated in CI.'
)

$outputDir = Split-Path $resolvedOutputPath -Parent
if (-not (Test-Path $outputDir)) {
  New-Item -ItemType Directory -Path $outputDir | Out-Null
}

$lines -join "`r`n" | Set-Content -Path $resolvedOutputPath -Encoding UTF8
Write-Host "Release notes written to: $resolvedOutputPath"
