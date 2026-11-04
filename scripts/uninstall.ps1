[CmdletBinding(SupportsShouldProcess)]
param([ValidateSet('Ask','Keep','Delete')][string]$Data = 'Ask')
$ErrorActionPreference = 'Stop'
$installRoot = [IO.Path]::GetFullPath($PSScriptRoot).TrimEnd('\')
$manifestPath = Join-Path $installRoot '.ztl-install.json'
if (!(Test-Path -LiteralPath $manifestPath) -or !(Test-Path -LiteralPath (Join-Path $installRoot 'Z.T.L.exe'))) {
    throw '请从完整的 Z.T.L 安装目录运行卸载脚本。'
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.product -ne 'Z.T.L') { throw '安装清单无效。' }
$expectedExe = Join-Path $installRoot 'Z.T.L.exe'
if (Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $expectedExe }) { throw '请先从托盘退出 Z.T.L。' }
if ($Data -eq 'Ask') {
    if ($WhatIfPreference) { $Data = 'Keep' }
    else { $answer = Read-Host '删除 data 中的全部个人数据？输入 DELETE 删除，其他输入均保留'; $Data = if ($answer -ceq 'DELETE') { 'Delete' } else { 'Keep' } }
}
# Resolve and validate every target before any removal; never follow a manifest outside this installation.
$targets = @($manifest.entries)
if ($Data -eq 'Delete') { $targets += 'data' }
$resolvedTargets = foreach ($entry in $targets) {
    if ([string]::IsNullOrWhiteSpace($entry) -or $entry -match '[/\\]' -or $entry -in @('.','..')) { throw '安装清单包含非法路径。' }
    if ($entry -eq 'data' -and $Data -ne 'Delete') { continue }
    $target = [IO.Path]::GetFullPath((Join-Path $installRoot $entry))
    if (!$target.StartsWith($installRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw '卸载路径超出安装目录。' }
    if (Test-Path -LiteralPath $target) {
        $item = Get-Item -LiteralPath $target -Force
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw '卸载目标包含链接，请先移除链接。' }
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
