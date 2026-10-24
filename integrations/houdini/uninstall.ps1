param([string]$Version = '22.0', [string]$PreferencesDirectory)
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^\d+\.\d+$') { throw 'Use a Houdini major.minor version.' }
if (!$PreferencesDirectory) {
    $PreferencesDirectory = Join-Path ([Environment]::GetFolderPath('MyDocuments')) ('houdini' + $Version)
}
$package = Join-Path ([System.IO.Path]::GetFullPath($PreferencesDirectory)) 'packages\jump_scene_save.json'
if (Test-Path -LiteralPath $package) {
    $previous = Get-Content -LiteralPath $package -Raw | ConvertFrom-Json
    if ($previous.jump_owner -ne 'Jump Houdini scene-save v1') { throw "Unrelated package: $package" }
    # Disabling our package suffices; no recursive deletion of Houdini preferences.
    $previous | Add-Member -NotePropertyName enable -NotePropertyValue $false -Force
    $previous | ConvertTo-Json | Set-Content -LiteralPath $package -Encoding utf8
}
Write-Host 'Jump scene-save integration disabled. Takes effect on the next Houdini start.'
