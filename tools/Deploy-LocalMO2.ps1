#requires -Version 7.0
[CmdletBinding()]
param([switch]$Apply)
$ErrorActionPreference = 'Stop'
$bcngRepo = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$bcngDll = Join-Path $bcngRepo 'build\v1.2.1\windows\x64\release\BodyChangeNG.dll'
$bcngExpectedHash = '61379E22BBA9F341BC5C827C816167FCBD44A90E4CA78B848B2DF89B107733B9'
$bcngArchive = Join-Path $bcngRepo 'build\mo2-backup-v1.2.1-npc-filter-20260913'
$bcngInstalls = @(
    @{ Name = 'TuLED'; Root = 'D:\TuLED13E\File Mod Skyrim SE\mods\Body Change NG' },
    @{ Name = 'TAKEALOOK'; Root = 'C:\TAKEALOOK\mods\Body Change NG' }
)
function Get-BCNGHash([string]$Path) {
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}
function Assert-BCNGChild([string]$Path, [string]$Root) {
    $full = [IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith([IO.Path]::GetFullPath($Root).TrimEnd('\') + '\',
        [StringComparison]::OrdinalIgnoreCase)) { throw "Out-of-scope target: $full" }
    $ancestor = $full
    while ($ancestor.Length -ge $Root.Length) {
        if (Test-Path -LiteralPath $ancestor) {
            if ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Refusing a reparse-point target: $ancestor"
            }
        }
        $ancestor = Split-Path -Parent $ancestor
    }
    $full
}
if (Get-Process -Name SkyrimSE,skse64_loader -ErrorAction SilentlyContinue) {
    throw 'Exit Skyrim/SKSE before deployment.'
}
if ((Get-BCNGHash $bcngDll) -ne $bcngExpectedHash) { throw 'Build hash changed; review before deploying.' }
if ((Get-Item -LiteralPath $bcngDll).VersionInfo.FileVersion -ne '1.2.1.0') { throw 'Wrong DLL version.' }

$bcngCopies = @(
    @{ Source = $bcngDll; Relative = 'SKSE\Plugins\BodyChangeNG.dll' },
    @{ Source = (Join-Path $bcngRepo 'package\BodySkin\README.txt'); Relative = 'BodySkin\README.txt' },
    @{ Source = (Join-Path $bcngRepo 'package\Futanari\README.txt'); Relative = 'Futanari\README.txt' },
    @{ Source = (Join-Path $bcngRepo 'package\CalienteTools\BodySlide\SliderPresets\README.txt'); Relative = 'CalienteTools\BodySlide\SliderPresets\README.txt' },
    @{ Source = (Join-Path $bcngRepo 'LICENSE'); Relative = 'LICENSE' },
    @{ Source = (Join-Path $bcngRepo 'THIRD_PARTY_NOTICES.md'); Relative = 'THIRD_PARTY_NOTICES.md' }
)
foreach ($install in $bcngInstalls) {
    if (Get-Process -Name SkyrimSE,skse64_loader -ErrorAction SilentlyContinue) {
        throw 'Game started; stop deployment.'
    }
    $root = (Resolve-Path -LiteralPath $install.Root).Path
    if ($root -ne $install.Root) { throw "Unexpected resolved mod root: $root" }
    if ((Get-Item -LiteralPath $root).Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw "Refusing a reparse-point mod root: $root"
    }
    $archiveRoot = Assert-BCNGChild (Join-Path $bcngArchive $install.Name) $bcngRepo
    $plugins = Join-Path $root 'SKSE\Plugins'
    $oldFiles = @(Get-ChildItem -LiteralPath $plugins -File | Where-Object {
        $_.Name -like 'BodyChangeNG*.dll.bak' -or $_.Name -like 'BodyChangeNG.dll.*.bak'
    })
    $oldBackupRoot = Join-Path $root 'DLL Backups'
    if (Test-Path -LiteralPath $oldBackupRoot) {
        [void](Assert-BCNGChild $oldBackupRoot $root)
        $backupFiles = @(Get-ChildItem -LiteralPath $oldBackupRoot -Recurse -File)
        if ($backupFiles | Where-Object { $_.Name -notlike 'BodyChangeNG*.dll.disabled' }) {
            throw 'Unexpected file in DLL Backups; do not move unrelated files.'
        }
        $oldFiles += $backupFiles
    }
    # Superseded release prose is kept outside the active mod, not discarded.
    foreach ($name in @('README.md', 'CHANGELOG.md', 'CHANGELOG-KO.md')) {
        $oldPath = Join-Path $root $name
        if (Test-Path -LiteralPath $oldPath) { $oldFiles += Get-Item -LiteralPath $oldPath }
    }
    # These old separate notices are consolidated in THIRD_PARTY_NOTICES.md.
    foreach ($name in @('CommonLibSSE-NG-COPYING.txt', 'Dear-ImGui-LICENSE.txt',
        'OpenVR-LICENSE.txt', 'pugixml-LICENSE.md')) {
        $oldPath = Join-Path $root ('licenses\' + $name)
        if (Test-Path -LiteralPath $oldPath) { $oldFiles += Get-Item -LiteralPath $oldPath }
    }
    $metaSource = Join-Path $bcngRepo ('build\v1.2.1\mo2-deploy-stage\' + $install.Name + '-meta.ini')
    $copyItems = $bcngCopies + @{ Source = $metaSource; Relative = 'meta.ini' }
    $protected = @{}
    foreach ($json in Get-ChildItem -LiteralPath $plugins -Recurse -File -Filter '*.json') {
        $protected[$json.FullName] = Get-BCNGHash $json.FullName
    }
    foreach ($copy in $copyItems) {
        if (-not (Test-Path -LiteralPath $copy.Source -PathType Leaf)) { throw "Missing source: $($copy.Source)" }
        $destination = Assert-BCNGChild (Join-Path $root $copy.Relative) $root
        Write-Output "COPY $($copy.Source) -> $destination"
        if ($Apply) {
            if (Test-Path -LiteralPath $destination) {
                $backup = Assert-BCNGChild (Join-Path $archiveRoot ('replaced\' + $copy.Relative)) $bcngRepo
                if (Test-Path -LiteralPath $backup) { throw "Existing backup; refusing to overwrite: $backup" }
                New-Item -ItemType Directory -Path (Split-Path -Parent $backup) -Force | Out-Null
                Copy-Item -LiteralPath $destination -Destination $backup
            }
            New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
            Copy-Item -LiteralPath $copy.Source -Destination $destination -Force
            if ((Get-BCNGHash $destination) -ne (Get-BCNGHash $copy.Source)) { throw "Copy mismatch: $destination" }
        }
    }
    foreach ($old in $oldFiles) {
        $oldPath = Assert-BCNGChild $old.FullName $root
        $relative = [IO.Path]::GetRelativePath($root, $oldPath)
        $backup = Assert-BCNGChild (Join-Path $archiveRoot ('removed-from-mod\' + $relative)) $bcngRepo
        Write-Output "ARCHIVE $oldPath -> $backup"
        if ($Apply) {
            if (Test-Path -LiteralPath $backup) { throw "Existing archive target: $backup" }
            $oldHash = Get-BCNGHash $oldPath
            New-Item -ItemType Directory -Path (Split-Path -Parent $backup) -Force | Out-Null
            Move-Item -LiteralPath $oldPath -Destination $backup
            if ((Get-BCNGHash $backup) -ne $oldHash) { throw "Archive mismatch: $backup" }
        }
    }
    if ($Apply) {
        foreach ($path in $protected.Keys) {
            if ((Get-BCNGHash $path) -ne $protected[$path]) { throw "Protected JSON changed: $path" }
        }
        foreach ($directory in @('DLL Backups', 'licenses')) {
            $emptyDirectory = Assert-BCNGChild (Join-Path $root $directory) $root
            if ((Test-Path -LiteralPath $emptyDirectory -PathType Container) -and
                @(Get-ChildItem -LiteralPath $emptyDirectory -Force).Count -eq 0) {
                Remove-Item -LiteralPath $emptyDirectory
            }
        }
        Write-Output "VERIFIED $($install.Name): DLL $bcngExpectedHash; protected JSON unchanged; archived $($oldFiles.Count) files"
    }
}
# This script deliberately does not copy the empty starter over existing rules,
# remove texture caches, modify packs, edit saves, or change other MO2 mods.
