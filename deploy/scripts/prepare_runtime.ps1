param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path (Split-Path $PSScriptRoot)
$interpreter = Join-Path $Destination 'python.exe'
$requirements = Join-Path $PSScriptRoot 'bootstrap-requirements.txt'
$digest = (Get-FileHash -LiteralPath $requirements -Algorithm SHA256).Hash
$marker = Join-Path $Destination '.ztl-bootstrap-deps'
$pth = Join-Path $Destination 'python313._pth'
$entries = [IO.File]::ReadAllText($pth)
if (-not ($entries -match '(?m)^Lib/site-packages\s*$')) { [IO.File]::AppendAllText($pth, "`nLib/site-packages`n") }
if ((Test-Path -LiteralPath $marker) -and ([IO.File]::ReadAllText($marker).Trim() -eq $digest)) {
    & $interpreter -B -c 'import sys,paramiko,httpx,bcrypt,nacl,cryptography; assert paramiko.__version__ == sys.argv[1]' '5.1.0'
    if ($LASTEXITCODE -eq 0) { Write-Output 'Bundled deployment dependencies ready'; return }
}
$download = Join-Path $projectRoot 'build\downloads'
$temporary = Join-Path $projectRoot 'build\runtime-tmp'
New-Item -ItemType Directory -Force -Path $download,$temporary | Out-Null
$pipWheel = Join-Path $download 'pip-25.3-py3-none-any.whl'
if (-not (Test-Path -LiteralPath $pipWheel)) {
    $metadata = Invoke-RestMethod -Uri 'https://pypi.org/pypi/pip/25.3/json'
    $wheel = $metadata.urls | Where-Object filename -eq 'pip-25.3-py3-none-any.whl' | Select-Object -First 1
    if (-not $wheel) { throw 'Dependency bootstrap wheel not found.' }
    Invoke-WebRequest -Uri $wheel.url -OutFile $pipWheel
    if ((Get-FileHash -LiteralPath $pipWheel -Algorithm SHA256).Hash.ToLowerInvariant() -ne $wheel.digests.sha256) { throw 'Dependency bootstrap checksum failed.' }
}
$beforeTemp=$env:TEMP; $beforeTmp=$env:TMP; $beforeUtf8=$env:PYTHONUTF8
try {
    $env:TEMP=$temporary; $env:TMP=$temporary; $env:PYTHONUTF8='1'
    $wheelCache = & $interpreter -I -B (Join-Path $projectRoot 'scripts\prepare_wheels.py') --requirements $requirements --cache-root (Join-Path $download 'bootstrap-wheels') --pip-wheel $pipWheel
    if ($LASTEXITCODE -ne 0) { throw 'Dependency cache preparation failed; no package was installed.' }
    $wheelCache = ([string]$wheelCache).Trim()
    & $interpreter -B -c 'import sys,runpy; sys.path.insert(0,sys.argv.pop(1)); runpy.run_module(sys.argv.pop(1),run_name=sys.argv.pop(1))' $pipWheel pip __main__ install --disable-pip-version-check --no-cache-dir --no-index --find-links $wheelCache --no-deps --only-binary=:all: --no-compile --no-warn-script-location --upgrade --target (Join-Path $Destination 'Lib\site-packages') -r $requirements
    if ($LASTEXITCODE -ne 0) { throw 'Bundled SSH dependencies installation failed.' }
    & $interpreter -B -c 'import sys,paramiko,httpx,bcrypt,nacl,cryptography; assert paramiko.__version__ == sys.argv[1]' '5.1.0'
    if ($LASTEXITCODE -ne 0) { throw 'Bundled SSH dependencies self-check failed.' }
    [IO.File]::WriteAllText($marker, $digest)
} finally { $env:TEMP=$beforeTemp; $env:TMP=$beforeTmp; $env:PYTHONUTF8=$beforeUtf8 }
