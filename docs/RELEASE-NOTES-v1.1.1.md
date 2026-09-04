# Body Change NG v1.1.1

Responsiveness, update scheduling, and load-safety fixes — 2026-09-05.

### Direct selection and skin updates

- Gives direct body, skin, and tint selections a reserved processing opportunity within the existing frame budget, while preserving service for automatic NPC distribution.
- Promotes the selected actor's required follow-up work without reordering that actor's preview, confirmation, or skin updates.
- Releases completed skin-callback ownership when the callback finishes, rather than waiting for the VM to destroy its callback object. Unfinished native calls remain protected.
- Keeps the latest queued selection and skips superseded skin-query follow-ups. Adds queued, applying, and delayed status in the existing UI help line.

### Load safety, previews, and outfit correction

- Schedules automatic distribution, equipment changes, and rebuild recovery across actual engine update boundaries instead of repeatedly draining the same task queue.
- Prevents overlapping BCNG updates on the same actor. Cancels stale session work during loading and bounds retries for actors whose 3D is unavailable.
- Prevents another NPC's automatic distribution from cancelling the selected actor's body preview. Loading a save no longer confirms pending choices from the previous UI session.
- Coordinates RaceMenu-close recovery with outstanding actor work. Default body/skin requests no longer fall back to an older selected result while removal is pending.
- Refreshes outfit correction after body reapplication and calculates procedural correction against the combined morph result without deleting other mods' morph keys.

### Refresh and processing cost

- Detects changed XML/DDS content under the same preset or pack ID. Refreshing changed DDS content uses a new texture-cache identity so the previous cached file is not reused.
- Caches content signatures and read-only rule snapshots; avoids repeated full-catalog copies and unnecessary force-outfit inventory searches.
- Keeps full DDS reads at initial catalog loading or explicit refresh, not at each NPC event. Expensive post-apply texture audits remain debug-only.
- Preserves existing rules, settings, co-save identifiers, starter exclusions, body-family filtering, skin part/channel routing, and camera values.

### Validation

Release build and all 12 regression test executables passed. Automated tests cover queue ordering, callback lifetime/cancellation, latest-selection handling, catalog refresh, and existing routing/state rules. This release is not a confirmed fix for the reported OverlayFix CTD and does not certify stutter-free gameplay or in-game appearance.

## Updating from 1.1.0

- Close Skyrim before replacing the DLL.
- Keep your existing `Data\SKSE\Plugins\BodyChangeNGdistribution.json`, settings, and personal BodySkin, TintMask, and SliderPresets content. Back up any active MO2 Overwrite/profile copies; do not replace personal rules with the bundled starter JSON.
- No new save is required by this update; JSON schema and co-save identifiers are unchanged. Content signatures may cause older saved results to be reevaluated/reapplied once.
- Full DDS content checks run during initial catalog loading or explicit Refresh. Large packs can make those scans take longer; they are not repeated for each NPC.
- A delayed status does not mean a failed application. BCNG does not force a new update over an unfinished native call.

## Downloads

- `Body-Change-NG-v1.1.1.zip` — MO2 installation: DLL, starter JSON, three folder guides, and two required license documents only.
- `Body-Change-NG-v1.1.1-Source.zip` — matching project source, build scripts, pinned build-required dependency sources, and licenses.
- `SHA256SUMS-v1.1.1.txt` — archive checksums.

[한국어 변경 이력 및 업데이트 안내](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.1.1/docs/RELEASE-NOTES-v1.1.1-KO.md)
