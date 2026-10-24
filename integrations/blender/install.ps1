param(
    [string]$Version = '5.2',
    [string]$AppPath
)
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^\d+\.\d+$') { throw 'Use a Blender major.minor version, e.g. 5.2.' }
if (!$AppPath) {
    $AppPath = Join-Path $PSScriptRoot '..\..\Jump.exe'
    if (!(Test-Path -LiteralPath $AppPath)) { $AppPath = Join-Path $PSScriptRoot '..\..\build\Jump.exe' }
}
$appFile = (Resolve-Path -LiteralPath $AppPath).Path
$scripts = Join-Path $env:APPDATA "Blender Foundation\Blender\$Version\scripts\startup"
New-Item -ItemType Directory -Force -Path $scripts | Out-Null
$destination = Join-Path $scripts 'jump_save_bridge.py'
if ((Test-Path -LiteralPath $destination) -and
    !([System.IO.File]::ReadAllText($destination).StartsWith('"""Jump Blender bridge v1:'))) {
    throw "Refusing to overwrite an unrelated file: $destination"
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'jump_save_bridge.py') -Destination $destination -Force
$config = @{ apps = @($appFile) } | ConvertTo-Json
[System.IO.File]::WriteAllText((Join-Path $scripts 'jump_save_bridge.json'), $config, [System.Text.UTF8Encoding]::new($false))
Write-Host "Installed: $destination"
Write-Host 'Loads automatically when Blender starts. Existing sessions can import jump_save_bridge and call register().'
