param(
  [string]$PackageDir = "out/package/windows-ninja-release",
  [string]$InstallerPath = "out/installer/CyberCompanionCppSetup.exe",
  [string]$OutputPath = "out/installer/release_manifest.json"
)

$ErrorActionPreference = "Stop"

function Get-FileMetadata([string]$path, [string]$root = "") {
  $resolved = (Resolve-Path $path).Path
  $item = Get-Item $resolved
  $hash = (Get-FileHash -Path $resolved -Algorithm SHA256).Hash.ToLowerInvariant()
  $relativePath = if ([string]::IsNullOrWhiteSpace($root)) {
    $item.Name
  } else {
    $normalizedRoot = ((Resolve-Path $root).Path).TrimEnd('\')
    if ($resolved.StartsWith($normalizedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
      $resolved.Substring($normalizedRoot.Length).TrimStart('\')
    } else {
      $item.Name
    }
  }

  return [ordered]@{
    path = $relativePath.Replace('\', '/')
    size_bytes = [int64]$item.Length
    sha256 = $hash
  }
}

$resolvedPackageDir = (Resolve-Path $PackageDir).Path
$resolvedInstallerPath = (Resolve-Path $InstallerPath).Path
$resolvedOutputPath = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputPath))
$versionManifestPath = Join-Path $resolvedPackageDir "version.json"

if (-not (Test-Path $versionManifestPath)) {
  throw "version.json not found at $versionManifestPath"
}

$versionManifest = Get-Content $versionManifestPath -Raw | ConvertFrom-Json
$packageFiles = Get-ChildItem -Path $resolvedPackageDir -Recurse -File | Sort-Object FullName
$packageMetadata = @()
foreach ($file in $packageFiles) {
  $packageMetadata += [pscustomobject](Get-FileMetadata -path $file.FullName -root $resolvedPackageDir)
}

$installerMetadata = [pscustomobject](Get-FileMetadata -path $resolvedInstallerPath)

$manifest = [ordered]@{
  app_version = $versionManifest.version
  commit_hash = $versionManifest.commit_hash
  build_date = $versionManifest.build_date
  installer = $installerMetadata
  package = [ordered]@{
    root = $resolvedPackageDir
    file_count = $packageMetadata.Count
    files = $packageMetadata
  }
}

$outputDir = Split-Path $resolvedOutputPath -Parent
if (-not (Test-Path $outputDir)) {
  New-Item -ItemType Directory -Path $outputDir | Out-Null
}

$manifest | ConvertTo-Json -Depth 6 | Set-Content $resolvedOutputPath -Encoding UTF8

Write-Host "Release manifest written to: $resolvedOutputPath"
