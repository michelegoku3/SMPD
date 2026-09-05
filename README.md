# SMPD — Steam Multi Pattern Downloader (`winmm.dll` proxy)

External bridge that downloads patterns the same way **AetherDLL** does and writes them in the format **LumaCore** expects. LC's `LumaCore.dll`, `dwmapi.dll` and `xinput1_4.dll` stay untouched.

## How it knows which patterns are installed

1. It hashes the Steam DLLs on disk with SHA-256 (`steamclient64.dll`, `steamui.dll`) — the hash is the per-build key.
2. It looks for `<Steam>\lumacore\pattern\<sha>.toml` (plus `steamclientipc\<sha>.toml` for the IPC spec, same SHA as steamclient).
3. Cache hit with valid entries = installed, no network. Cache miss, corrupt cache, or a strictly larger upstream table = download/upgrade from the source chain, then the file is written atomically (`.tmp` + rename).
4. Every run is logged to `<Steam>\lumacore\smpd_bridge.log`.

## Popup policy (change-only)

The `SMPD -- Pattern Bridge` popup appears **only when this run actually wrote pattern files**, for example after a Steam update or when your source publishes new tables:

```
New Steam patterns installed:

- steamclient (a1b2c3d4e5f6) downloaded from migo3:raw
- steamclientipc (a1b2c3d4e5f6) updated from ost to migo3

Please restart Steam so the new patterns take effect.

Next pattern check in 1h 24m (at 14:00 local time).
```

Plain launches with everything up to date stay silent. The restart notice is required because LumaCore reads the cache at startup, so freshly fetched files apply on the next launch. The countdown line follows the CI cron (`0 * * * *` UTC): it is computed from the user's own clock and shown in local time.

When a table is served by the OST fallback, the popup and the log additionally warn that cloud error notifications are normal (the fallback does not publish the cloud patterns) and will disappear once migo3 publishes the new patterns. Upgrades from a previously cached source are reported as transitions (e.g. `updated from ost to migo3`); each written file stores a `<sha>.toml.src` sidecar so the next run knows the previous source.

## Research notes: which DLLs Steam loads from its root

* `dwmapi.dll` + `xinput1_4.dll` — the only two documented names (OST docs, OST releases, LumaCore, AetherInjector, ValvePlug). Owned by LC: do not touch.
* `winmm.dll` — used by LuaTools/LuaLoader as a proxy inside the Steam folder. This is our slot.
* `version.dll` — bitness trap: the 32-bit launcher imports it too, an x64 `version.dll` kills everything with `0xc000007b`. Discarded.
* `msimg32.dll` — field-tested: never loaded (no popup/log). Discarded.

## Build (same procedure as AetherDLL)

* Release x64 only, `x64-Release` preset (Ninja). Requires MASM (`ml64`, included with the C++ Build Tools).
* Open the `SMPD` folder in **Visual Studio 2026** (CMake folder open) and build the `x64-Release` preset, or from a prompt:

```bat
build.cmd
```

Output:

```
out\build\x64-Release\out\Release\winmm.dll
```

## Install

1. Copy the new `winmm.dll` into `<Steam>\`.
2. (Optional) `<Steam>\smpd_mirror.txt` holding `https://my-server/pattern/{subdir}/{sha}.toml`.
3. Start Steam. If patterns changed you get the popup; details are always in `lumacore\smpd_bridge.log`, TOMLs in `lumacore\pattern\` (including `steamclientipc\`).

## How it works (AetherDLL mapping)

| AetherDLL | SMPD |
|---|---|
| `AetherCore/utils/PatternSource.cpp` | `SmpdProxy/utils/PatternSource.cpp` — same ordered registry (position = priority). Put your source at index 0 |
| `AetherCore/network/RuntimeHttp.cpp GetUnchecked` | `SmpdProxy/network/RuntimeHttp.cpp` — same WinHTTP GET, no allowlist |
| `AetherCore/utils/PatternDownloader.cpp` | `SmpdProxy/utils/PatternDownloader.cpp` — Level 0 custom mirror, then registry; `raw` then `cdn` on transport error only; `404` = skip source; atomic `.tmp + MoveFileEx` writes |
| `AetherCore/utils/Hasher.cpp` | `SmpdProxy/utils/Hasher.cpp` — same BCrypt SHA-256 |
| `AetherCore/utils/PatternEngine.cpp LoadModule` | `SmpdProxy/core/BridgeCache.cpp EnsureOne` — missing cache = validated download; corrupt cache = `.bad` + re-fetch; upgrade-only; separate `CountIpcEntries` for `steamclientipc` (`funcHash` schema, not `name/rva`) |
| `AetherCore/utils/IpcSpec.cpp CachePath` | same client SHA for `steamclientipc\<sha>.toml` |
| LC/Aether proxy pattern | `SmpdProxy/core/dllmain.cpp` + `winmm.def` + `winmm_stubs.asm` (MASM, 180 names + ordinal 2 from the real export table) + `winmm_forward.cpp` (fills from System32, loop-safe) — bridge on a dedicated thread outside the loader lock, `steam.exe` with nearby `steamclient64.dll` only |

## Changing sources

Edit `SmpdProxy/utils/PatternSource.cpp:DefaultSources()` — order = priority, like Aether. Rebuild Release x64 only.

## Releases (GitHub Actions, like AetherDLL)

Push a tag and the `Build and Release SMPD` workflow compiles `winmm.dll` on `windows-latest` and publishes `SMPD-<version>.zip` plus a `latest-smpd.json` sidecar:

```bat
git tag smpd-0.2.0
git push origin smpd-0.2.0
```

Tag streams: `smpd-0.2.0` / `smpd-v0.2.0` for stable, `tsmpd-*` for testing. The version is taken from the tag and synced into `CMakeLists.txt` (`project(SMPD ... VERSION ...)`, embedded as `VS_VERSION_INFO`), so never bump it by hand for a release.
