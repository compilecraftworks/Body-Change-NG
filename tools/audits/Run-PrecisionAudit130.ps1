# The original defect probe was promoted to a normal-outcome regression test.
# This standalone runner compiles the SAME fixture used by xmake.
param([switch]$AddressSanitizer)
$ErrorActionPreference = 'Stop'
$auditRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$auditBuild = Join-Path $auditRoot 'build/audits/precision-v1.3.0'
New-Item -ItemType Directory -Force -Path $auditBuild | Out-Null
function Export-FunctionBody([string]$source, [string]$signature, [string]$file) {
    $text = Get-Content -LiteralPath (Join-Path $auditRoot $source) -Raw
    $start = $text.IndexOf($signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Function not found: $signature" }
    $body = [regex]::Match($text.Substring($start), '\r?\n\s*\{')
    if (!$body.Success) { throw "Body not found: $signature" }
    $open = $text.IndexOf('{', $start + $body.Index)
    $depth = 1; $end = $open + 1
    while ($depth -gt 0 -and $end -lt $text.Length) {
        if ($text[$end] -eq '{') { ++$depth }
        if ($text[$end] -eq '}') { --$depth }
        ++$end
    }
    if ($depth -ne 0) { throw "Unbalanced function: $signature" }
    [IO.File]::WriteAllText((Join-Path $auditBuild $file), $text.Substring($start, $end - $start))
}
Export-FunctionBody 'src/BodyChangeNG/FrameTasks.cpp' 'void Pump(std::uint64_t epoch)' 'frame_pump.inc'
Export-FunctionBody 'src/BodyChangeNG/RaceMenuBodyMorph.cpp' 'void QueueActorTask(' 'body_queue_actor.inc'
Export-FunctionBody 'src/BodyChangeNG/RaceMenuBodyMorph.cpp' 'void ClearPreviewNow(' 'body_clear_preview.inc'
Export-FunctionBody 'src/BodyChangeNG/RaceMenuBodyMorph.cpp' 'void QueueCancelPreview()' 'body_cancel_preview.inc'
Export-FunctionBody 'src/BodyChangeNG/RaceMenuBodyMorph.cpp' 'void QueueClearInactivePreview(' 'body_clear_inactive.inc'
Export-FunctionBody 'src/BodyChangeNG/RaceMenuBodyMorph.cpp' 'void QueueClearBodyChangeMorphs(' 'body_clear_default.inc'
Export-FunctionBody 'src/BodyChangeNG/RaceMenuBodyMorph.cpp' 'void QueueReapplyCurrent(RE::Actor* actor)' 'body_reapply.inc'
Export-FunctionBody 'src/BodyChangeNG/RaceMenuOverlay.cpp' 'void QueueCancelPreviews(RE::Actor* actor)' 'overlay_cancel.inc'
Export-FunctionBody 'src/BodyChangeNG/RaceMenuOverlay.cpp' 'void QueueReapplySaved(RE::Actor* actor)' 'overlay_reapply.inc'
Export-FunctionBody 'src/BodyChangeNG/ActorEvents.cpp' 'void ReapplyPlayerSelectionsAfterRaceMenu(' 'player_restore.inc'
Export-FunctionBody 'src/BodyChangeNG/OutfitRefit.cpp' 'void OutfitRefit::ProcessActor(' 'outfit_process.inc'
$auditName = if ($AddressSanitizer) { 'AppearanceLifecycle-asan' } else { 'AppearanceLifecycle' }
$compilerOptions = @('/nologo', '/EHsc', '/std:c++latest', '/utf-8', '/O2')
if ($AddressSanitizer) { $compilerOptions += @('/fsanitize=address', '/Zi', "/Fd$auditBuild/$auditName.pdb") }
& cl.exe @compilerOptions /I (Join-Path $auditRoot 'src') /I $auditBuild "/Fo$auditBuild/$auditName.obj" "/Fe$auditBuild/$auditName.exe" (Join-Path $auditRoot 'tests/AppearanceLifecycleTests.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Regression compilation failed' }
& (Join-Path $auditBuild "$auditName.exe")
if ($LASTEXITCODE -ne 0) { throw 'Appearance lifecycle regression failed' }
