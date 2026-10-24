param([string]$HoudiniRoot='C:\Program Files\Side Effects Software\Houdini 22.0.368')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$configuration=Get-Content -LiteralPath (Join-Path $HoudiniRoot 'toolkit\cmake\HoudiniConfigVersion.cmake') -Raw
$version=[regex]::Match($configuration,'set\( PACKAGE_VERSION ([\d.]+) \)').Groups[1].Value
if($version -ne '22.0.368'){throw 'Native UI symbols have only been verified for Houdini 22.0.368.'}
$out=Join-Path $root "build\houdini\$version"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Visual Studio C++ Build Tools are required to build the native adapter.'}
$environment=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
[IO.File]::WriteAllText((Join-Path $out 'version.h'),"#define JUMP_HOUDINI_VERSION `"$version`"`n",[Text.Encoding]::ASCII)
$script=@"
@echo off
call "$environment" >nul
if errorlevel 1 exit /b 1
cd /d "$out"
cl /nologo /LD /MD /O2 /utf-8 /std:c++20 /Zc:__cplusplus /EHsc /GR /DNDEBUG /DUT_ASSERT_LEVEL=0 /DI386 /DWIN32 /D_USE_MATH_DEFINES /DAMD64 /DSIZEOF_VOID_P=8 /DSESI_LITTLE_ENDIAN /DSWAP_BITFIELDS /DNOMINMAX /DWIN32_LEAN_AND_MEAN /DMAKING_DSO /FI"$out\version.h" /I"$HoudiniRoot\toolkit\include" "$PSScriptRoot\native_bridge.cpp" /link /OUT:jump_houdini_native.dll /LIBPATH:"$HoudiniRoot\custom\houdini\dsolib" libSI.lib libUI.lib libUT.lib libSYS.lib Qt6Widgets.lib Qt6Gui.lib Qt6Core.lib
exit /b %errorlevel%
"@
[IO.File]::WriteAllText((Join-Path $out 'compile.cmd'),$script,[Text.Encoding]::ASCII)
& $env:ComSpec /d /c (Join-Path $out 'compile.cmd')
if($LASTEXITCODE){throw 'Houdini native adapter build failed.'}
Write-Host "Built native Houdini $version adapter in $out"
