[CmdletBinding(SupportsShouldProcess = $true)]
param()

$ErrorActionPreference = 'Stop'
$cleanupRoot = (Resolve-Path -LiteralPath (Split-Path $PSScriptRoot)).Path.TrimEnd('\')
if (-not (Test-Path -LiteralPath (Join-Path $cleanupRoot 'CMakeLists.txt')) -or
    -not (Test-Path -LiteralPath (Join-Path $cleanupRoot 'desktop\main.cpp')) -or
    -not (Test-Path -LiteralPath (Join-Path $cleanupRoot '.git'))) {
    throw 'Run this script from the original Sidekick project checkout.'
}

# Keep source tests, fixtures, build scripts, instruct.md and repository configuration.
# Only explicitly listed intermediate outputs and local IDE state can be removed.
$cleanupRelative = @(
    'build\Desktop\CMakeFiles'
    'build\Desktop\Debug\CMakeFiles'
    'build\Desktop\Release\CMakeFiles'
    'build\ServerManager\CMakeFiles'
    'build\ServerManager\Debug\CMakeFiles'
    'build\ServerManager\Release\CMakeFiles'
    '.qtcreator'
    'scripts\__pycache__'
    'tests\__pycache__'
)

# Validate the complete list before deleting anything. Never follow junctions or links.
$cleanupTargets = @()
foreach ($relativePath in $cleanupRelative) {
    $candidate = Join-Path $cleanupRoot $relativePath
    if (-not (Test-Path -LiteralPath $candidate)) { continue }
    $resolved = (Resolve-Path -LiteralPath $candidate).Path
    if (-not $resolved.StartsWith($cleanupRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Target is outside the project: $resolved"
    }
    $ancestor = $candidate
    while ($ancestor -and $ancestor -ne $cleanupRoot) {
        if ((Test-Path -LiteralPath $ancestor) -and ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Refusing linked ancestry: $ancestor" }
        $ancestor = Split-Path $ancestor
    }
    $entry = Get-Item -LiteralPath $resolved -Force
    if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw "Refusing to remove a link: $resolved"
    }
    if ($entry.PSIsContainer) {
        $protected = @(Get-ChildItem -LiteralPath $resolved -Recurse -Force | Where-Object { $_.Name -eq 'data' -or $_.Name -like '*install*manifest*' -or $_.Name -eq '.ztl-install.json' -or $_.Extension -eq '.exe' -and $_.Name -eq 'Z.T.L.exe' })
        if ($protected.Count -gt 0) { throw "Target contains an installation or data; refusing to clean: $resolved" }
        $links = @(Get-ChildItem -LiteralPath $resolved -Recurse -Force -Attributes ReparsePoint)
        if ($links.Count -gt 0) { throw "Target contains links: $resolved" }
    }
    $cleanupTargets += $resolved
}

foreach ($target in $cleanupTargets) {
    if ($PSCmdlet.ShouldProcess($target, 'Permanently remove project output or obsolete document')) {
        Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction Stop
        Write-Output "Removed: $target"
    }
}
Write-Output 'Source code, source tests, README, instruct.md and Git/Codex/GitHub configuration are retained.'
