<#
.SYNOPSIS
Installs ImageView for the current user (no admin rights needed unless the
destination needs them): copies dist\ImageView to the destination folder
(default %LOCALAPPDATA%\Programs\ImageView), adds a Start menu shortcut
and registers ImageView under "Open with" for image files, so it can be picked
in Settings > Apps > Default apps. Run .\build_windows.ps1 first.

.PARAMETER Destination
Folder to install into, e.g. "D:\Program Files\ImageView". Defaults to the
previous install location, or %LOCALAPPDATA%\Programs\ImageView. An existing
folder is only replaced if it is empty or holds an earlier ImageView.

.PARAMETER Uninstall
Removes the installed files, shortcut and registry entries again.
#>
param([string]$Destination, [switch]$Uninstall)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

$Previous = (Get-ItemProperty 'HKCU:\Software\ImageView' -ErrorAction SilentlyContinue).InstallDir
$Dest = if ($Destination) { $Destination } elseif ($Previous) { $Previous } else { Join-Path $env:LOCALAPPDATA 'Programs\ImageView' }
$Dest = [IO.Path]::GetFullPath($Dest).TrimEnd('\')
$Exe = Join-Path $Dest 'ImageView.exe'

# Never wipe a folder that isn't an ImageView install (e.g. a mistyped
# "D:\Program Files").
function Test-SafeToReplace($dir) {
    if (-not (Test-Path $dir)) { return $true }
    if (-not (Get-ChildItem -Force $dir | Select-Object -First 1)) { return $true }
    return Test-Path (Join-Path $dir 'ImageView.exe')
}
if (-not (Test-SafeToReplace $Dest)) {
    throw "$Dest exists and is not an ImageView install; refusing to replace it. Pick an empty or new folder."
}
$Shortcut = Join-Path ([Environment]::GetFolderPath('Programs')) 'ImageView.lnk'
$ProgId = 'ImageView.Image'
$Classes = 'HKCU:\Software\Classes'
$Capabilities = 'HKCU:\Software\ImageView\Capabilities'
$Extensions = '.png .jpg .jpeg .jpe .jfif .gif .webp .bmp .dib .tif .tiff .ico .tga .svg .wbmp .icns' -split ' '

function Remove-Key($path) { if (Test-Path $path) { Remove-Item -Recurse -Force $path } }

# Undo any previous registration first, so reinstalling is clean.
foreach ($ext in $Extensions) {
    $k = "$Classes\$ext\OpenWithProgids"
    if (Test-Path $k) { Remove-ItemProperty -Path $k -Name $ProgId -ErrorAction SilentlyContinue }
}
Remove-Key "$Classes\$ProgId"
Remove-Key "$Classes\Applications\ImageView.exe"
Remove-Key 'HKCU:\Software\ImageView'
Remove-ItemProperty -Path 'HKCU:\Software\RegisteredApplications' -Name 'ImageView' -ErrorAction SilentlyContinue
if (Test-Path $Shortcut) { Remove-Item -Force $Shortcut }

if ($Uninstall) {
    Remove-Key $Dest
    Write-Host 'ImageView uninstalled.'
    return
}

if (-not (Test-Path 'dist\ImageView\ImageView.exe')) { throw 'Run .\build_windows.ps1 first.' }
Get-Process ImageView -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $Exe } | Stop-Process
Remove-Key $Dest
New-Item -ItemType Directory -Force (Split-Path $Dest) | Out-Null
Copy-Item -Recurse 'dist\ImageView' $Dest
New-Item -Force 'HKCU:\Software\ImageView' | Out-Null
Set-ItemProperty 'HKCU:\Software\ImageView' -Name 'InstallDir' -Value $Dest

$shell = New-Object -ComObject WScript.Shell
$lnk = $shell.CreateShortcut($Shortcut)
$lnk.TargetPath = $Exe
$lnk.WorkingDirectory = $Dest
$lnk.Description = 'Simple image viewer'
$lnk.Save()

$open = "`"$Exe`" `"%1`""
New-Item -Force "$Classes\$ProgId\DefaultIcon" -Value "`"$Exe`",0" | Out-Null
New-Item -Force "$Classes\$ProgId\shell\open\command" -Value $open | Out-Null
Set-ItemProperty "$Classes\$ProgId" -Name '(default)' -Value 'Image'
Set-ItemProperty "$Classes\$ProgId" -Name 'FriendlyTypeName' -Value 'Image'

New-Item -Force "$Classes\Applications\ImageView.exe\shell\open\command" -Value $open | Out-Null
Set-ItemProperty "$Classes\Applications\ImageView.exe" -Name 'FriendlyAppName' -Value 'ImageView'
New-Item -Force "$Classes\Applications\ImageView.exe\SupportedTypes" | Out-Null

New-Item -Force "$Capabilities\FileAssociations" | Out-Null
Set-ItemProperty $Capabilities -Name 'ApplicationName' -Value 'ImageView'
Set-ItemProperty $Capabilities -Name 'ApplicationDescription' -Value 'Simple image viewer'
New-Item -Force 'HKCU:\Software\RegisteredApplications' | Out-Null
Set-ItemProperty 'HKCU:\Software\RegisteredApplications' -Name 'ImageView' -Value 'Software\ImageView\Capabilities'

foreach ($ext in $Extensions) {
    New-Item -Force "$Classes\$ext\OpenWithProgids" | Out-Null
    New-ItemProperty -Force -Path "$Classes\$ext\OpenWithProgids" -Name $ProgId -PropertyType String -Value '' | Out-Null
    New-ItemProperty -Force -Path "$Classes\Applications\ImageView.exe\SupportedTypes" -Name $ext -PropertyType String -Value '' | Out-Null
    New-ItemProperty -Force -Path "$Capabilities\FileAssociations" -Name $ext -PropertyType String -Value $ProgId | Out-Null
}

# Tell Explorer the associations changed.
Add-Type -Namespace Win32 -Name Shell -MemberDefinition '[DllImport("shell32.dll")] public static extern void SHChangeNotify(int e, uint f, IntPtr a, IntPtr b);'
[Win32.Shell]::SHChangeNotify(0x08000000, 0, [IntPtr]::Zero, [IntPtr]::Zero)

Write-Host "Installed to $Dest"
Write-Host 'To make it the default viewer: Settings > Apps > Default apps > ImageView.'
