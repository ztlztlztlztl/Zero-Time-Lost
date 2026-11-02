param(
    [string]$QtRoot = $(if ($env:QT_ROOT) { $env:QT_ROOT } else { 'D:\Qt\6.11.0\mingw_64' })
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot
$toolsRoot = Join-Path (Split-Path (Split-Path $QtRoot)) 'Tools'
$cmake = Join-Path $toolsRoot 'CMake_64\bin\cmake.exe'
$env:PATH = (Join-Path $toolsRoot 'mingw1310_64\bin') + ';' + (Join-Path $QtRoot 'bin') + ';' + $env:PATH
$buildPath = Join-Path $projectRoot 'build\Sidekick'
$destination = Join-Path $projectRoot 'build\Sidekick-package'
# Python is an application dependency. Deployment must include it without user configuration.
& (Join-Path $PSScriptRoot 'prepare_runtime.ps1') -Destination (Join-Path $buildPath 'runtime')
& $cmake --install $buildPath --prefix $destination
if ($LASTEXITCODE -ne 0) { throw '部署 Qt 依赖失败。' }
Write-Output (Join-Path $destination 'bin\Zero-Time-Lost-tobedone.exe')
