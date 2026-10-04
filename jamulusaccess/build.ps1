# JamulusAccess - Windows build and installer.
#
#   powershell -ExecutionPolicy Bypass -File jamulusaccess\build.ps1             (build + installer)
#   powershell -ExecutionPolicy Bypass -File jamulusaccess\build.ps1 -NoInstaller
#
# Needs: Visual Studio 2022 Build Tools (C++), Qt 6 msvc2022_64 (default C:\Qt\6.10.2\msvc2022_64),
# the Steinberg ASIO SDK in libs\ASIOSDK2 (never committed), Inno Setup 6.
# (the tools run through cmd: Windows PowerShell 5.1 would take their messages for errors)
# Everything it makes goes NEXT TO the repository, never in it:
#   ..\build (compilation), ..\build_log (logs), ..\Output\JamulusAccessInstaller.exe (+ _<version>.exe)
# The version is read from src\global.h (JAMULUSACCESS_VERSION).
param(
    [switch]$NoInstaller,
    [string]$QtDir = 'C:\Qt\6.10.2\msvc2022_64'
)
$ErrorActionPreference = 'Stop'
$Repo     = Split-Path $PSScriptRoot -Parent
$Root     = Split-Path $Repo -Parent
$BuildDir = Join-Path $Root 'build'
$LogDir   = Join-Path $Root 'build_log'
$OutDir   = Join-Path $Root 'Output'
$Stage    = Join-Path $BuildDir 'stage'
$VsDir    = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
if (-not (Test-Path $VsDir)) { $VsDir = 'C:\Program Files\Microsoft Visual Studio\2022\Community' }
$Vcvars   = Join-Path $VsDir 'VC\Auxiliary\Build\vcvars64.bat'
$Iscc     = Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'
if (-not (Test-Path $Iscc)) { $Iscc = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe' }

foreach ($need in @($Vcvars, (Join-Path $QtDir 'bin\qmake.exe'), (Join-Path $Repo 'libs\ASIOSDK2\common'))) {
    if (-not (Test-Path $need)) { throw "Missing: $need" }
}
New-Item -ItemType Directory -Force $BuildDir, $LogDir, $OutDir | Out-Null

# version: the string and the two numbers must agree (the updater compares the numbers)
$global = Get-Content (Join-Path $Repo 'src\global.h') -Raw
$Version = [regex]::Match($global, '#define JAMULUSACCESS_VERSION\s+"([0-9]+\.[0-9]{2})"').Groups[1].Value
$Major   = [regex]::Match($global, '#define JAMULUSACCESS_VERSION_MAJOR\s+([0-9]+)').Groups[1].Value
$Minor   = [regex]::Match($global, '#define JAMULUSACCESS_VERSION_MINOR\s+([0-9]+)').Groups[1].Value
if (-not $Version -or ("{0}.{1:D2}" -f [int]$Major, [int]$Minor) -ne $Version) {
    throw "src\global.h: JAMULUSACCESS_VERSION ($Version) does not match MAJOR.MINOR ($Major.$Minor)"
}
"JamulusAccess $Version"

# the Visual Studio environment, taken into this PowerShell session
cmd /c "`"$Vcvars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($Matches[1])" $Matches[2] }
}
$env:PATH = (Join-Path $QtDir 'bin') + ';' + $env:PATH

Push-Location $BuildDir
try {
    cmd /c "qmake `"$(Join-Path $Repo 'JamulusAccess.pro')`" CONFIG+=release QMAKE_CXXFLAGS+=/MP > `"$(Join-Path $LogDir 'qmake.log')`" 2>&1"
    if ($LASTEXITCODE -ne 0) { throw 'qmake failed, see build_log\qmake.log' }
    cmd /c "nmake release > `"$(Join-Path $LogDir 'build.log')`" 2>&1"
    if ($LASTEXITCODE -ne 0) { throw 'Compilation failed, see build_log\build.log' }
} finally { Pop-Location }
"Built: $(Join-Path $BuildDir 'release\JamulusAccess.exe')"
if ($NoInstaller) { return }

# a clean copy: the program, Qt (with Qt's own translations: Yes, No, Cancel...), the manual and changelog
if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory $Stage | Out-Null
Copy-Item (Join-Path $BuildDir 'release\JamulusAccess.exe') $Stage
New-Item -ItemType Directory (Join-Path $Stage 'docs') | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'docs\*.html') (Join-Path $Stage 'docs')
cmd /c "windeployqt --release --no-compiler-runtime --no-opengl-sw --no-system-d3d-compiler `"$(Join-Path $Stage 'JamulusAccess.exe')`" > `"$(Join-Path $LogDir 'deploy.log')`" 2>&1"
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed, see build_log\deploy.log' }

# Microsoft C++ runtime, put next to the program by the installer
$Redist = Get-ChildItem (Join-Path $VsDir 'VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT') -Directory |
    Sort-Object FullName | Select-Object -Last 1 -ExpandProperty FullName
if (-not $Redist) { throw 'Microsoft.VC143.CRT not found in the Visual Studio folder' }
if (-not (Test-Path $Iscc)) { throw "Missing: Inno Setup ($Iscc)" }

$iscc = Start-Process $Iscc -Wait -PassThru -NoNewWindow -RedirectStandardOutput (Join-Path $LogDir 'installer.log') `
    -RedirectStandardError (Join-Path $LogDir 'installer_errors.log') -ArgumentList @(
    "/DMyAppVersion=$Version", "`"/DAppDir=$Stage`"", "`"/DRedistDir=$Redist`"", "`"/DSrcDir=$Repo`"", "`"/DOutputDir=$OutDir`"",
    "`"$(Join-Path $PSScriptRoot 'installer\JamulusAccess.iss')`"")
if ($iscc.ExitCode -ne 0) { throw 'Inno Setup failed, see build_log\installer.log and installer_errors.log' }

$Installer = Join-Path $OutDir "JamulusAccessInstaller_$Version.exe"
Copy-Item $Installer (Join-Path $OutDir 'JamulusAccessInstaller.exe') -Force
$hash = (Get-FileHash $Installer -Algorithm SHA256).Hash
"Installer: $Installer ($((Get-Item $Installer).Length) bytes, SHA-256 $hash)"
"Release assets: JamulusAccessInstaller.exe (the updater looks for this name) and JamulusAccessInstaller_$Version.exe"
