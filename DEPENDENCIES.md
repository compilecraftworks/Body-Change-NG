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

- Legacy Skyrim SE Override v0, UBE's newer AE-backported Override v0, and
  official Override v1 are distinguished at runtime and submit texture strings
  through RaceMenu's own NiOverride Papyrus natives. This avoids constructing
  another DLL's private string variant while preserving exact persistent keys.
- Official Override v2 uses the audited public native wrapper interface. It is
  never selected for either v0 implementation or v1.
- The loaded Skyrim runtime separates legacy SE v0 from the UBE AE-backport v0.
  The resolved route and runtime are written to `BodyChangeNG.log`.
- Unknown BodyMorph versions outside v4-v5 and Override versions outside
  v0-v2 fail closed until their ABI is audited.

CommonLibSSE-NG's locked transitive xmake closure is kept intact: DirectXMath
2024.02, DirectXTK 24.2.0, rapidcsv v8.92, spdlog v1.16.0, Xbyak v7.06,
CMake 4.3.4, and Ninja v1.13.2. These versions are not independently upgraded.

The source archive contains the CommonLib headers, sources, resource templates,
the OpenVR headers used by the unified SE/AE/VR build, and all applicable
licenses. Repository history, sample applications, prebuilt binaries, and
generated build files are deliberately excluded. The source ZIP is assembled
by `scripts/Package-Release.ps1`: Git-tracked project files plus a minimal
build-required dependency tree, not Git's empty submodule placeholders.
Downloaded xmake packages are resolved through the unchanged lock file.
