#requires -Version 7.0
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath $RepositoryRoot).Path
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo 'release' }
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (-not $output.StartsWith($repo + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Release output must be a child of the repository.'
}
$config = Get-Content -Raw -LiteralPath (Join-Path $repo 'xmake.lua')
if ($config -notmatch 'local version = "(\d+\.\d+\.\d+)"') { throw 'Missing release version.' }
$version = $Matches[1]
$dirty = & git -c "safe.directory=$repo" -C $repo status --porcelain --untracked-files=normal
if ($LASTEXITCODE -ne 0 -or $dirty) { throw 'Commit reviewed changes before packaging a source release.' }
$revision = (& git -c "safe.directory=$repo" -C $repo rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve source revision.' }
$dll = Join-Path $repo "build\v$version\windows\x64\release\BodyChangeNG.dll"
if ((Get-Item -LiteralPath $dll).VersionInfo.FileVersion -ne "$version.0") {
    throw 'DLL resource version does not match the source version.'
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\', '/')
$stage = Join-Path $tempRoot ('BCNG-release-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
New-Item -ItemType Directory -Path $output -Force | Out-Null

function Copy-ReleaseFile([string]$RelativePath, [string]$DestinationRoot, [string]$DestinationPath = $RelativePath) {
    $destination = Join-Path $DestinationRoot $DestinationPath
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repo $RelativePath) -Destination $destination
}
function Copy-ReleaseTree([string]$RelativePath, [string]$DestinationRoot) {
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repo $RelativePath) -Recurse -File) {
        Copy-ReleaseFile ([IO.Path]::GetRelativePath($repo, $file.FullName)) $DestinationRoot
    }
}
function New-VerifiedArchive([string]$SourceRoot, [string]$ArchivePath) {
    # Never silently replace a published artifact.
    if (Test-Path -LiteralPath $ArchivePath) { throw "Archive already exists: $ArchivePath" }
    [IO.Compression.ZipFile]::CreateFromDirectory($SourceRoot, $ArchivePath, [IO.Compression.CompressionLevel]::Optimal, $false)
    $archive = [IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        $expected = @{}
        foreach ($file in Get-ChildItem -LiteralPath $SourceRoot -File -Recurse -Force) {
            $entry = [IO.Path]::GetRelativePath($SourceRoot, $file.FullName).Replace('\', '/')
            $expected[$entry] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        }
        $actual = @($archive.Entries | Where-Object { $_.Name })
        if ($actual.Count -ne $expected.Count) { throw 'Archive file count mismatch.' }
        foreach ($entry in $actual) {
            $key = $entry.FullName.Replace('\', '/')
            if (-not $expected.ContainsKey($key)) { throw "Unexpected archive entry: $key" }
            $stream = $entry.Open()
            $sha = [Security.Cryptography.SHA256]::Create()
            try { $hash = [Convert]::ToHexString($sha.ComputeHash($stream)) }
            finally { $sha.Dispose(); $stream.Dispose() }
            if ($hash -ne $expected[$key]) { throw "Archive hash mismatch: $key" }
        }
    } finally { $archive.Dispose() }
}
try {
    $binary = Join-Path $stage 'binary'
    $source = Join-Path $stage 'source'
    New-Item -ItemType Directory -Path $binary, $source | Out-Null
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repo 'package') -File -Recurse) {
        $relative = [IO.Path]::GetRelativePath((Join-Path $repo 'package'), $file.FullName)
        Copy-ReleaseFile (Join-Path 'package' $relative) $binary $relative
    }
    Copy-ReleaseFile "build\v$version\windows\x64\release\BodyChangeNG.dll" $binary 'SKSE\Plugins\BodyChangeNG.dll'
    # MO2 needs runtime files and folder-placement guidance, not release/source docs.
    # Required license terms are consolidated into these two files.
    foreach ($file in @('LICENSE', 'THIRD_PARTY_NOTICES.md')) {
        Copy-ReleaseFile $file $binary
    }
    $expectedBinaryFiles = @(
        'BodySkin/README.txt',
        'CalienteTools/BodySlide/SliderPresets/README.txt',
        'LICENSE',
        'SKSE/Plugins/BodyChangeNG.dll',
        'SKSE/Plugins/BodyChangeNGdistribution.json',
        'THIRD_PARTY_NOTICES.md',
        'TintMask/README.txt'
    )
    $actualBinaryFiles = @(Get-ChildItem -LiteralPath $binary -File -Recurse -Force | ForEach-Object {
        [IO.Path]::GetRelativePath($binary, $_.FullName).Replace('\', '/')
    })
    if (Compare-Object ($expectedBinaryFiles | Sort-Object) ($actualBinaryFiles | Sort-Object)) {
        throw 'MO2 archive must contain only runtime files, asset-folder guidance, and two license documents.'
    }
    $starter = Get-Content -Raw -LiteralPath (Join-Path $binary 'SKSE\Plugins\BodyChangeNGdistribution.json') | ConvertFrom-Json
    if ($starter.schemaVersion -ne 4 -or $starter.rules.Count -ne 8) { throw 'Unexpected starter rule schema/count.' }

    $projectZip = Join-Path $stage 'project.zip'
    & git -c "safe.directory=$repo" -C $repo archive --format=zip "--output=$projectZip" HEAD
    if ($LASTEXITCODE -ne 0) { throw 'git archive failed.' }
    [IO.Compression.ZipFile]::ExtractToDirectory($projectZip, $source)
    # Expand Git's empty submodule placeholders with build-required files only.
    foreach ($tree in @('include', 'src', 'res', 'cmake', 'licenses', 'extern\openvr\headers')) {
        Copy-ReleaseTree "third_party\CommonLibSSE-NG\$tree" $source
    }
    foreach ($file in @('xmake.lua', 'COPYING', 'EXCEPTIONS.md', 'README.md', 'CMakeLists.txt', 'CMakePresets.json', 'vcpkg.json', 'extern\openvr\LICENSE', 'extern\openvr\README.md')) {
        Copy-ReleaseFile "third_party\CommonLibSSE-NG\$file" $source
    }
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repo 'third_party\imgui') -File | Where-Object { $_.Extension -in '.cpp', '.h' -or $_.Name -eq 'LICENSE.txt' }) {
        Copy-ReleaseFile ([IO.Path]::GetRelativePath($repo, $file.FullName)) $source
    }
    foreach ($file in @('backends\imgui_impl_dx11.cpp', 'backends\imgui_impl_dx11.h', 'backends\imgui_impl_win32.cpp', 'backends\imgui_impl_win32.h', 'misc\cpp\imgui_stdlib.cpp', 'misc\cpp\imgui_stdlib.h')) {
        Copy-ReleaseFile "third_party\imgui\$file" $source
    }
    Copy-ReleaseTree 'third_party\pugixml\src' $source
    Copy-ReleaseFile 'third_party\pugixml\LICENSE.md' $source
    foreach ($required in @('third_party\CommonLibSSE-NG\include\RE\Skyrim.h', 'third_party\CommonLibSSE-NG\res\commonlibsse-ng-plugin.cpp.in', 'third_party\CommonLibSSE-NG\extern\openvr\headers\openvr.h', 'third_party\imgui\imgui.cpp', 'third_party\pugixml\src\pugixml.cpp')) {
        if (-not (Test-Path -LiteralPath (Join-Path $source $required))) { throw "Missing source dependency: $required" }
    }
    $prohibited = Get-ChildItem -LiteralPath $source -Recurse -File -Force | Where-Object {
        $_.Extension -in '.dll', '.exe', '.lib', '.obj', '.pdb', '.ilk', '.exp' -or
        $_.FullName -match '[\\/](\.git|\.xmake|build)[\\/]'
    }
    if ($prohibited) { throw 'Generated/private files found in source staging.' }
    [IO.File]::WriteAllLines((Join-Path $source 'SOURCE-REVISION.txt'), @("Body Change NG $version", "Git revision: $revision", 'Dependency pins: DEPENDENCIES.md and xmake-requires.lock'), [Text.UTF8Encoding]::new($false))
    $binaryZip = Join-Path $output "Body-Change-NG-v$version.zip"
    $sourceZip = Join-Path $output "Body-Change-NG-v$version-Source.zip"
    New-VerifiedArchive $binary $binaryZip
    New-VerifiedArchive $source $sourceZip
    $sums = foreach ($path in @($binaryZip, $sourceZip)) {
        '{0}  {1}' -f (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash, [IO.Path]::GetFileName($path)
    }
    [IO.File]::WriteAllLines((Join-Path $output "SHA256SUMS-v$version.txt"), $sums, [Text.UTF8Encoding]::new($false))
    Get-Item -LiteralPath $binaryZip, $sourceZip | Select-Object Name, Length
    $sums
} finally {
    $resolved = (Resolve-Path -LiteralPath $stage).Path
    if (-not $resolved.StartsWith($tempRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Leaf $resolved) -notmatch '^BCNG-release-[0-9a-f]{32}$') {
        throw 'Unsafe staging cleanup target.'
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
