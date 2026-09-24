#requires -Version 7.0
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$helper = Join-Path $repo 'package\Tools\BodyChangeNG-RuleId.ps1'
function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function MustFail([scriptblock]$Action, [string]$Message) {
    $failed = $false
    try { & $Action | Out-Null } catch { $failed = $true }
    Require $failed $Message
}
$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\', '/')
$stage = Join-Path $tempRoot ('BCNG-rule-authoring-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
try {
    $presets = Join-Path $stage 'SliderPresets'
    $nested = Join-Path $presets 'My Pack'
    New-Item -ItemType Directory -Path $nested | Out-Null
    $xml = Join-Path $nested 'My Presets.xml'
    $fixture = '<SliderPresets><Preset name="Preset A"/><Preset name="Body &amp; Shape"/><Preset name="Preset A"/><Preset name="Outfit-Refit"/><Preset/></SliderPresets>'
    [IO.File]::WriteAllText($xml, $fixture, [Text.UTF8Encoding]::new($false))
    $before = (Get-FileHash -LiteralPath $xml -Algorithm SHA256).Hash
    $rows = @(& $helper -PresetXml $xml | ConvertFrom-Json)
    Require ($rows.Count -eq 2) 'Preset deduplication/refit filtering failed.'
    Require ($rows[0].id -ceq ('My Pack/My Presets.xml' + [char]31 + 'Preset A')) 'Preset relative path/separator mismatch.'
    Require ($rows[1].name -ceq 'Body & Shape') 'XML entity decoding failed.'
    Require ((Get-FileHash -LiteralPath $xml -Algorithm SHA256).Hash -eq $before) 'Helper changed XML.'
    $customRootRows = @(& $helper -PresetXml $xml -PresetRoot $nested | ConvertFrom-Json)
    Require ($customRootRows[0].source -ceq 'My Presets.xml') 'Explicit preset root ignored.'
    MustFail { & $helper -PresetXml $xml -PresetRoot (Join-Path $stage 'missing') } 'Missing root accepted.'
    $sibling = Join-Path $stage 'Sibling'
    New-Item -ItemType Directory -Path $sibling | Out-Null
    MustFail { & $helper -PresetXml $xml -PresetRoot $sibling } 'Out-of-root XML accepted.'
    $dtd = Join-Path $presets 'DTD.xml'
    [IO.File]::WriteAllText($dtd, '<!DOCTYPE SliderPresets [<!ENTITY test "Preset">]><SliderPresets><Preset name="&test;"/></SliderPresets>')
    MustFail { & $helper -PresetXml $dtd } 'DTD accepted.'
    $broken = Join-Path $presets 'Broken.xml'
    [IO.File]::WriteAllText($broken, '<SliderPresets><Preset')
    MustFail { & $helper -PresetXml $broken } 'Malformed XML accepted.'
    $empty = Join-Path $presets 'Empty.xml'
    [IO.File]::WriteAllText($empty, '<SliderPresets/>')
    MustFail { & $helper -PresetXml $empty } 'Empty catalog accepted.'
    $overlay = & $helper -OverlayArea body -OverlayTexture 'Actors/Character/Overlays/My Paint.dds' | ConvertFrom-Json
    Require ($overlay.id -ceq 'overlay-body-9e9392d11baf62f0') 'Overlay hash differs from runtime algorithm.'
    $normalized = & $helper -OverlayArea BODY -OverlayTexture '  .\.\Actors\Character\Overlays\My Paint.dds  ' | ConvertFrom-Json
    Require ($normalized.id -ceq $overlay.id) 'Overlay case/whitespace/separator normalization failed.'
    $prefixed = & $helper -OverlayArea body -OverlayTexture 'Textures/Actors/Character/Overlays/My Paint.dds' | ConvertFrom-Json
    Require ($prefixed.id -cne $overlay.id) 'Registration prefix was incorrectly removed.'
    $hands = & $helper -OverlayArea hands -OverlayTexture 'Actors/Character/Overlays/My Paint.dds' | ConvertFrom-Json
    Require ($hands.id -cne $overlay.id) 'Area did not affect overlay identity.'
    MustFail { & $helper -OverlayArea body -OverlayTexture '   ' } 'Empty overlay texture accepted.'
    $template = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $repo 'package\SKSE\Plugins\BodyChangeNGdistribution.json')
    $starter = $template | ConvertFrom-Json
    Require ($starter.rules.Count -eq 0 -and $starter.schemaVersion -eq 8) 'Starter contains active example rules.'
    $exampleMatch = [regex]::Match($template, '(?ms)^/\* EXAMPLE: individual_body\r?\n(.*?)^END EXAMPLE \*/')
    Require $exampleMatch.Success 'First commented example missing.'
    $example = $exampleMatch.Groups[1].Value | ConvertFrom-Json
    Require ($example.presets[0] -ceq 'Preset A') 'JSON example must use the displayed preset name.'
    Write-Output 'Distribution authoring passed: preset IDs/entities/refit filtering, read-only behavior, path boundaries, DTD/malformed XML rejection, overlay hash/normalization/area, inert JSON examples.'
} finally {
    $resolved = (Resolve-Path -LiteralPath $stage).Path
    if (-not $resolved.StartsWith($tempRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Leaf $resolved) -notmatch '^BCNG-rule-authoring-[0-9a-f]{32}$') {
        throw 'Unsafe test-fixture cleanup target.'
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
