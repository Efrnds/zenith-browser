$ErrorActionPreference = 'Stop'

$targetQt = $env:QT_ROOT_DIR
if (-not $targetQt) { throw 'QT_ROOT_DIR is missing' }
$hostQt = Join-Path (Split-Path $targetQt -Parent) 'msvc2022_64'
$hostQtPaths = Join-Path $hostQt 'bin/qtpaths6.exe'
if (-not (Test-Path $hostQtPaths)) { throw "x64 qtpaths is missing: $hostQtPaths" }

$targetBin = Join-Path $targetQt 'bin'
$targetConf = Join-Path $targetBin 'zenith-target-qt.conf'
$targetQtPaths = Join-Path $targetBin 'zenith-qtpaths.bat'
$targetPrefix = $targetQt.Replace('\', '/')
$conf = "[Paths]`r`nPrefix = $targetPrefix`r`n"
$wrapper = "@echo off`r`n`"$hostQtPaths`" --qtconf `"$targetConf`" %*`r`n"
[System.IO.File]::WriteAllText($targetConf, $conf)
[System.IO.File]::WriteAllText($targetQtPaths, $wrapper)

$reported = & $targetQtPaths --query QT_INSTALL_PREFIX
if ($LASTEXITCODE -ne 0) { throw "Target Qt paths query failed: $targetQtPaths" }
$reportedPrefix = ($reported | Select-Object -Last 1).Trim()
if ($reportedPrefix.Replace('\', '/').TrimEnd('/') -ne $targetPrefix.TrimEnd('/')) {
    throw "Target Qt paths query returned '$reportedPrefix' instead of '$targetPrefix'"
}
Write-Host "ARM64 Qt deployment paths verified: $reportedPrefix"
