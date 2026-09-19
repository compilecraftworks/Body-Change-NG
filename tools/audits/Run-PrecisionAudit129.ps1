$ErrorActionPreference = 'Stop'
$auditRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$auditBuild = Join-Path $auditRoot 'build/audits/precision-v1.2.9'
New-Item -ItemType Directory -Force -Path $auditBuild | Out-Null
function Export-FunctionBody([string]$source, [string]$signature, [string]$file) {
    # This diagnostic intentionally reproduces the historical pre-fix source.
    # Current behavior is covered by AppearanceIntegrationTests in xmake.
    $lines = & git -C $auditRoot show "747de9b06bac5a7c74525cd5bf9a554cd932e6cd:$source"
    if ($LASTEXITCODE -ne 0) { throw "Historical source unavailable: $source" }
    $text = $lines -join "`n"
    $start = $text.IndexOf($signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Function not found: $signature" }
    # Start at the body, not default argument braces in ApplyNow's signature.
    $body = $text.IndexOf("`n    {", $start, [StringComparison]::Ordinal)
    if ($body -lt 0) { throw "Body not found: $signature" }
    $open = $text.IndexOf('{', $body)
    $depth = 1; $end = $open + 1
    while ($depth -gt 0 -and $end -lt $text.Length) {
        if ($text[$end] -eq '{') { ++$depth }
        if ($text[$end] -eq '}') { --$depth }
        ++$end
    }
    if ($depth -ne 0) { throw "Unbalanced function: $signature" }
    [IO.File]::WriteAllText((Join-Path $auditBuild $file), $text.Substring($start, $end - $start))
}
Export-FunctionBody 'src/BodyChangeNG/RaceMenuOverlay.cpp' '[[nodiscard]] bcn::overlay::ApplyResult ApplyNow(' 'overlay_apply.inc'
Export-FunctionBody 'src/BodyChangeNG/RaceMenuOverlay.cpp' 'void QueueReapplySaved(RE::Actor* actor)' 'overlay_reapply.inc'
Export-FunctionBody 'src/BodyChangeNG/FrameTasks.cpp' 'void Pump(std::uint64_t epoch)' 'frame_pump.inc'
Export-FunctionBody 'src/BodyChangeNG/RaceMenuBodyMorph.cpp' 'std::optional<bool> LiveBodyChangeStateMatches(' 'body_live.inc'
Export-FunctionBody 'src/BodyChangeNG/RaceMenuBodyMorph.cpp' 'void QueueVerifySavedBody(RE::Actor* actor)' 'body_verify.inc'
& cl.exe /nologo /EHsc /std:c++latest /utf-8 /O2 /I (Join-Path $auditRoot 'src') /I $auditBuild "/Fo$auditBuild/PrecisionAudit129.obj" "/Fe$auditBuild/PrecisionAudit129.exe" (Join-Path $PSScriptRoot 'PrecisionAudit129.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Diagnostic compilation failed' }
& (Join-Path $auditBuild 'PrecisionAudit129.exe')
if ($LASTEXITCODE -ne 0) { throw 'Diagnostic checks failed' }
