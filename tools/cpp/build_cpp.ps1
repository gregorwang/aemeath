$ErrorActionPreference = "Stop"

param(
  [string]$Preset = "windows-ninja-release",
  [switch]$RunTests
)

function Resolve-CMake {
  $cmake = Get-Command cmake -ErrorAction SilentlyContinue
  if ($cmake) { return $cmake.Source }
  throw "cmake not found in PATH. Install CMake or launch from a developer shell."
}

if (-not $env:QT_ROOT -and -not $env:CMAKE_PREFIX_PATH) {
  throw "Set QT_ROOT or CMAKE_PREFIX_PATH before building."
}

$cmake = Resolve-CMake

& $cmake --preset $Preset
& $cmake --build --preset ("build-" + $Preset)

if ($RunTests) {
  & ctest --preset ("test-" + $Preset)
}
