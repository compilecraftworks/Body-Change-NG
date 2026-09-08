# Body Change NG 1.2.0 release notes

Version 1.2.0 is the separately maintained native-skin architecture. BodySlide
shape changes still use RaceMenu BodyMorph, while normal body skin now uses a
private clone of the actor's current native TXST → ArmorAddon → Skin Armor
graph attached at the NPC ActorBase.

## Upgrade precautions

- Do not combine the 1.1.4 and 1.2.0 DLLs.
- Preserve personal `BodySkin`, `Futanari`, `TintMask`, and BodySlide preset
  folders if they are stored inside the main mod.
- Preserve `SKSE\Plugins\BodyChangeNGdistribution.json` before replacing the
  mod. Settings and distribution JSON remain supported.
- Per-skin `profile.json` is no longer supported or read. Skin identity is the
  existing relative-folder automatic ID. An explicit `!UBE` texture namespace
  is UBE; every conventional humanoid texture tree is Legacy.
- Keep the previous DLL backup until the intended saves have been exercised in
  game with body selection, skin selection, outfit changes, save/reload, and
  NPC distribution.

## Compatibility scope

- Skyrim SE 1.5.97 and verified Skyrim AE 1.6.x layouts through 1.6.1179
- Matching SKSE64, Address Library, and RaceMenu
- CBBE 3BA, BHUNP/UNP, UBE 2.0, HIMBO, SAM, and vanilla humanoid bodies
- Argonian and Khajiit skin race boundaries
- RSV provider rebasing and bounded face reconciliation
- Independent SOS/TNG/TRX/ERF external-genital skin adapters
- No Skyrim VR build

## Verification

- Release plugin build passed.
- All 15 automated regression executables passed.
- OBody distribution and OBody NG ORefit parsing were audited against all six installed
  TAKEALOOK/TuLED JSON providers. Every parsed outfit-name/plugin exclusion and
  force-refit entry was exercised through the same decision policy used by the
  plugin; registration now re-evaluates every loaded actor immediately.
- The real TAKEALOOK asset tree produced 11 BodySkin rows: 7 Legacy and 4 UBE.
- That scan mapped 139 skin DDS files, left 23 unrelated DDS files unassigned,
  and retained 85 tint entries plus 10 futanari entries.
- The installer archive contains only the approved MO2 payload and no
  skin-pack `profile.json`.

## Artifacts

- `Body Change NG v1.2.0.zip` — MO2-ready installer
- `Body Change NG v1.2.0 Source.zip` — corresponding buildable source,
  pinned dependency closure, scripts, licenses, and release documentation
