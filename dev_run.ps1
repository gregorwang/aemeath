$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $repoRoot

$venvPython = Join-Path $repoRoot ".venv\Scripts\python.exe"
if (-not (Test-Path $venvPython)) {
    Write-Host "[dev] 未检测到 .venv，正在创建..."
    python -m venv .venv
}

if (-not (Test-Path $venvPython)) {
    throw "[dev] .venv 创建失败，未找到 $venvPython"
}

$env:AEMEATH_DEV_FAST_IDLE = "1"
Write-Host "[dev] 已启用开发快节奏空闲配置 (AEMEATH_DEV_FAST_IDLE=1)"

function Start-AppProcess {
    Write-Host "[dev] 启动应用: src/main.py"
    Start-Process `
        -FilePath $venvPython `
        -ArgumentList "src/main.py" `
        -WorkingDirectory $repoRoot `
        -NoNewWindow `
        -PassThru
}

$watchRoots = @("src", "config", "characters")
$watchExtensions = @(
    ".py", ".json", ".yaml", ".yml", ".toml", ".ini",
    ".png", ".jpg", ".jpeg", ".gif", ".webp"
)

$state = [hashtable]::Synchronized(@{
    RestartRequested = $false
    LastEventAt = [datetime]::MinValue
    LastPath = ""
})

$watchers = @()
$subscriptions = @()

$onFileChanged = {
    $path = $Event.SourceEventArgs.FullPath
    if (-not $path) { return }
    if ($path -match "\\__pycache__\\") { return }
    if ($path.EndsWith(".pyc")) { return }

    $ext = [System.IO.Path]::GetExtension($path).ToLowerInvariant()
    if ($watchExtensions -notcontains $ext) { return }

    $now = Get-Date
    $msSinceLast = ($now - $state.LastEventAt).TotalMilliseconds
    if (($state.LastPath -eq $path) -and ($msSinceLast -lt 600)) {
        return
    }

    $state.LastPath = $path
    $state.LastEventAt = $now
    $state.RestartRequested = $true
    Write-Host "[dev] 检测到变更: $path"
}

try {
    foreach ($root in $watchRoots) {
        $fullRoot = Join-Path $repoRoot $root
        if (-not (Test-Path $fullRoot)) {
            continue
        }

        $watcher = New-Object System.IO.FileSystemWatcher
        $watcher.Path = $fullRoot
        $watcher.Filter = "*.*"
        $watcher.IncludeSubdirectories = $true
        $watcher.NotifyFilter = [System.IO.NotifyFilters]::FileName `
            -bor [System.IO.NotifyFilters]::LastWrite `
            -bor [System.IO.NotifyFilters]::CreationTime `
            -bor [System.IO.NotifyFilters]::DirectoryName `
            -bor [System.IO.NotifyFilters]::Size
        $watcher.EnableRaisingEvents = $true
        $watchers += $watcher

        $subscriptions += Register-ObjectEvent -InputObject $watcher -EventName Changed -Action $onFileChanged
        $subscriptions += Register-ObjectEvent -InputObject $watcher -EventName Created -Action $onFileChanged
        $subscriptions += Register-ObjectEvent -InputObject $watcher -EventName Deleted -Action $onFileChanged
        $subscriptions += Register-ObjectEvent -InputObject $watcher -EventName Renamed -Action $onFileChanged
    }

    Write-Host "[dev] 文件监听已开启: $($watchRoots -join ', ')"
    Write-Host "[dev] 修改后将自动重启。按 Ctrl+C 退出。"

    $appProcess = Start-AppProcess

    while ($true) {
        Start-Sleep -Milliseconds 250

        if ($state.RestartRequested) {
            $state.RestartRequested = $false
            Write-Host "[dev] 正在重启应用..."
            if ($appProcess -and -not $appProcess.HasExited) {
                Stop-Process -Id $appProcess.Id -Force
                $appProcess.WaitForExit()
            }
            $appProcess = Start-AppProcess
            continue
        }

        if ($appProcess -and $appProcess.HasExited) {
            Write-Host "[dev] 应用已退出 (code=$($appProcess.ExitCode))，等待变更后自动重启..."
            while (-not $state.RestartRequested) {
                Start-Sleep -Milliseconds 250
            }
            $state.RestartRequested = $false
            Write-Host "[dev] 检测到新变更，重新启动应用..."
            $appProcess = Start-AppProcess
        }
    }
}
finally {
    foreach ($subscription in $subscriptions) {
        if ($subscription) {
            Unregister-Event -SubscriptionId $subscription.Id -ErrorAction SilentlyContinue
        }
    }

    foreach ($watcher in $watchers) {
        if ($watcher) {
            $watcher.EnableRaisingEvents = $false
            $watcher.Dispose()
        }
    }

    if ($appProcess -and -not $appProcess.HasExited) {
        Stop-Process -Id $appProcess.Id -Force -ErrorAction SilentlyContinue
    }
}
