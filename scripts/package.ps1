param(
    [string]$QtRoot = $(if ($env:QT_ROOT) { $env:QT_ROOT } else { 'D:\Qt\6.11.0\mingw_64' }),
    [string]$Destination = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot
$toolsRoot = Join-Path (Split-Path (Split-Path $QtRoot)) 'Tools'
$cmake = Join-Path $toolsRoot 'CMake_64\bin\cmake.exe'
$env:PATH = (Join-Path $toolsRoot 'mingw1310_64\bin') + ';' + (Join-Path $QtRoot 'bin') + ';' + $env:PATH
$buildPath = Join-Path $projectRoot 'build\Sidekick'
$destination = if ($Destination) { [IO.Path]::GetFullPath($Destination) } else { Join-Path $projectRoot 'build\ZTL-package' }
# Python is an application dependency. Deployment must include it without user configuration.
& (Join-Path $PSScriptRoot 'prepare_runtime.ps1') -Destination (Join-Path $buildPath 'runtime')
& $cmake --install $buildPath --prefix $destination
if ($LASTEXITCODE -ne 0) { throw '部署 Qt 依赖失败。' }
# Record only deployed top-level entries, so uninstall preserves unrelated user files.
$entries = @(Get-ChildItem -LiteralPath $destination -Force | Where-Object { $_.Name -ne 'data' } | ForEach-Object Name)
$entries += '.ztl-install.json'
@{ product = 'Z.T.L'; entries = @($entries | Select-Object -Unique) } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $destination '.ztl-install.json') -Encoding utf8
Write-Output (Join-Path $destination 'Z.T.L.exe')
