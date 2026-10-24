param(
    [string]$Version = '22.0',
    [string]$PythonVersion = '3.13',
    [string]$AppPath,
    [string]$PreferencesDirectory
)
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^\d+\.\d+$' -or $PythonVersion -notmatch '^3\.\d+$') {
    throw 'Use a Houdini major.minor version and Python major.minor version.'
}
if (!$AppPath) {
    $AppPath = Join-Path $PSScriptRoot '..\..\Jump.exe'
    if (!(Test-Path -LiteralPath $AppPath)) { $AppPath = Join-Path $PSScriptRoot '..\..\build\Jump.exe' }
}
$appFile = (Resolve-Path -LiteralPath $AppPath).Path
$nativeSource = Join-Path $PSScriptRoot 'native\22.0.368\jump_houdini_native.dll'
if (!(Test-Path -LiteralPath $nativeSource)) {
    $nativeSource = Join-Path $PSScriptRoot '..\..\build\houdini\22.0.368\jump_houdini_native.dll'
}
if ($Version -ne '22.0' -or $PythonVersion -ne '3.13' -or !(Test-Path -LiteralPath $nativeSource)) {
    throw 'Build the native Houdini 22.0.368 adapter with integrations/houdini/build_native.ps1 first.'
}
$nativeHash = (Get-FileHash -LiteralPath $nativeSource -Algorithm SHA256).Hash.ToLower()
$nativeName = 'jump_houdini_native-' + $nativeHash.Substring(0, 12) + '.dll'
if (!$PreferencesDirectory) {
    $PreferencesDirectory = Join-Path ([Environment]::GetFolderPath('MyDocuments')) ('houdini' + $Version)
}
$root = [System.IO.Path]::GetFullPath($PreferencesDirectory)
$packageDirectory = Join-Path $root 'packages'
$adapterRoot = Join-Path $root 'jump_scene_save'
$library = Join-Path $adapterRoot ('python' + $PythonVersion + 'libs')
$package = Join-Path $packageDirectory 'jump_scene_save.json'
$script = Join-Path $library 'jump_scene_save.py'
$startup = Join-Path $library 'uiready.py'
$config = Join-Path $library 'jump_scene_save.json'
# Only modify our own package. Leave existing Houdini startup scripts untouched.
if ((Test-Path -LiteralPath $package)) {
    $previous = Get-Content -LiteralPath $package -Raw | ConvertFrom-Json
    if ($previous.jump_owner -ne 'Jump Houdini scene-save v1') { throw "Unrelated package: $package" }
}
foreach ($item in @(@($script, '"""Jump Houdini '),
                     @((Join-Path $library 'jump_pipe.py'), '"""Jump local message pipe;'),
                     @($startup, '# Jump Houdini '))) {
    if ((Test-Path -LiteralPath $item[0]) -and
        ![System.IO.File]::ReadAllText($item[0]).StartsWith($item[1])) {
        throw "Refusing to overwrite unrelated file: $($item[0])"
    }
}
if (Test-Path -LiteralPath $config) {
    $previous = Get-Content -LiteralPath $config -Raw | ConvertFrom-Json
    if ($previous.jump_owner -ne 'Jump Houdini scene-save v1') { throw "Unrelated configuration: $config" }
}
New-Item -ItemType Directory -Force -Path $library,$packageDirectory | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'jump_scene_save.py') -Destination $script -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'uiready.py') -Destination $startup -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'jump_pipe.py') -Destination (Join-Path $library 'jump_pipe.py') -Force
$nativeTarget = Join-Path $library $nativeName
if (!(Test-Path -LiteralPath $nativeTarget)) {
    Copy-Item -LiteralPath $nativeSource -Destination $nativeTarget
} elseif ((Get-FileHash -LiteralPath $nativeTarget).Hash.ToLower() -ne $nativeHash) {
    throw "Native adapter identity mismatch: $nativeTarget"
}
@{jump_owner='Jump Houdini scene-save v1'; apps=@($appFile); houdini_build='22.0.368'; native_library=$nativeName} |
    ConvertTo-Json | Set-Content -LiteralPath $config -Encoding utf8
@{jump_owner='Jump Houdini scene-save v1'; hpath=$adapterRoot.Replace('\','/')} |
    ConvertTo-Json | Set-Content -LiteralPath $package -Encoding utf8
Write-Host "Installed Houdini native-window integration: $package"
Write-Host 'Available on the next Houdini start. Existing scenes and startup scripts are preserved.'
