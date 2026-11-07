$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Split-Path (Split-Path $PSScriptRoot)))
$testRoot = Join-Path $projectRoot ('build\uninstall-tests-' + [Guid]::NewGuid().ToString('N'))
if (![IO.Path]::GetFullPath($testRoot).StartsWith((Join-Path $projectRoot 'build') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid test directory' }
function New-Fixture([string]$name, [string[]]$entries) {
    $fixture = Join-Path $testRoot $name
    New-Item -ItemType Directory -Path (Join-Path $fixture 'data'),(Join-Path $fixture 'runtime') -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectRoot 'scripts\uninstall.ps1') -Destination (Join-Path $fixture 'uninstall.ps1')
    Set-Content -LiteralPath (Join-Path $fixture 'Z.T.L.exe') -Value 'fixture'
    Set-Content -LiteralPath (Join-Path $fixture 'data\keep.txt') -Value 'personal-data-fixture'
    Set-Content -LiteralPath (Join-Path $fixture 'unlisted.txt') -Value 'user-file-fixture'
    @{ product = 'Z.T.L'; entries = $entries } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $fixture '.ztl-install.json') -Encoding utf8
    return $fixture
}
$entries = @('Z.T.L.exe','runtime','uninstall.ps1','.ztl-install.json')
$keep = New-Fixture 'keep' $entries
& (Join-Path $keep 'uninstall.ps1') -Data Keep | Out-Null
if (!(Test-Path -LiteralPath (Join-Path $keep 'data\keep.txt')) -or (Test-Path -LiteralPath (Join-Path $keep 'Z.T.L.exe')) -or !(Test-Path -LiteralPath (Join-Path $keep 'unlisted.txt'))) { throw 'Keep data failed' }
$delete = New-Fixture 'delete' $entries
& (Join-Path $delete 'uninstall.ps1') -Data Delete | Out-Null
if ((Test-Path -LiteralPath (Join-Path $delete 'data')) -or !(Test-Path -LiteralPath (Join-Path $delete 'unlisted.txt'))) { throw 'Delete data failed' }
$invalid = New-Fixture 'invalid' @('Z.T.L.exe','..')
$rejected = $false
try { & (Join-Path $invalid 'uninstall.ps1') -Data Delete | Out-Null } catch { $rejected = $true }
if (!$rejected -or !(Test-Path -LiteralPath (Join-Path $invalid 'Z.T.L.exe')) -or !(Test-Path -LiteralPath (Join-Path $invalid 'data\keep.txt'))) { throw 'Invalid manifest changed files' }
Write-Output 'Uninstall tests passed: keep data, delete data, preserve unlisted files, reject traversal before deletion.'
