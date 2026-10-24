$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$version = '0.1.0'
# Always compile Release; an existing build may be the Debug development app.
& (Join-Path $root 'build.ps1')
if (!$?) { throw 'Release build failed; no package was updated.' }
$binary = Join-Path $root 'build\Jump.exe'
$native = Join-Path $root 'build\houdini\22.0.368\jump_houdini_native.dll'
if (!(Test-Path -LiteralPath $native)) { throw 'Build the Houdini 22.0.368 adapter before packaging.' }
if ((Get-Item -LiteralPath $native).LastWriteTimeUtc -lt (Get-Item -LiteralPath (Join-Path $root 'integrations\houdini\native_bridge.cpp')).LastWriteTimeUtc) {
    throw 'The Houdini adapter is older than its source; rebuild it before packaging.'
}
# Fresh staging keeps all personal settings, test hosts and old dist files out.
$stage = Join-Path $root ('build\package-staging\' + [Guid]::NewGuid().ToString('N') + '\Jump')
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item -LiteralPath $binary -Destination (Join-Path $stage 'Jump.exe')
Copy-Item -LiteralPath (Join-Path $root 'README.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'docs\release-quick-start.md') -Destination (Join-Path $stage '开始使用.md')
foreach ($file in @(Get-ChildItem -LiteralPath (Join-Path $root 'integrations') -Recurse -File)) {
    if ($file.Extension -notin '.ps1','.py','.cpp','.md' -or $file.FullName -match '[\\/]__pycache__[\\/]') { continue }
    $target = Join-Path $stage $file.FullName.Substring($root.Length + 1)
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $target
}
$nativeDirectory = Join-Path $stage 'integrations\houdini\native\22.0.368'
New-Item -ItemType Directory -Force -Path $nativeDirectory | Out-Null
Copy-Item -LiteralPath $native -Destination $nativeDirectory
# User docs are whitelisted above (README + quick start); docs/dev stays internal.
$manifest = @{
    version = $version
    configuration = 'Release'
    architecture = 'x64'
    builtAtUtc = [DateTime]::UtcNow.ToString('o')
    files = @(Get-ChildItem -LiteralPath $stage -File -Recurse | Sort-Object FullName | ForEach-Object {
        @{ path = $_.FullName.Substring($stage.Length + 1).Replace('\','/'); sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
    })
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'release-manifest.json') -Encoding UTF8
$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$archive = Join-Path $dist "Jump-$version-win-x64.zip"
$stagedArchive = Join-Path (Split-Path -Parent $stage) "Jump-$version-win-x64.zip"
Compress-Archive -LiteralPath $stage -DestinationPath $stagedArchive
Copy-Item -LiteralPath $stagedArchive -Destination $archive -Force
# Preserve existing portable user data when refreshing the unpacked app.
$destination = Join-Path $dist 'Jump'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
Get-ChildItem -LiteralPath $stage | Copy-Item -Destination $destination -Recurse -Force
$hashes = @(
    (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() + "  Jump-$version-win-x64.zip"
    (Get-FileHash -LiteralPath (Join-Path $destination 'Jump.exe') -Algorithm SHA256).Hash.ToLowerInvariant() + '  Jump/Jump.exe'
)
[System.IO.File]::WriteAllLines((Join-Path $dist 'SHA256.txt'), $hashes, [System.Text.Encoding]::ASCII)
Write-Host "Packaged Release: $archive"
