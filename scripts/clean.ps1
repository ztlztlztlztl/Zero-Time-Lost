[CmdletBinding(SupportsShouldProcess = $true)]
param()

$ErrorActionPreference = 'Stop'
$cleanupRoot = (Resolve-Path -LiteralPath (Split-Path $PSScriptRoot)).Path.TrimEnd('\')
if (-not (Test-Path -LiteralPath (Join-Path $cleanupRoot 'CMakeLists.txt')) -or
    -not (Test-Path -LiteralPath (Join-Path $cleanupRoot 'main.cpp')) -or
    -not (Test-Path -LiteralPath (Join-Path $cleanupRoot '.git'))) {
    throw 'Run this script from the original Sidekick project checkout.'
}

# Keep source tests, fixtures, build scripts, instruct.md and repository configuration.
# These are generated outputs, local IDE state and superseded planning/report files.
$cleanupRelative = @(
    'build'
    '.qtcreator'
    'scripts\__pycache__'
    'tests\__pycache__'
    'architecture.md'
    'methodology.md'
    'report.md'
    'repuire.md'
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
    $entry = Get-Item -LiteralPath $resolved -Force
    if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw "Refusing to remove a link: $resolved"
    }
    if ($entry.PSIsContainer) {
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
