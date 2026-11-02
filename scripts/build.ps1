param(
    [string]$QtRoot = $(if ($env:QT_ROOT) { $env:QT_ROOT } else { 'D:\Qt\6.11.0\mingw_64' }),
    [string]$Configuration = 'Debug'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot
$toolsRoot = Join-Path (Split-Path (Split-Path $QtRoot)) 'Tools'
$cmake = Join-Path $toolsRoot 'CMake_64\bin\cmake.exe'
$ninja = Join-Path $toolsRoot 'Ninja\ninja.exe'
$compiler = Join-Path $toolsRoot 'mingw1310_64\bin\g++.exe'
$env:PATH = (Split-Path $compiler) + ';' + (Join-Path $QtRoot 'bin') + ';' + $env:PATH
$buildPath = Join-Path $projectRoot 'build\Sidekick'
& $cmake -S $projectRoot -B $buildPath -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration" "-DCMAKE_PREFIX_PATH=$QtRoot" "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_CXX_COMPILER=$compiler"
if ($LASTEXITCODE -ne 0) { throw 'CMake 配置失败。' }
& $cmake --build $buildPath --parallel 4
if ($LASTEXITCODE -ne 0) { throw '编译失败。' }
