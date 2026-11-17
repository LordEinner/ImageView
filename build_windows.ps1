<#
.SYNOPSIS
Builds ImageView for Windows into dist\ImageView (a self-contained folder with
the Qt DLLs, plugins and MSVC runtime) and zips it as ImageView-windows-x64.zip.

.DESCRIPTION
Needs Visual Studio 2022 (or its Build Tools) with the "Desktop development
with C++" workload, which includes CMake and Ninja, and Qt 6.9+ for MSVC 2022
64-bit with the Qt Image Formats module (WebP, TIFF, TGA, ICNS...).

.PARAMETER QtDir
The Qt kit folder, e.g. C:\Qt\6.11.3\msvc2022_64. Defaults to $env:QTDIR or the
newest C:\Qt\<version>\msvc2022_64.

.EXAMPLE
.\build_windows.ps1
.\build_windows.ps1 -QtDir D:\Qt\6.10.1\msvc2022_64
#>
param(
    [string]$QtDir = $env:QTDIR,
    [switch]$NoZip
)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

if (-not $QtDir) {
    $QtDir = Get-ChildItem C:\Qt -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' -and (Test-Path "$($_.FullName)\msvc2022_64\bin\Qt6Core.dll") } |
        Sort-Object { [version]$_.Name } | Select-Object -Last 1 |
        ForEach-Object { "$($_.FullName)\msvc2022_64" }
}
if (-not $QtDir -or -not (Test-Path "$QtDir\lib\cmake\Qt6")) {
    throw "Qt for MSVC 2022 64-bit not found. Pass -QtDir C:\Qt\<version>\msvc2022_64 or set QTDIR."
}
Write-Host "Using Qt: $QtDir"

# Load the 64-bit MSVC environment (cl, link, rc, plus VS's own CMake and Ninja).
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw 'Visual Studio 2022 with the C++ workload not found.' }
    cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($Matches[1])" $Matches[2] }
    }
}

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtDir"
if ($LASTEXITCODE) { throw 'CMake configure failed' }
cmake --build build
if ($LASTEXITCODE) { throw 'Build failed' }

if (Test-Path dist\ImageView) { Remove-Item -Recurse -Force dist\ImageView }
cmake --install build --prefix dist\ImageView
if ($LASTEXITCODE) { throw 'Install/deploy failed' }

# Plugins a desktop image viewer doesn't need.
Remove-Item -Force -ErrorAction SilentlyContinue `
    dist\ImageView\plugins\imageformats\qpdf.dll,
    dist\ImageView\plugins\platforms\qdirect2d.dll,
    dist\ImageView\plugins\platforms\qminimal.dll,
    dist\ImageView\plugins\platforms\qoffscreen.dll

if (-not $NoZip) {
    $zip = 'ImageView-windows-x64.zip'
    if (Test-Path $zip) { Remove-Item $zip }
    Compress-Archive -Path dist\ImageView -DestinationPath $zip
    Write-Host "Built $zip"
}
Write-Host 'Run: dist\ImageView\ImageView.exe [image]'
