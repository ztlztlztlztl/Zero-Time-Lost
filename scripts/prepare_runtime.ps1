param(
    [string]$Destination = '',
    [string]$Archive = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot
$version = '3.13.9'
$expectedHash = '91D828C2DA3A029B41699E918674A0CB379C02CF20DAB9C501306885F837402A'
if (-not $Destination) { $Destination = Join-Path $projectRoot 'build\Sidekick\runtime' }
if (-not $Archive) { $Archive = Join-Path $projectRoot "build\downloads\python-$version-embed-amd64.zip" }
$interpreter = Join-Path $Destination 'python.exe'
$marker = Join-Path $Destination '.ztl-runtime-sha256'
if ((Test-Path -LiteralPath $marker) -and (Test-Path -LiteralPath $interpreter) -and
    ([IO.File]::ReadAllText($marker).Trim() -eq $expectedHash)) {
    & $interpreter -I -c 'import sys,ssl,imaplib,email,json; assert sys.version_info[:3] == (3,13,9)'
    if ($LASTEXITCODE -eq 0) { Write-Output 'Bundled mail runtime ready'; return }
}
New-Item -ItemType Directory -Force -Path (Split-Path $Archive) | Out-Null
if (-not (Test-Path -LiteralPath $Archive)) {
    Invoke-WebRequest -Uri "https://www.python.org/ftp/python/$version/python-$version-embed-amd64.zip" -OutFile $Archive
}
if ((Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash -ne $expectedHash) {
    throw 'Python 运行时校验失败，请重新获取官方文件。'
}
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
Expand-Archive -LiteralPath $Archive -DestinationPath $Destination -Force
$interpreter = Join-Path $Destination 'python.exe'
& $interpreter -I -c 'import ssl,imaplib,email,json; print("Bundled mail runtime ready")'
if ($LASTEXITCODE -ne 0) { throw '内置邮件运行时自检失败。' }
[IO.File]::WriteAllText($marker, $expectedHash)
Write-Output $Destination
