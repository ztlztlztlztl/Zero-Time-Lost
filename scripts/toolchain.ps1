function Get-ZtlToolchain([string]$QtRoot) {
    $tools = Join-Path (Split-Path (Split-Path $QtRoot)) 'Tools'
    $result = @{ CMake=(Join-Path $tools 'CMake_64\bin\cmake.exe'); Ninja=(Join-Path $tools 'Ninja\ninja.exe'); Compiler=(Join-Path $tools 'mingw1310_64\bin\g++.exe'); BinPath=((Join-Path $tools 'mingw1310_64\bin') + ';' + (Join-Path $QtRoot 'bin')) }
    foreach ($entry in @(@('CMake','ZTL_CMAKE'),@('Ninja','ZTL_NINJA'),@('Compiler','ZTL_CXX'))) { $override = [Environment]::GetEnvironmentVariable($entry[1]); if ($override) { $result[$entry[0]] = $override } }
    $result.BinPath = (Split-Path $result.Compiler) + ';' + (Join-Path $QtRoot 'bin')
    foreach ($name in 'CMake','Ninja','Compiler') { if (!(Test-Path -LiteralPath $result[$name])) { throw "Missing tool: $($result[$name]); set QT_ROOT or -QtRoot." } }
    return $result
}
