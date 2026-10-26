# TizGBA status

## Current state — 2026-10-24


- **TizGBA 0.2.0 installed (2026-10-26, still never launched on the TV):**
  adds Settings → System → **Performance overlay** (Off / FPS / Detailed,
  persisted as `perf_overlay` in the existing `version=2` settings file) and
  starts the library on the first game (Settings when empty) instead of Exit.
  Signed TPK SHA-256 `d953f57d601ebee1339a26e335b7a3c520969d2192fc17f0384b862204ab88d6`;
  installed ELF `c980a936072efea047305faa5d176b6d224b2e8fe008daa075f707616df330da`
  (a later Makefile-only rebuild re-signed `app/out/` with the identical ELF,
  so that TPK's hash differs from the installed one).
  `harness/` is the first execution of the real `gba_main.c` and `filters.c` (fake
  EFL, library and mGBA bridge): 21 checks, ASan/UBSan clean. Core, saves, audio
  and controllers remain unverified. Rollback: `backups/2026-10-26-before-0.2.0-deploy/`.

- A native Tizen frontend is present, using the NES/GB menu layout, Exit-first
  ROM library, sectioned settings, persistent configuration, two controller
  slots, independent digital D-pad and analog mapping, guided input checks,
  audio controls, video filters, and a System section.
- The pinned mGBA 0.10.5 GBA core is built statically. The platform adapter
  reads ROMs and optional BIOS/save imports into memory, uses HLE BIOS fallback,
  supports battery/RTC saves and hashes the exact loaded ROM bytes to key the
  internal save. The GBA logo and separate Home launcher are included.
- `make -j4 package` completed successfully on the ARM Tizen toolchain and
  produced signed package version 0.1.0. The WGT launcher was built and signed
  earlier in this work. The current ELF is ARM32 EABI soft-float PIE. Its 91
  undefined dynamic symbols were compared with the cached TV libraries; no
  unresolved exports were found.
- The root input watcher follows the established emulator watcher pattern,
  coordinates through the shared input lock, and grants capture only while a
  live TizGBA process has written its active marker. Pause, exit, and terminate
  remove the marker; stale markers are discarded by the watcher.
- Video settings provide nearest, smooth, sharp-bilinear, Scale2x, Scale3x,
  Scale4x, edge-directed scaling, LCD grid, scanlines, two LCD colour modes,
  frame blending, and integer/fit/fill sizing. Filter costs remain estimates;
  no Q60A timing measurements have been made.
- The local PSX 0.2.7 package was already installed and matched the local
  package/executable during the prior continuation, so no PSX reinstall was
  needed.
- **TizGBA 0.1.0 is installed on the Q60A (2026-10-24, launch deferred).**
  The signed native TPK and Home launcher WGT are registered and enabled. The
  installed native ELF matches the local build. The `tizgba-input-access`
  watcher is active; NES still owns `/dev/input/event15`, and GBA holds no
  event node while the app is stopped. The WGT is installed in Apps; pinning
  it to Home requires the TV Apps UI.
- **Super Mario Advance 2 is staged on the TV pendrive.** Extracted from the
  user-provided BC-250 Downloads ZIP and copied to
  `/opt/media/USBDriveA1/TizGBA/games/`. The 4 MiB file's hash matches the
  extracted BC-250 stream and the TV copy. The app has not been launched, so
  app-context USB access and game behavior are unverified.

## Not verified

- The GBA app has **not** been launched on the TV. The game library, mGBA ROM
  startup, controller mappings/capture, audio, save persistence, display
  sizing, and filter performance therefore remain unverified on-device.
- No GBA test ROM was used. Filter image-quality and per-filter CPU behavior
  have not been exercised by an automated harness or on the TV.
- A downstream reviewer ran isolated host fakeroot assertions for the watcher
  and launcher in an earlier continuation; the assertions passed, but the
  watcher runner emitted a fakeroot teardown SIGTERM warning. Those temporary
  harness files are not part of this repository. This is not TV evidence.

## Next work

1. When the TV is free, Pablo can launch the installed TizGBA Home tile, then
   use `tools/deferred-tv-checks.sh` and test the staged game, controls, audio,
   saves, and filter performance.
2. Update this file with functional evidence only after
   those checks actually happen.

## Artifacts from the latest local build

- ELF: `app/bin/tizgba`, SHA-256
  `d5aa006b86573a0430791b0f2ac7b5708e0ef149f9639711ff727b028f16aff2`
- Signed TPK: `app/out/org.pablo.tizgba-0.1.0-arm.tpk`, SHA-256
  `d01a46aaf476f015af53c337160c28a55043992617ee052ab23e5671a195f1e9`
- Signed launcher WGT: `launcher/out/GBALAUNCH1-1.0.0.wgt`, SHA-256
  `b01b3772f99a2ced6a66d24eeae291ce1ef395aeca7ec67af958fbf11749ab14`

## References

- User-facing build, settings and validation notes: [README.md](README.md)
- Filter behavior and unmeasured cost model: [docs/FILTERS.md](docs/FILTERS.md)
- mGBA pin and source provenance: [vendor/mGBA/UPSTREAM.md](vendor/mGBA/UPSTREAM.md)
