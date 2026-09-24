#requires -Version 5.1
<#
Read-only offline ID helper. It does not edit JSON, presets or game files.
Preset mode prints the exact IDs of ordinary Preset elements in one XML.
Overlay mode needs the exact texture string used by the installed registration.
An ID alone does not establish catalog discovery or actor compatibility.
#>
[CmdletBinding(DefaultParameterSetName = 'Preset')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Preset')]
    [string]$PresetXml,
    [Parameter(ParameterSetName = 'Preset')]
    [string]$PresetRoot,
    [Parameter(Mandatory, ParameterSetName = 'Overlay')]
    [ValidateSet('face', 'body', 'hands', 'feet')]
    [string]$OverlayArea,
    [Parameter(Mandatory, ParameterSetName = 'Overlay')]
    [string]$OverlayTexture
)
$ErrorActionPreference = 'Stop'
if ($PSCmdlet.ParameterSetName -eq 'Preset') {
    $xmlFile = Get-Item -LiteralPath $PresetXml
    if ($xmlFile.PSIsContainer -or $xmlFile.Extension -ine '.xml' -or
        $xmlFile.Length -eq 0 -or $xmlFile.Length -gt 8MB) {
        throw 'Use a non-empty BodySlide XML file of at most 8 MiB.'
    }
    if (-not $PresetRoot) {
        $candidate = $xmlFile.Directory
        while ($candidate -and $candidate.Name -ine 'SliderPresets') { $candidate = $candidate.Parent }
        if (-not $candidate) { throw 'No SliderPresets parent found. Specify -PresetRoot explicitly.' }
        $PresetRoot = $candidate.FullName
    }
    $root = Get-Item -LiteralPath $PresetRoot
    if (-not $root.PSIsContainer) { throw 'PresetRoot must be the directory corresponding to SliderPresets.' }
    $prefix = $root.FullName.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $xmlFile.FullName.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'The XML must be inside PresetRoot.'
    }
    $source = $xmlFile.FullName.Substring($prefix.Length).Replace('\', '/')
    $xmlSettings = [Xml.XmlReaderSettings]::new()
    $xmlSettings.DtdProcessing = [Xml.DtdProcessing]::Prohibit
    $xmlSettings.XmlResolver = $null
    $reader = [Xml.XmlReader]::Create($xmlFile.FullName, $xmlSettings)
    try {
        $document = [Xml.XmlDocument]::new()
        $document.XmlResolver = $null
        $document.Load($reader)
    } finally { $reader.Dispose() }
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $rows = @(foreach ($preset in $document.SelectNodes('/SliderPresets/Preset')) {
        $name = $preset.GetAttribute('name')
        if (-not $name -or $name.EndsWith('-Refit', [StringComparison]::Ordinal)) { continue }
        $id = $source + [char]31 + $name
        if ($seen.Add($id)) { [pscustomobject]@{ name = $name; source = $source; id = $id } }
    })
    if ($rows.Count -eq 0) { throw 'No ordinary named Preset elements found (names ending in -Refit are separate correction presets).' }
    ConvertTo-Json -InputObject $rows -Depth 4
} else {
    # Match the runtime's ASCII whitespace/case and separator normalization.
    # Do not remove a textures/ prefix: StableId hashes the registration string.
    $texture = $OverlayTexture.Trim([char[]]@(' ', "`t", "`r", "`n", "`v", "`f")).Replace('/', '\')
    $texture = -join @($texture.ToCharArray() | ForEach-Object {
        if ([int]$_ -ge 65 -and [int]$_ -le 90) { [char]([int]$_ + 32) } else { $_ }
    })
    while ($texture.StartsWith('.\', [StringComparison]::Ordinal)) { $texture = $texture.Substring(2) }
    if (-not $texture) { throw 'An exact registered overlay texture path is required.' }
    $area = $OverlayArea.ToLowerInvariant()
    Add-Type -AssemblyName System.Numerics
    $hash = [Numerics.BigInteger]::Parse('1469598103934665603')
    $prime = [Numerics.BigInteger]::Parse('1099511628211')
    $modulus = [Numerics.BigInteger]::Parse('18446744073709551616')
    foreach ($part in @($area, $texture)) {
        foreach ($octet in [Text.Encoding]::UTF8.GetBytes($part)) {
            $hash = (($hash -bxor [Numerics.BigInteger]$octet) * $prime) % $modulus
        }
        $hash = (($hash -bxor [Numerics.BigInteger]255) * $prime) % $modulus
    }
    $hex = ([uint64]$hash).ToString('x16', [Globalization.CultureInfo]::InvariantCulture)
    [pscustomobject]@{ area = $area; texture = $texture; id = "overlay-$area-$hex" } | ConvertTo-Json
}
