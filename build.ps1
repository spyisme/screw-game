param(
    [string]$BuiltExe = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Join-Path $root "client\Skrew"
$finalDir = Join-Path $root "final"
$dllDir = Join-Path $finalDir "dlls"
$sfmlDir = "C:\Users\Spy\source\SFML-3.0.2"
$sfmlBin = Join-Path $sfmlDir "bin"

if (-not $BuiltExe) {
    $BuiltExe = Join-Path $root "client\Skrew\x64\Release\Skrew.exe"
}

if (-not (Test-Path -LiteralPath $BuiltExe)) {
    throw "Built executable not found: $BuiltExe"
}

if (-not (Test-Path -LiteralPath $sfmlBin)) {
    throw "SFML bin folder not found: $sfmlBin"
}

if (Test-Path -LiteralPath $finalDir) {
    Remove-Item -LiteralPath $finalDir -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $finalDir | Out-Null
New-Item -ItemType Directory -Force -Path $dllDir | Out-Null

Copy-Item -LiteralPath $BuiltExe -Destination (Join-Path $finalDir "Skrew.exe") -Force

$sfmlDlls = @(
    "sfml-audio-3.dll",
    "sfml-graphics-3.dll",
    "sfml-network-3.dll",
    "sfml-system-3.dll",
    "sfml-window-3.dll"
)

foreach ($dll in $sfmlDlls) {
    Copy-Item -LiteralPath (Join-Path $sfmlBin $dll) -Destination $dllDir -Force
}

$redistRoot = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Redist\MSVC"
$redist = Get-ChildItem -LiteralPath $redistRoot -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match '^\d+\.' } |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "x64\Microsoft.VC145.CRT") } |
    Sort-Object Name -Descending |
    Select-Object -First 1

if ($redist) {
    $crtDir = Join-Path $redist.FullName "x64\Microsoft.VC145.CRT"
    Copy-Item -Path (Join-Path $crtDir "*.dll") -Destination $dllDir -Force
}

$ucrtRoot = "C:\Program Files (x86)\Windows Kits\10\Redist"
$ucrt = Get-ChildItem -LiteralPath $ucrtRoot -Directory -ErrorAction SilentlyContinue |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "ucrt\DLLs\x64\ucrtbase.dll") } |
    Sort-Object Name -Descending |
    Select-Object -First 1

if ($ucrt) {
    $ucrtDir = Join-Path $ucrt.FullName "ucrt\DLLs\x64"
    Copy-Item -Path (Join-Path $ucrtDir "*.dll") -Destination $dllDir -Force
}

Copy-Item -LiteralPath (Join-Path $projectDir "config.json") -Destination $finalDir -Force
Copy-Item -LiteralPath (Join-Path $projectDir "assets") -Destination $dllDir -Recurse -Force

$zipPath = Join-Path $root "final.zip"
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

Compress-Archive -Path (Join-Path $finalDir "*") -DestinationPath $zipPath -Force

Write-Host "Packaged Skrew to $finalDir"
Write-Host "Created $zipPath"
