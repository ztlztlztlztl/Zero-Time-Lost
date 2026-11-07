param([string]$QtRoot=$(if ($env:QT_ROOT) { $env:QT_ROOT } else { 'D:\Qt\6.11.0\mingw_64' }), [ValidateSet('Debug','Release')][string]$Configuration='Release')
& (Join-Path (Split-Path $PSScriptRoot) 'scripts\build.ps1') -Component Manager @PSBoundParameters
