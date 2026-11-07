param(
    [string]$QtRoot = $(if ($env:QT_ROOT) { $env:QT_ROOT } else { 'D:\Qt\6.11.0\mingw_64' }),
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [ValidateSet('Desktop','Manager')][string]$Component = 'Desktop'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$toolchain = Get-ZtlToolchain $QtRoot
$cmake = $toolchain.CMake
$ninja = $toolchain.Ninja
$compiler = $toolchain.Compiler
$env:PATH = $toolchain.BinPath + ';' + $env:PATH
$sourcePath = if ($Component -eq 'Manager') { Join-Path $projectRoot deploy } else { Join-Path $projectRoot desktop }
$buildPath = Join-Path $projectRoot $(if ($Component -eq 'Manager') { "build\ServerManager\$Configuration" } else { "build\Desktop\$Configuration" })
& $cmake -S $sourcePath -B $buildPath -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration" "-DCMAKE_PREFIX_PATH=$QtRoot" "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_CXX_COMPILER=$compiler"
if ($LASTEXITCODE -ne 0) { throw 'CMake 配置失败。' }
& $cmake --build $buildPath --parallel 4
if ($LASTEXITCODE -ne 0) { throw '编译失败。' }
