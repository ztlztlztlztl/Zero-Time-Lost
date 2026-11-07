[CmdletBinding(SupportsShouldProcess)]
param([ValidateSet('Ask','Keep','Delete')][string]$Data = 'Ask')
$ErrorActionPreference = 'Stop'
$installRoot = [IO.Path]::GetFullPath($PSScriptRoot).TrimEnd('\')
$manifestPath = Join-Path $installRoot '.ztl-install.json'
if (!(Test-Path -LiteralPath $manifestPath)) {
    throw '请从完整的 Z.T.L 安装目录运行卸载脚本。'
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.product -notin @('Z.T.L','Z.T.L.ServerManager')) { throw '安装清单无效。' }
$expectedExe = Join-Path $installRoot ($manifest.product + '.exe')
if (!(Test-Path -LiteralPath $expectedExe)) { throw 'Missing product executable.' }
if (Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $expectedExe }) { throw '请先从托盘退出 Z.T.L。' }
if ($Data -eq 'Ask') {
    if ($WhatIfPreference) { $Data = 'Keep' }
    else { $answer = Read-Host '删除 data 中的全部个人数据？输入 DELETE 删除，其他输入均保留'; $Data = if ($answer -ceq 'DELETE') { 'Delete' } else { 'Keep' } }
}
# Resolve and validate every target before any removal; never follow a manifest outside this installation.
$targets = @($manifest.entries)
if ($Data -eq 'Delete') { $targets += 'data' }
$resolvedTargets = foreach ($entry in $targets) {
    if ([string]::IsNullOrWhiteSpace($entry) -or [IO.Path]::IsPathRooted($entry) -or $entry -match '(^|[/\\])\.\.?([/\\]|$)' -or $entry.Contains(':')) { throw '安装清单包含非法路径。' }
    if ($entry -match '^data([/\\]|$)' -and $Data -ne 'Delete') { continue }
    if ($entry -match '^data[/\\]') { throw 'Manifest must not own user data.' }
    $target = [IO.Path]::GetFullPath((Join-Path $installRoot $entry))
    if (!$target.StartsWith($installRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw '卸载路径超出安装目录。' }
    $parentPath = Split-Path $target
    while ($parentPath -and $parentPath -ne $installRoot) {
        if ((Test-Path -LiteralPath $parentPath) -and ((Get-Item -LiteralPath $parentPath -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw '卸载路径经过链接。' }
        $parentPath = Split-Path $parentPath
    }
    if (Test-Path -LiteralPath $target) {
        $item = Get-Item -LiteralPath $target -Force
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw '卸载目标包含链接，请先移除链接。' }
        if ($item.PSIsContainer -and $entry -ne 'data') {
            if ($manifest.format -eq 2) { throw '文件清单包含目录。' }
            if (@(Get-ChildItem -LiteralPath $target -Force).Count -gt 0) { continue }
        }
        if ($item.PSIsContainer -and @(Get-ChildItem -LiteralPath $target -Recurse -Force -Attributes ReparsePoint).Count -gt 0) { throw '卸载目录包含链接，请先移除链接。' }
    }
    $target
}
$runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$command = (Get-ItemProperty -LiteralPath $runKey -Name 'Z.T.L' -ErrorAction SilentlyContinue).'Z.T.L'
if ($command -eq ('"' + $expectedExe + '" --autostart') -and $PSCmdlet.ShouldProcess('Z.T.L', '删除开机自启动项')) {
    Remove-ItemProperty -LiteralPath $runKey -Name 'Z.T.L'
}
foreach ($target in $resolvedTargets) {
    if ((Test-Path -LiteralPath $target) -and $PSCmdlet.ShouldProcess($target, '卸载')) { Remove-Item -LiteralPath $target -Recurse -Force }
}
Write-Output "卸载操作完成；data 选择：$Data。安装目录中未列入清单的文件会保留。"
