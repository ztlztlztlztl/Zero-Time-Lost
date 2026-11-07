param(
    [string]$QtRoot = $(if ($env:QT_ROOT) { $env:QT_ROOT } else { 'D:\Qt\6.11.0\mingw_64' }),
    [string]$Destination = '',
    [ValidateSet('Desktop','Manager')][string]$Component = 'Desktop',
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [string]$BuildDirectory = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$toolchain = Get-ZtlToolchain $QtRoot
$cmake = $toolchain.CMake
$env:PATH = $toolchain.BinPath + ';' + $env:PATH
$componentDir = if ($Component -eq 'Manager') { 'ServerManager' } else { 'Desktop' }
$buildPath = if ($BuildDirectory) { [IO.Path]::GetFullPath($BuildDirectory) } else { Join-Path $projectRoot "build\$componentDir\$Configuration" }
$destination = if ($Destination) { [IO.Path]::GetFullPath($Destination) } else { Join-Path $projectRoot $(if ($Component -eq 'Manager') { 'dist\ZTL-ServerManager' } else { 'dist\ZTL' }) }
$stage = Join-Path $projectRoot ('build\package-stage-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
& $cmake --install $buildPath --prefix $stage
if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed; existing installation was not changed.' }
# Construct the runtime from a clean directory, never inherit dependencies from an old package.
$stageRuntime = [IO.Path]::GetFullPath((Join-Path $stage 'runtime'))
if (!$stageRuntime.StartsWith($stage + '\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid staging runtime.' }
if (Test-Path -LiteralPath $stageRuntime) { Remove-Item -LiteralPath $stageRuntime -Recurse -Force }
& (Join-Path $PSScriptRoot 'prepare_runtime.ps1') -Destination $stageRuntime
if ($Component -eq 'Manager') { & (Join-Path $projectRoot 'deploy\scripts\prepare_runtime.ps1') -Destination $stageRuntime }
$product = if ($Component -eq 'Manager') { 'Z.T.L.ServerManager' } else { 'Z.T.L' }
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'uninstall.ps1') -Destination (Join-Path $stage 'uninstall.ps1') -Force
& (Join-Path $PSScriptRoot 'install-package.ps1') -Stage $stage -Destination $destination -Product $product
if (!$stage.StartsWith([IO.Path]::GetFullPath((Join-Path $projectRoot 'build')) + '\',[StringComparison]::OrdinalIgnoreCase) -or (Get-Item -LiteralPath $stage).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Invalid completed package stage.' }
Remove-Item -LiteralPath $stage -Recurse -Force
Write-Output (Join-Path $destination ($product + '.exe'))
