param(
    [switch]$RecreateEnv,
    [switch]$SkipTests,
    [switch]$SkipSmoke,
    [int]$SmokeSeconds = 8
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $repoRoot

$buildEnv = Join-Path $repoRoot "build_env"
$buildPython = Join-Path $buildEnv "Scripts\python.exe"

if ($RecreateEnv -and (Test-Path $buildEnv)) {
    Write-Host "[build] 重建 build_env..."
    Remove-Item $buildEnv -Recurse -Force
}

if (-not (Test-Path $buildPython)) {
    Write-Host "[build] 创建 build_env..."
    python -m venv build_env
}

Write-Host "[build] 安装构建依赖..."
& $buildPython -m pip install -r requirements-build.txt
& $buildPython -m pip install "pyinstaller>=6.0.0"

if (-not $SkipTests) {
    Write-Host "[build] 运行测试..."
    & $buildPython -m pytest tests/ -v --tb=short
}

Write-Host "[build] 开始 PyInstaller 打包..."
& $buildPython -m PyInstaller --clean --noconfirm build.spec

$exePath = Join-Path $repoRoot "dist\CyberCompanion\CyberCompanion-core.exe"
if (-not (Test-Path $exePath)) {
    throw "未找到打包产物: $exePath"
}

if (-not $SkipSmoke) {
    $seconds = [Math]::Max(3, $SmokeSeconds)
    Write-Host "[build] 冒烟启动测试 (${seconds}s)..."
    $proc = Start-Process -FilePath $exePath -PassThru
    Start-Sleep -Seconds $seconds
    if (-not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force
    }
}

Write-Host ""
Write-Host "[build] ✅ 完成，产物路径:"
Write-Host $exePath
