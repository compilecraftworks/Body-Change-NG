# Pinned build dependencies

Body Change NG v1.2.0 is built with xmake 3.1.0 and the exact dependency
closure below. `xmake-requires.lock` remains authoritative for xmake packages
and pins the xmake-repo commit `e36e822129b0fcbdfb51633a7fcee8c76af344bf`.

- CommonLibSSE-NG v6.7.1, commit
  `70c1acd5261210982bd52f6d4468a082fe04d798`
- OpenVR v1.0.15 headers, commit
  `60eb187801956ad277f1cae6680e3a410ee0873b`
- Dear ImGui 1.92.9b; vendored `imgui.cpp` SHA-256
  `01CD8AFB847FE33F6D3386ECA9FA057BB87783364BF3BD13B0FC582D1F3FB5FF`
- pugixml 1.16; vendored `pugixml.cpp` SHA-256
  `04CDC6BDE588039E7E3F2AF195A6CDAD33303B2DB39568067FA5CE55E0B723C9`
- nlohmann/json v3.12.0

## RaceMenu runtime interface compatibility

- BodyMorph v4/v5 and the paired Overlay/Override v1/v1 or v2/v2 contracts are
  selected using their reported interface versions, not a product-file version
  whitelist. Legacy v1 has a separate string/variant ABI adapter; public v2 uses
  its visitor wrappers. They are never cast to each other's layout.
- Unknown higher revisions use only the highest known compatible prefix (v5
  for BodyMorph, v2 for public Overlay/Override). This assumes the provider
  retains that prefix, and is not proof of future ABI compatibility. Missing,
  below-minimum, or mixed legacy/public contracts remain unsupported.
- Body/hand/foot skin uses native TXST/Skin Armor. Face skin uses NiOverride
  node-texture channels (not paint overlays): the matching legacy-v1/public-v2
  adapter for synchronous batches, with a Papyrus fallback when unavailable.
- Official interface history is checked from RM 0.4.11 (SE 1.5.97) through
  commit `9ebcb733e17be695f994cd2e9cc383043446bc02`, including every intervening
  interface-header change. Unknown higher interface revisions still require
  backward-compatible method prefixes; they are not automatically proven.
- Legacy overlay capacities use RaceMenu's published Scaleform metadata after
  startup, including its custom-INI overrides and face-enable configuration.
- Native SOS/TNG TXST routing admits the twelve listed SE/AE runtimes and
  resolves their actual Address Libraries. Startup validates relocation-aware
  ownership code and unique unwind-bounded visitor calls. Only SE 1.5.97 and
  AE 1.6.1170 currently have saved executable-code evidence; other runtimes
  must match a verified template before this backend can install. See
  `docs/COMPATIBILITY-UI-20260912-KO.md` for the verification limits.
- The non-public FormDelete guard recognizes the erroneous narrowing callback
  in the loaded SKEE image and validates the running VM's direct-form handle
  format. Official full-width or removed callbacks are left unchanged. The
  normal API is not disabled when this optional bug guard does not match.
- The native face-attachment hook and Face TXST clone/assignment path have been
  retired. Face restoration uses a separate FCNI v1 cosave record; ASTR v4 is
  unchanged. See `docs/FACE-NIOVERRIDE-TRIAL-20260911-KO.md` for limitations and
  tests. Compilation is not proof of in-game compatibility with every version.

CommonLibSSE-NG's locked transitive xmake closure is kept intact: DirectXMath
2024.02, DirectXTK 24.2.0, rapidcsv v8.92, spdlog v1.16.0, Xbyak v7.06,
CMake 4.3.4, and Ninja v1.13.2. These versions are not independently upgraded.

The source archive contains the CommonLib headers, sources, resource templates,
the vendored upstream dependency headers, and all applicable
licenses. Repository history, sample applications, prebuilt binaries, and
generated build files are deliberately excluded. The source ZIP is assembled
by `scripts/Package-Release.ps1`: Git-tracked project files plus a minimal
build-required dependency tree, not Git's empty submodule placeholders.
Downloaded xmake packages are resolved through the unchanged lock file.
BCNG is built with `EXCLUSIVE_SKYRIM_FLAT`, with VR configuration disabled;
vendored upstream VR headers do not imply a supported VR target.

## Optional SFS rendered-outfit ABI

The local SFS v1.6.5 development ABI v1 is pinned as a POD header only:
`src/BodyChangeNG/SFSRenderedOutfitAPI.h`, from SFS's
`extras/SkyrimFittingSystemRenderedOutfitAPI.h` on 2026-09-13. Original file
SHA-256: `6EE5B2E36B0AC0E17096E2E74EA53C87BE0475D6F7256CD4D620FB4FDF12FD1B`
(vendored text uses repository line endings). There is no import library or
required SFS DLL dependency. Resolve the already-loaded `SFSCore.dll` at runtime
and accept API v1 only. No dependency/build-tool baseline was upgraded.

This is the provider's unreleased development contract, not a claim that SFS
v1.6.4 exports it. Query and notification handling are isolated in
`RenderedOutfit.cpp`; fake-engine regression tests compile that actual consumer.
See `docs/SFS-OREFIT-INTEGRATION-20260913-KO.md` for lifecycle and verification.
