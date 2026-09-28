# UBE SE morph baseline and actor-weight audit — 2026-09-28

## Confirmed scope

Skyrim SE 1.5.97, UBE 2.0 SE's bundled `skee64.dll`:

- File size: 2,007,552 bytes.
- SHA-256: `283ea6f0df6234b5636d6b03445a57c90369514e61ec3b02da07f731fcf3469b`.
- The active DLL's code and BodyMorph v4 vtable were checked against this file.
- Observations used `OpenProcess(QUERY_INFORMATION | VM_READ)` and
  `ReadProcessMemory` only. No game function calls, injected code, remote writes,
  suspended threads or save modifications were used for diagnosis.

## Confirmed baseline defect

The bundled DLL's ExtraData qsort comparator at RVA `0xF2390` subtracts only the
low 32 bits of interned name pointers. Its subsequent binary lookup compares
complete 64-bit pointers. These orderings can disagree.

The affected Studded armor `BaseShape` contained `LOCKEDNORM` followed by an
existing `SHAPEDATA`. Their observed name pointers were `0x2008B099888` and
`0x1F9CBAABB68`. Full-width comparison and the truncated subtraction have opposite
signs. Lookup missed the existing pristine baseline and a second `SHAPEDATA`
was created from already-morphed vertices.

For the 28,624-vertex body shape, the pristine baseline matched the winning disk
NIF to a maximum coordinate-distance error of approximately `0.00000763`. The
current body differed from one application on the pristine baseline by up to
`5.452114`. It instead matched application on the second, already-morphed
baseline within `0.0000474`. Three successive reads showed unchanged morph
values and geometry. Other inspected armor components still matched a single
application. This is not evidence that every armor component or every body
family is affected.

The `skee64backports` comparison did not establish a direct fix for this
comparator. Allocation-dependent pointer ordering explains why the defect can
be intermittent; it does not prove which allocation changed when another DLL
was enabled.

## BCNG compatibility fix

`RaceMenuExtraDataGuard` installs once at SKSE PostPostLoad, before actor geometry
is loaded. It requires Skyrim 1.5.97, the complete file digest above, matching
image headers and unchanged live comparator/caller/lookup/version code.

Only the comparator's 14-byte entry is replaced with a tail jump to a full-width
unsigned pointer comparison. The original DLL on disk is not edited. The fix
does not replace morph arithmetic, parallel jobs, geometry updates, or search
code. Unknown DLLs and externally changed code remain untouched. Official SE
0.4.16 and AE 0.4.20 DLLs are not targets.

There are no retained actor/mesh pointers, additional per-frame scans, new
geometry caches or executable trampoline allocations. File/hash validation uses
temporary startup storage, released when installation returns. Already damaged
live geometry is not traversed or repaired in place; the new DLL requires a
game restart before it can prevent recurrence.

## Weight units: separate from the baseline defect

RaceMenu's displayed weight slider uses 0–1. This must not be confused with the
TESNPC weight read by `TESForm::GetWeight`, which uses 0–100. BCNG and
[OBody NG 4.4.3](https://github.com/Aietos/OBody-NG/blob/4.4.3/src/Body/Body.cpp)
both divide this engine value by 100 before interpolating the XML endpoints.

The running 1.5.97 function was read at Address Library ID 14809, RVA `0x1A1730`.
Its NPC switch branch resolves to `0x1A177F`, loads the float at `TESNPC + 0x1FC`
and returns it without scaling. On the current loaded UBE player this field was
100, so the interpolation fraction is 1. No engine function was remotely called.

At 23:50 KST, the committed `SSkwaii00 Body UBE - BodySlide Preset` had 137 of
154 authored sliders equal to the XML high endpoint within floating-point
tolerance. The 17 differences were sliders controlled by the enabled nipple
or genital randomization options, not a weight conversion error. Examples:

- `Big_SaggyBreasts`: XML high 39 → live morph approximately 0.39.
- `HipsOuterBonesFatty`: XML high 80 → live morph approximately 0.80.
- `BreastsBigger`: XML high 27 → live morph approximately 0.27.

The previous process had an actual field value of 1, and its non-randomized
values matched a 0.01 interpolation fraction. The reason this earlier field
differed from the current one was not established. The earlier explanation
equating a displayed RaceMenu value of 1 with 1% was incorrect.

`BodyMorphWeight.h` names the unit conversion explicitly and is shared by base
preset preview/commit and outfit correction for 3BA, BHUNP, UBE and Necoco. This
refactor does not change the calculation. In particular, it does not guess
that engine values at or below 1 are normalized: an actual 1% NPC must remain
1%, not become 100%.

## Verification

- Production Release DLL build succeeded with the existing pinned SE/AE-only
  configuration; no dependency upgrade or VR target was introduced.
- Exact captured pointer-ordering regression, boundary/equal pointers, 50,000
  random addresses and repeated baseline lookup passed.
- Private-image tests rejected wrong runtime, digest, truncated file, altered
  live code/header and official SE/AE DLLs. Exact-file installation changed only
  the comparator entry, restored page protection and was idempotent. The only
  executed entry was the new jump into BCNG's own comparator; original DLL code
  and DllMain were never executed by the test.
- OBody reference-parser parity: 2,400 XML cases.
- 3BA/BHUNP/UBE/Necoco engine-weight interpolation: 12,600 cases, including
  0, 0.5, 1, 25, 50, 73 and 100 engine-percent inputs.
- Existing numeric/refit/randomization parity: 6,224 cases; body-morph key
  isolation/restoration: 8,929 checks; existing UBE interpolation: 7,200 cases.
- After the user closed Skyrim, the Release DLL was deployed on September 29
  to `D:\TuLED\File Mod Skyrim SE\mods\Body Change NG\SKSE\Plugins\BodyChangeNG.dll`.
  Source and installed SHA-256 both equal
  `202AE96735F0A17D7F7BE6801DC37C3CA1DDE0858107BA62670582EC3BF4D9A6`.
  The previous DLL was backed up under ignored
  `build/deployment-backups/TuLED-20260929-extra-data-guard/` before replacement.
  Settings, saves, RaceMenu/UBE DLLs and other MO2 profiles were not modified.
  In-game verification of this new guard remains pending.

Read-only diagnostic snapshots and disassembly remain under ignored
`build/analysis/`; they are not release assets.

## Official SE RaceMenu plus backports — September 29 live comparison

The user selected UBE's DLL-free installation option and enabled
`skee64backports`, retaining the BCNG build above. At 00:24 KST, process 8860
actually loaded official RaceMenu 0.4.16 from TuLED's `Race Menu` mod:
SHA-256 `255c0db0ba5ff14640cc6d75ccddfb24474b498d35b77f3140ccb05eb80e7273`.
The separate backports DLL was also loaded. This is still Skyrim 1.5.97,
not an AE RaceMenu DLL running on SE.

- The current backports log explicitly recorded activation of its 1597 normal
  recalculation and subsequent recalculation jobs.
- BCNG logged that the ExtraData guard left this non-target DLL unchanged.
  Consequently the successful observation below does not validate the new
  UBE-DLL guard in-game or depend on that guard being active.
- The diagnostic reader used an explicit official-0.4.16 layout, independently
  verified from its constructor, morph setter/getter and INI reader. Loaded
  module path, file digest, relevant live code and vtable were checked before
  reading the actor dictionary. Unknown layouts are rejected.
- Two snapshots had identical morph values and captured geometry, stable roots,
  and no structural read errors. Actor engine weight was 100.
- The worn Studded armor's 28,624-vertex `BaseShape` had one `SHAPEDATA` baseline,
  exactly matching the winning weight-100 NIF vertices. It matched one morph
  application within a maximum coordinate-distance error of `0.0000496222`.
  All 13 matched body/armor shapes had one pristine baseline and a maximum
  one-application error below `0.00005`. Four other captured geometry objects
  were not included in this disk-shape correspondence test.
- For `SSkwaii00 Body UBE - BodySlide Preset`, 136 of 154 authored sliders matched
  the XML high endpoint within `0.00001`; all 18 differences were nipple/genital
  sliders handled by the enabled randomization options. All 14 UBE outfit
  correction entries matched the implemented weight-100 correction table.

This supports the official SE DLL plus backports as a working alternative for
the observed UBE character, preset and armor state. It is not a universal
compatibility test, nor a controlled test proving that adding backports to the
faulty UBE DLL corrects its ordering defect. This observation made no game
memory writes, remote calls, save changes or MO2 changes.

Evidence: `build/analysis/player-live-20260929-official-backports-{1,2}.json`,
the first snapshot's `.comparison.json`, and
`build/analysis/official-backports-20260929-values.json`.
