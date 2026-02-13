$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $repoRoot

$venvPython = Join-Path $repoRoot ".venv\Scripts\python.exe"
if (-not (Test-Path $venvPython)) {
    Write-Host "[dev] 未检测到 .venv，正在创建..."
    python -m venv .venv
}

Write-Host "[dev] 启动应用: src/main.py"
& $venvPython "src/main.py"
