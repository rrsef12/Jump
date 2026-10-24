param([switch]$Debug)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Visual Studio C++ Build Tools and Windows SDK are required.' }
$environment = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
New-Item -ItemType Directory -Force -Path (Join-Path $root 'build') | Out-Null
$flags = if ($Debug) { '/MTd /Od /Zi /D_DEBUG /Fd:build\compiler.pdb' } else { '/MT /O2 /DNDEBUG' }
$script = @"
@echo off
call "$environment" >nul
if errorlevel 1 exit /b 1
cd /d "$root"
rc /nologo /fo build\jump.res src\jump.rc
if errorlevel 1 exit /b 1
cl /nologo /MP /std:c++20 /EHsc /W4 /utf-8 $flags /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Fo:build\ src\main.cpp src\model.cpp src\workspace.cpp build\jump.res /Fe:build\Jump.exe /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib gdiplus.lib shell32.lib ole32.lib oleaut32.lib comctl32.lib comdlg32.lib dwmapi.lib uxtheme.lib advapi32.lib uuid.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /W4 /utf-8 $flags /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Fo:build\ src\test_host.cpp /Fe:build\Jump.TestHost.exe /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib shell32.lib ole32.lib comdlg32.lib uuid.lib
exit /b %errorlevel%
"@
[System.IO.File]::WriteAllText((Join-Path $root 'build\compile.cmd'), $script, [System.Text.Encoding]::ASCII)
& $env:ComSpec /d /c (Join-Path $root 'build\compile.cmd')
if ($LASTEXITCODE) { throw "Build failed: $LASTEXITCODE" }
Write-Host "Built $root\build\Jump.exe"
