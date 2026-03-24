$ErrorActionPreference = "Stop"

function Test-CommandAvailable([string]$Name) {
  return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Resolve-CommandPath([string]$Name) {
  $command = Get-Command $Name -ErrorAction SilentlyContinue
  if ($command) {
    return $command.Source
  }
  return $null
}

function Resolve-FirstExistingPath([string[]]$Candidates) {
  foreach ($candidate in $Candidates) {
    if ($candidate -and (Test-Path $candidate)) {
      return $candidate
    }
  }
  return $null
}

$qmakePath = Resolve-CommandPath "qmake"

$qtRoot = $env:QT_ROOT
if (-not $qtRoot -and $qmakePath) {
  $qtRoot = Split-Path -Parent $qmakePath | Split-Path -Parent
}

$qtRoot = if ($qtRoot) { [System.IO.Path]::GetFullPath($qtRoot) } else { $null }
$qtBaseRoot = if ($qtRoot) { Split-Path -Parent (Split-Path -Parent $qtRoot) } else { $null }
$qtToolsRoot = if ($qtBaseRoot) { Join-Path $qtBaseRoot "Tools" } else { $null }
$preferredMingwBin = if ($qtToolsRoot) { Join-Path $qtToolsRoot "mingw1310_64\bin" } else { $null }
$preferredNinja = if ($qtToolsRoot) { Join-Path $qtToolsRoot "Ninja\ninja.exe" } else { $null }

$cmakePath = Resolve-CommandPath "cmake"
$ctestPath = Resolve-CommandPath "ctest"
$ninjaPath = Resolve-FirstExistingPath @(
  (Resolve-CommandPath "ninja"),
  $preferredNinja
)
$clPath = Resolve-CommandPath "cl"
$gccPath = Resolve-FirstExistingPath @(
  $(if ($preferredMingwBin) { Join-Path $preferredMingwBin "gcc.exe" }),
  (Resolve-CommandPath "gcc")
)
$gxxPath = Resolve-FirstExistingPath @(
  $(if ($preferredMingwBin) { Join-Path $preferredMingwBin "g++.exe" }),
  (Resolve-CommandPath "g++")
)
$windeployqtPath = Resolve-FirstExistingPath @(
  $(if ($qtRoot) { Join-Path $qtRoot "bin\windeployqt.exe" }),
  (Resolve-CommandPath "windeployqt")
)
$isccPath = Resolve-CommandPath "iscc"

$msvcReady = [bool]$clPath
$mingwReady = [bool]$gxxPath -and [bool]$ninjaPath
$qtReady = [bool]$qmakePath -or [bool]$qtRoot -or [bool]$env:CMAKE_PREFIX_PATH

$results = [ordered]@{
  cmake = [bool]$cmakePath
  ctest = [bool]$ctestPath
  ninja = [bool]$ninjaPath
  cl = [bool]$clPath
  gcc = [bool]$gccPath
  "g++" = [bool]$gxxPath
  qmake = [bool]$qmakePath
  windeployqt = [bool]$windeployqtPath
  iscc = [bool]$isccPath
  qt_root_env = [bool]$env:QT_ROOT
  cmake_prefix_env = [bool]$env:CMAKE_PREFIX_PATH
  qt_ready = $qtReady
  msvc_ready = $msvcReady
  mingw_ready = $mingwReady
}

$results.GetEnumerator() | ForEach-Object {
  "{0,-18} {1}" -f $_.Key, $_.Value
}

Write-Host ""
Write-Host "Detected paths:"
foreach ($entry in @(
  @{ Name = "cmake"; Value = $cmakePath },
  @{ Name = "ctest"; Value = $ctestPath },
  @{ Name = "ninja"; Value = $ninjaPath },
  @{ Name = "cl"; Value = $clPath },
  @{ Name = "gcc"; Value = $gccPath },
  @{ Name = "g++"; Value = $gxxPath },
  @{ Name = "qmake"; Value = $qmakePath },
  @{ Name = "windeployqt"; Value = $windeployqtPath },
  @{ Name = "iscc"; Value = $isccPath },
  @{ Name = "qt_root"; Value = $qtRoot }
)) {
  "{0,-18} {1}" -f $entry.Name, ($(if ($entry.Value) { $entry.Value } else { "<missing>" }))
}

Write-Host ""
Write-Host "Build routes:"
Write-Host ("{0,-18} {1}" -f "MinGW/Qt", ($qtReady -and $mingwReady -and [bool]$cmakePath))
Write-Host ("{0,-18} {1}" -f "MSVC/Qt", ($qtReady -and $msvcReady -and [bool]$cmakePath))

if (-not $results.cmake) {
  Write-Host ""
  Write-Host "Missing: cmake"
}

if (-not $qtReady) {
  Write-Host ""
  Write-Host "Missing: Qt 6 environment (qmake or QT_ROOT/CMAKE_PREFIX_PATH)"
}

if (-not $msvcReady -and -not $mingwReady) {
  Write-Host ""
  Write-Host "Missing: compiler environment (MSVC developer shell or MinGW g++)"
}
