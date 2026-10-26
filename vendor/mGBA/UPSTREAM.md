# mGBA source provenance

The vendored core files are from the official mGBA 0.10.5 release source.
The Tizen GBA build targets the GBA core only. Existing files were retained;
missing source files required by the upstream ARM, mCore, GBA and utility source
lists were copied from this exact upstream Git commit:

- Repository: https://github.com/mgba-emu/mgba
- Tag: `0.10.5`
- Commit: `26b7884bc25a5933960f3cdcd98bac1ae14d42e2`
- Downloaded GitHub codeload archive SHA-256:
  `9475c26e9fa2f4b30c07ab6636e4b0a5b62e4baee2109ede7b2fecc52edae366`

No upstream build scripts were run. The upstream source archive was used only
to inspect and copy source files. See `LICENSE` for the MPL-2.0 license.
`src/core/version.c` is retained from the existing 0.10.5 snapshot.

## Local safety patches

The following small downstream changes preserve upstream behavior while
making allocation failures recoverable in the constrained Tizen process:

- `src/gba/core.c`: return `NULL` if `GBACoreCreate` cannot allocate its
  wrapper; report failed PSG audio allocation from core initialization.
- `src/gb/audio.c`, `src/gba/audio.c`: check both blip buffers before setting
  rates or exposing the core as initialized.
- `src/core/config.c`: preserve a null config-port pointer when allocation
  fails instead of formatting through it.
- `src/gba/gba.c`: fail ROM load cleanly if the non-power-of-two mirror buffer
  allocation fails.
- `src/gba/savedata.c`: do not serialize an RTC trailer until the game has
  latched an initial clock value.
