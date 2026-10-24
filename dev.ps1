param(
    [ValidateSet('Save', 'Classic', 'Open', 'Manage', 'Background')]
    [string]$Mode = 'Save',
    [switch]$Rebuild,
    [switch]$NoBuild,
    [switch]$Stop,
    [switch]$UiTest
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$buildDirectory = Join-Path $root 'build'
$appFile = Join-Path $buildDirectory 'Jump.exe'
$hostFile = Join-Path $buildDirectory 'Jump.TestHost.exe'
$releaseFile = Join-Path $root 'dist\Jump\Jump.exe'
$devDirectory = Join-Path $buildDirectory 'dev'
$configFile = Join-Path $devDirectory 'settings.ini'
$stampFile = Join-Path $devDirectory 'build-state.json'
New-Item -ItemType Directory -Force -Path $devDirectory | Out-Null

function Get-SourceHash([string]$File) {
    $hash = [System.Security.Cryptography.SHA256]::Create()
    $stream = [System.IO.File]::OpenRead($File)
    try { return [BitConverter]::ToString($hash.ComputeHash($stream)) }
    finally { $stream.Dispose(); $hash.Dispose() }
}

# Only operate on Jump windows belonging to processes at this project's exact paths.
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class JumpDevWindows {
    public delegate bool EnumProc(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback, IntPtr parameter);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr window, StringBuilder name, int count);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr first, IntPtr second);
    public static bool HasController(int process) {
        bool found = false;
        EnumWindows((window, parameter) => {
            uint owner; GetWindowThreadProcessId(window, out owner);
            var name = new StringBuilder(256); GetClassName(window, name, name.Capacity);
            if (owner == process && name.ToString() == "Jump.Control") found = true;
            return true;
        }, IntPtr.Zero);
        return found;
    }
    public static void Close(int process) {
        EnumWindows((window, parameter) => {
            uint owner; GetWindowThreadProcessId(window, out owner);
            if (owner != process) return true;
            var name = new StringBuilder(256); GetClassName(window, name, name.Capacity);
            if (name.ToString() == "#32770") PostMessage(window, 0x111, new IntPtr(2), IntPtr.Zero);
            else if (name.ToString() == "Jump.Control") PostMessage(window, 0x10, IntPtr.Zero, IntPtr.Zero);
            return true;
        }, IntPtr.Zero);
    }
}
'@

function Stop-ProjectProcess([string]$File) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($File)
    foreach ($process in @(Get-Process -Name $name -ErrorAction SilentlyContinue)) {
        if (!$process.Path -or ![string]::Equals($process.Path, $File, [System.StringComparison]::OrdinalIgnoreCase)) { continue }
        [JumpDevWindows]::Close($process.Id)
        # A debugged or unresponsive development process must not lock the next build forever.
        if (!$process.WaitForExit(2000)) { Stop-Process -InputObject $process -Force; $process.WaitForExit() }
    }
}

Stop-ProjectProcess $hostFile
Stop-ProjectProcess $appFile
if ($Stop) { Write-Host 'Development session stopped.'; exit 0 }
if ($Rebuild -and $NoBuild) { throw 'Choose either -Rebuild or -NoBuild.' }

$inputs = @((Get-Item -LiteralPath (Join-Path $root 'build.ps1')))
$inputs += @(Get-ChildItem -LiteralPath (Join-Path $root 'src') -File -Recurse)
$sourceKey = 'debug|' + (($inputs | Sort-Object FullName | ForEach-Object {
    $_.FullName + ':' + (Get-SourceHash $_.FullName)
}) -join '|')
$sha = [System.Security.Cryptography.SHA256]::Create()
try { $signature = [BitConverter]::ToString($sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($sourceKey))) }
finally { $sha.Dispose() }
$previous = $null
if (Test-Path -LiteralPath $stampFile) {
    try { $previous = Get-Content -LiteralPath $stampFile -Raw | ConvertFrom-Json } catch { $previous = $null }
}
$available = (Test-Path -LiteralPath $appFile) -and (Test-Path -LiteralPath $hostFile)
$changed = !$available -or !$previous -or $previous.signature -ne $signature
if (!$changed) {
    # Another build command may have replaced the executables without changing source files.
    $changed = $previous.appHash -ne (Get-SourceHash $appFile) -or
               $previous.hostHash -ne (Get-SourceHash $hostFile)
}
if ($NoBuild -and !$available) { throw 'No build exists yet. Run Start-Test.cmd once without -NoBuild.' }
if (!$NoBuild -and ($Rebuild -or $changed)) {
    Write-Host 'Source changed: building the development version (no packaging)...'
    & (Join-Path $root 'build.ps1') -Debug
    if (!$?) { throw 'Build failed; the previous executable will not be launched.' }
    @{ signature = $signature; appHash = (Get-SourceHash $appFile);
       hostHash = (Get-SourceHash $hostFile) } |
        ConvertTo-Json | Set-Content -LiteralPath $stampFile -Encoding UTF8
} else { Write-Host 'Using the existing development build.' }

# Switch away from this project's portable copy so two assistants do not attach to one dialog.
Stop-ProjectProcess $releaseFile
$saveDirectory = Join-Path $devDirectory 'saves'
$sourceDirectory = Join-Path $devDirectory 'source'
New-Item -ItemType Directory -Force -Path $saveDirectory,$sourceDirectory | Out-Null
$sourceFile = Join-Path $sourceDirectory 'sample.txt'
if (!(Test-Path -LiteralPath $sourceFile)) {
    [System.IO.File]::WriteAllText($sourceFile, 'Jump development test document', [System.Text.Encoding]::UTF8)
}
if (!(Test-Path -LiteralPath $configFile)) {
    # Development folders are not user favorites. Keep the same empty initial
    # collection as the app; folders enter it through the star or management.
    $settings = "[Jump]`r`nschema=1`r`nFavorite=0`r`nHistory=0`r`nmodifiers=0`r`nrecipes=0`r`n"
    [System.IO.File]::WriteAllText($configFile, $settings, [System.Text.Encoding]::Unicode)
}
$appArguments = @('--config', ('"{0}"' -f $configFile))
if ($UiTest) { $appArguments += '--test-ui' }
# Refresh only adapters previously installed for this exact development app.
# Do not opt other Blender versions or another Jump checkout into integration.
$blenderSettings = Join-Path $env:APPDATA 'Blender Foundation\Blender'
$bridgeSource = Join-Path $root 'integrations\blender\jump_save_bridge.py'
if ((Test-Path -LiteralPath $blenderSettings) -and (Test-Path -LiteralPath $bridgeSource)) {
    foreach ($versionFolder in @(Get-ChildItem -LiteralPath $blenderSettings -Directory)) {
        $bridgeFolder = Join-Path $versionFolder.FullName 'scripts\startup'
        $bridgeConfig = Join-Path $bridgeFolder 'jump_save_bridge.json'
        $bridgeFile = Join-Path $bridgeFolder 'jump_save_bridge.py'
        if (!(Test-Path -LiteralPath $bridgeConfig) -or !(Test-Path -LiteralPath $bridgeFile)) { continue }
        try { $bridgeOptions = Get-Content -LiteralPath $bridgeConfig -Raw | ConvertFrom-Json } catch { continue }
        if (@($bridgeOptions.apps) -notcontains $appFile) { continue }
        if (![System.IO.File]::ReadAllText($bridgeFile).StartsWith('"""Jump Blender bridge v1:')) { continue }
        if ((Get-SourceHash $bridgeSource) -ne (Get-SourceHash $bridgeFile)) {
            Copy-Item -LiteralPath $bridgeSource -Destination $bridgeFile -Force
        }
    }
}
# Houdini uses its own package and never replaces an existing user uiready.py.
$houdiniDocuments = [Environment]::GetFolderPath('MyDocuments')
$houdiniSource = Join-Path $root 'integrations\houdini'
foreach ($preferences in @(Get-ChildItem -LiteralPath $houdiniDocuments -Directory -Filter 'houdini*' -ErrorAction SilentlyContinue)) {
    $adapter = Join-Path $preferences.FullName 'jump_scene_save'
    if (!(Test-Path -LiteralPath $adapter)) { continue }
    foreach ($library in @(Get-ChildItem -LiteralPath $adapter -Directory -Filter 'python*libs')) {
        $optionsFile = Join-Path $library.FullName 'jump_scene_save.json'
        if (!(Test-Path -LiteralPath $optionsFile)) { continue }
        try { $options = Get-Content -LiteralPath $optionsFile -Raw | ConvertFrom-Json } catch { continue }
        if ($options.jump_owner -ne 'Jump Houdini scene-save v1' -or @($options.apps) -notcontains $appFile) { continue }
        if (!$options.native_library) { continue } # Do not re-enable/migrate the rejected window replacement.
        foreach ($entry in @(@('jump_scene_save.py', '"""Jump Houdini native-save adapter v2:'),
                             @('jump_pipe.py', '"""Jump local message pipe;'),
                             @('uiready.py', '# Jump Houdini native-save startup v2.'))) {
            $installed = Join-Path $library.FullName $entry[0]
            $source = Join-Path $houdiniSource $entry[0]
            if (!(Test-Path -LiteralPath $installed) -or !(Test-Path -LiteralPath $source)) { continue }
            if (![System.IO.File]::ReadAllText($installed).StartsWith($entry[1])) { continue }
            if ((Get-SourceHash $source) -ne (Get-SourceHash $installed)) {
                Copy-Item -LiteralPath $source -Destination $installed -Force
            }
        }
    }
}
if ($Mode -eq 'Manage') { $appArguments += '--manage' }
$app = Start-Process -FilePath $appFile -ArgumentList $appArguments -WorkingDirectory $root -WindowStyle Hidden -PassThru
$ready = $false
for ($attempt = 0; $attempt -lt 100; $attempt++) {
    if ($app.HasExited) { throw "Development app exited: $($app.ExitCode)" }
    if ([JumpDevWindows]::HasController($app.Id)) { $ready = $true; break }
    Start-Sleep -Milliseconds 50
}
if (!$ready) { throw 'Development app did not become ready.' }
if ($Mode -in @('Save','Classic','Open')) {
    $hostArguments = @('--output', ('"{0}"' -f $saveDirectory), '--source', ('"{0}"' -f $sourceFile))
    if ($Mode -eq 'Classic') { $hostArguments += '--classic' }
    if ($Mode -eq 'Open') { $hostArguments += '--open' }
    # This is the visible interactive test window the user explicitly requested.
    Start-Process -FilePath $hostFile -ArgumentList $hostArguments -WorkingDirectory $root -WindowStyle Normal | Out-Null
}
Write-Host "Ready: $Mode. Development presets persist in $configFile"
Write-Host 'Run Start-Test.cmd again after edits. Use Start-Test.cmd -Stop to exit the test session.'
