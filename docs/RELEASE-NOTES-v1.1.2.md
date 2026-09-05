# Body Change NG v1.1.2

Black/gray interface refresh, verified CBBE 3BA anatomy safety, and corrected UBE BodySkin routing — 2026-09-06.

### Interface theme

- Replaces the blue-tinted window, child-panel, popup, title, idle button, input, card, table, scrollbar, resize-grip, and navigation surfaces with a consistent black, charcoal, and grayscale foundation.
- Preserves the established blue selection, hover, and pressed states plus warning, success, progress, favorite, incompatibility, and live tint-preview colors that convey state or content.
- Shortens the nearby-actor refresh button to `Refresh actors`, with matching Korean and Simplified Chinese labels; its loaded-actor refresh behavior is unchanged.
- Preserves UI layout, scaling, keyboard/gamepad input, catalog behavior, camera behavior, rules, settings, JSON schemas, co-save identifiers, and body/skin/tint application paths.

### Direct selection and skin updates

- Gives direct body, skin, and tint selections a reserved processing opportunity within the existing frame budget, while preserving service for automatic NPC distribution.
- Promotes the selected actor's required follow-up work without reordering that actor's preview, confirmation, or skin updates.
- Releases completed skin-callback ownership when the callback finishes, rather than waiting for the VM to destroy its callback object. Unfinished native calls remain protected.
- Cancels a RaceMenu SE callback batch that never returns after a bounded timeout. Late callbacks remain invalid, and the actor queue resumes only after its normal quiet boundary.
- Keeps the latest queued selection and skips superseded skin-query follow-ups. Adds queued, applying, and delayed status in the existing UI help line.

### Load safety, previews, and outfit correction

- Schedules automatic distribution, equipment changes, and rebuild recovery across actual engine update boundaries instead of repeatedly draining the same task queue.
- Prevents overlapping BCNG updates on the same actor. Cancels stale session work during loading and bounds retries for actors whose 3D is unavailable.
- Prevents another NPC's automatic distribution from cancelling the selected actor's body preview. Loading a save no longer confirms pending choices from the previous UI session.
- Coordinates RaceMenu-close recovery with outstanding actor work. Default body/skin requests no longer fall back to an older selected result while removal is pending.
- Refreshes outfit correction after body reapplication and calculates procedural correction against the combined morph result without deleting other mods' morph keys.
- Limits clothed breast/nipple correction to verified CBBE 3BA actors. UBE uses materially different slider names and non-zero body defaults, so UBE and ambiguous female families are skipped rather than receiving guessed anatomy values or imported/named ORefit layers.
- Limits stable nipple/genital randomization to CBBE 3BA NPCs. The controls are explicitly named `Randomize NPC nipple shape` and `Randomize NPC genital shape` in English, with equivalent Korean and Simplified Chinese labels.
- Shows an UBE warning and tooltips for the selected actor. Global switches remain usable so mixed installations can skip an UBE player while still correcting or randomizing verified CBBE 3BA NPCs.
- Cross-checked this boundary against TAKEALOOK's installed UBE 2.0 SliderSets and OBody NG's CBBE-oriented ORefit/randomization implementation. A previously owned unsupported outfit layer is cleared once and later equipment events reuse a cached no-op signature.

### Refresh and processing cost

- Detects changed XML/DDS content under the same preset or pack ID. Refreshing changed DDS content uses a new texture-cache identity so the previous cached file is not reused.
- Caches content signatures and read-only rule snapshots; avoids repeated full-catalog copies and unnecessary force-outfit inventory searches.
- Reads only the required boolean options on automatic body and outfit paths instead of copying settings with all favorite lists. Equipment events create no BCNG work for actors using neither BodySkin nor outfit correction.
- Routes body, hand, and foot geometry separately even when one multi-slot Skin Armor clone contains all three. Partial skin packs replace only the supplied parts and channels while preserving the actor's current textures for everything absent.
- Stores the durable body, hand, and foot state as separate one-bit RaceMenu skin-slot keys. Exact ArmorAddon skin nodes remain a complementary route for outfits, so clothed and naked CBBE-family/UBE limbs cannot be skipped merely because the ordinary Biped target is absent.
- Prepares only the selected actor's effective skin texture aliases on one background worker before entering the geometry-application continuation. Direct selections are prioritized over bulk distribution and cached aliases are reused.
- Keeps full DDS reads at initial catalog loading or explicit refresh, not at each NPC event. Expensive post-apply texture audits remain debug-only.
- Adds bounded Racial Skin Variance compatibility: BCNG targets the actor's live RSV Skin Armor, keeps RSV's serialized FaceGen ownership intact, paints the selected BCNG face immediately, and coalesces one face-only reconciliation after RSV's deferred node update. Body Preset and TintMask-only use remains independent and adds no RSV polling or file scans.
- Improves compatibility with OverlayFix by tightening actor-update and asynchronous-work boundaries.
- Deduplicates full DDS hashing when MO2 exposes the same backing file through both its physical mod directory and virtual Data path. Per-file catalog diagnostics for large packs are debug-only.
- Preserves existing rules, settings, co-save identifiers, starter exclusions, body-family filtering, skin part/channel routing, and camera values.

### Catalog and release guidance

- Updates both live geometries when an UBE outfit splits visible body skin across slots 32 and 53. The UBE Body atlas is applied consistently to UBE body, hand, and foot surfaces and the same routing is used for verification and recovery.
- Default Skin restoration now detects BCNG textures left on loaded body, hand, or foot clones after RaceMenu keys are removed. Only the mismatched actor is rebuilt once; no polling or all-NPC scan was added.
- Actors explicitly returned to Default Skin remain tracked for equipment reconciliation. Only a later equipment change on that actor is checked, allowing legacy outfit-specific BCNG keys to be removed without adding background scans.
- The main Body Skins list shows only the selected actor's actual body family. NPC-rule skin and body pools show only the female/male NPC body type selected in Mod Settings, matching runtime distribution filtering.
- Removes the per-rule body-family dropdown and displays the active Mod Settings family as text. Combat Style is not offered for new rules, while detailed race, faction, plugin, keyword, and class dropdowns open downward only. Legacy Combat Style JSON remains readable for schema compatibility.
- Restores the full `Body Presets`, `Body Skins`, and `Tint Masks` tab names and keeps the player Tint Masks tab visible so its catalog and installation guidance remain accessible.
- Adds Korean, English, and Simplified Chinese empty-list messages with exact BodySlide, BodySkin, UBE Body/Head, and TintMask placement paths plus the Refresh action.
- States explicitly that tint-mask-based facial tints are supported while overlay-based tints are not.
- Uses English names for the eight bundled starter rules and runtime-generated defaults. Existing user-authored rule names remain unchanged.

### Validation

Release build and all 12 regression test executables passed. Automated tests cover queue ordering, callback lifetime/cancellation, latest-selection handling, catalog refresh, MO2 path-alias deduplication, and existing routing/state rules.

## Updating from 1.1.1

- Close Skyrim before replacing the DLL.
- Keep your existing `Data\SKSE\Plugins\BodyChangeNGdistribution.json`, settings, and personal BodySkin, TintMask, and SliderPresets content. Back up any active MO2 Overwrite/profile copies; do not replace personal rules with the bundled starter JSON.
- No new save is required by this update; JSON schema and co-save identifiers are unchanged. Content signatures may cause older saved results to be reevaluated/reapplied once.
- If a private/test build previously wrote experimental UBE anatomy values, reapply the current body preset once after installing this release. BCNG rebuilds only its own committed morph key; it does not globally clear morph keys owned by other mods.
- Full DDS content checks run during initial catalog loading or explicit Refresh. Large packs can make those scans take longer; they are not repeated for each NPC.
- A delayed status does not mean a failed application. BCNG does not force a new update over an unfinished native call.

## Downloads

- `Body-Change-NG-v1.1.2.zip` — MO2 installation: DLL, starter JSON, three folder guides, and two required license documents only.
- `Body-Change-NG-v1.1.2-Source.zip` — matching project source, build scripts, pinned build-required dependency sources, and licenses.
- `SHA256SUMS-v1.1.2.txt` — archive checksums.

[한국어 변경 이력 및 업데이트 안내](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.1.2/docs/RELEASE-NOTES-v1.1.2-KO.md)
