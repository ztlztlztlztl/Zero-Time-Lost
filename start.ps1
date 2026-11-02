param(
    [string]$QtRoot = $(if ($env:QT_ROOT) { $env:QT_ROOT } else { 'D:\Qt\6.11.0\mingw_64' }),
    [string]$BuildDirectory = 'build\Sidekick'
)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$binary = Join-Path $projectRoot (Join-Path $BuildDirectory 'Z.T.L.exe')
if (-not (Test-Path -LiteralPath $binary)) { throw '请先运行 scripts/build.ps1。' }
$qtBin = Join-Path $QtRoot 'bin'
if (Test-Path -LiteralPath $qtBin) { $env:PATH = $qtBin + ';' + $env:PATH }
$mingw = Join-Path (Split-Path (Split-Path $QtRoot)) 'Tools\mingw1310_64\bin'
if (Test-Path -LiteralPath $mingw) { $env:PATH = $mingw + ';' + $env:PATH }
& $binary
