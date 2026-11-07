param([string]$Stage,[string]$Destination,[string]$Product)
$ErrorActionPreference = 'Stop'
$stageRoot = [IO.Path]::GetFullPath($Stage).TrimEnd('\')
$targetRoot = [IO.Path]::GetFullPath($Destination).TrimEnd('\')
if ($stageRoot -eq $targetRoot -or $stageRoot.StartsWith($targetRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or $targetRoot.StartsWith($stageRoot+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Stage and installation must not overlap.' }
function Resolve-InstallFile([string]$Relative) {
    if ([IO.Path]::IsPathRooted($Relative) -or $Relative -match '(^|[/\\])\.\.?([/\\]|$)' -or $Relative -match '^data([/\\]|$)' -or [string]::IsNullOrWhiteSpace($Relative)) { throw 'Invalid package file path.' }
    $path = [IO.Path]::GetFullPath((Join-Path $targetRoot $Relative))
    if (!$path.StartsWith($targetRoot + '\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Package path escaped installation.' }
    $part = $path
    while ($part -and $part -ne $targetRoot) {
        if ((Test-Path -LiteralPath $part) -and ((Get-Item -LiteralPath $part -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Installation contains a link: $part" }
        $part = Split-Path $part
    }
    return $path
}
foreach ($rootPath in @($stageRoot,$targetRoot)) {
    $part=$rootPath
    while($part) {
        if((Test-Path -LiteralPath $part) -and ((Get-Item -LiteralPath $part -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Installation ancestry contains a link: $part"}
        $parent=Split-Path $part
        if($parent -eq $part){break}
        $part=$parent
    }
}
$exe = Join-Path $targetRoot ($Product + '.exe')
if (Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe }) { throw '请先退出目标目录中的 Z.T.L 程序。' }
$manifestPath = Join-Path $targetRoot '.ztl-install.json'
$journalPath = Join-Path $targetRoot '.ztl-update.json'
function Restore-PackageUpdate($Transaction) {
    $savedRoot=[IO.Path]::GetFullPath($Transaction.rollback)
    if(!(Test-Path -LiteralPath $savedRoot -PathType Container) -or ((Get-Item -LiteralPath $savedRoot).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Update recovery snapshot is unavailable.'}
    foreach($name in $Transaction.touched){
        $target=Resolve-InstallFile $name
        $saved=[IO.Path]::GetFullPath((Join-Path $savedRoot $name))
        if(!$saved.StartsWith($savedRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Invalid recovery path.'}
        if(Test-Path -LiteralPath $saved -PathType Leaf){New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null;Copy-Item -LiteralPath $saved -Destination $target -Force}
        elseif(Test-Path -LiteralPath $target -PathType Leaf){Remove-Item -LiteralPath $target -Force}
    }
    Remove-Item -LiteralPath $journalPath -Force
}
if(Test-Path -LiteralPath $journalPath){Restore-PackageUpdate (Get-Content -LiteralPath $journalPath -Raw | ConvertFrom-Json)}
$old = @()
if (Test-Path -LiteralPath $manifestPath) {
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.product -ne $Product) { throw 'Installation product mismatch.' }
    if ($manifest.format -eq 2) { $old = @($manifest.entries) }
    else { $old = @($manifest.entries | Where-Object { $_ -notmatch '[/\\]' -and $_ -ne 'data' -and (Test-Path -LiteralPath (Join-Path $targetRoot $_) -PathType Leaf) }) }
}
$files = @(Get-ChildItem -LiteralPath $stageRoot -Recurse -File -Force | ForEach-Object { $_.FullName.Substring($stageRoot.Length + 1) } | Where-Object { $_ -notmatch '^\.rollback([/\\]|$)' })
if (@(Get-ChildItem -LiteralPath $stageRoot -Recurse -Force -Attributes ReparsePoint).Count) { throw 'Stage contains links.' }
foreach ($name in @($files) + @($old)) {
    $target = Resolve-InstallFile $name
    if ($name -in $files -and (Test-Path -LiteralPath $target) -and $name -notin $old) { throw "Unknown file conflicts with package; move it first: $target" }
}
New-Item -ItemType Directory -Path $targetRoot -Force | Out-Null
$rollback = Join-Path $stageRoot '.rollback'
New-Item -ItemType Directory -Path $rollback | Out-Null
$touched = @($files + $old + '.ztl-install.json' | Select-Object -Unique)
foreach ($name in $touched) {
    $target = Resolve-InstallFile $name
    if (Test-Path -LiteralPath $target -PathType Leaf) {
        $saved = Join-Path $rollback $name
        New-Item -ItemType Directory -Path (Split-Path $saved) -Force | Out-Null
        Copy-Item -LiteralPath $target -Destination $saved
    }
}
$transaction=@{rollback=$rollback;touched=$touched}
$journalTemp=$journalPath+'.tmp'
$transaction | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $journalTemp -Encoding utf8
Move-Item -LiteralPath $journalTemp -Destination $journalPath -Force
try {
    foreach ($name in $files) {
        $target = Resolve-InstallFile $name
        New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $stageRoot $name) -Destination $target -Force
    }
    foreach ($name in $old) {
        $target = Resolve-InstallFile $name
        if ($name -notin $files -and (Test-Path -LiteralPath $target -PathType Leaf)) { Remove-Item -LiteralPath $target -Force }
    }
    @{format=2;product=$Product;entries=@($files + '.ztl-install.json' | Select-Object -Unique)} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifestPath -Encoding utf8
    Remove-Item -LiteralPath $journalPath -Force
} catch {
    Restore-PackageUpdate $transaction
    throw
}
