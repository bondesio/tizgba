# TizGBA

> **This is a native Tizen app, not a web app.** TizGBA is a 32-bit ARM ELF written in C/C++ against
> EFL/Evas and `capi-media-audio-io`, packaged as a signed **TPK**. It is *not* an HTML5/WGT web application and
> no web engine is involved in emulation. The only WGT in this repository (`launcher/`) is a Home-rail tile that
> starts the native app.
>
> It runs on **one rooted Samsung QE75Q60AAUXXC** (Tizen 6.0, ARMv7) and depends on the research that made that possible:
>
> - [Samsung QE75Q60AAUXXC root research](https://github.com/bondesio/qe75q60aa-root-research): how the TV was rooted, how native packages are installed and how the root helper services work.
> - [Native Tizen 6 apps and UEP](https://github.com/bondesio/tizen6-native-app-uep-research): why a native process starts under User Execution Prevention but only renders its UI once UEP is disabled.
>
> This is a research record and source release, not an installer for stock TVs. No game ROMs, BIOS images, saves or
> Samsung binaries are included; link libraries are copied from your own TV at build time (see *Build*).
> Nothing here has been published as a ready-to-install package.

**Sibling projects**

- [Tizen emulator starter kit](https://github.com/bondesio/tizen-emulator-framework): Shared conventions, snippets and host-test fakes for the native Tizen TV emulators
- [TizNES](https://github.com/bondesio/tiznes): Native Tizen 6 NES emulator (InfoNES) for a rooted Samsung Q60A
- [TizGB](https://github.com/bondesio/tizgb): Native Tizen 6 Game Boy / Game Boy Color emulator (Walnut-CGB) for a rooted Samsung Q60A
- [TizNeo](https://github.com/bondesio/tizneo): Native Tizen 6 Neo Geo AES/MVS emulator (Geolith) for a rooted Samsung Q60A
- [TizPSX](https://github.com/bondesio/tizpsx): Native Tizen 6 PlayStation emulator (PCSX ReARMed) for a rooted Samsung Q60A

TizGBA is a native ARM Game Boy Advance emulator for the Samsung Q60A. It
uses the pinned mGBA 0.10.5 core and follows the NES/GB shared emulator layout
and input settings. A valid GBA BIOS is optional: mGBA's HLE BIOS is used when
`gba_bios.bin` is absent or invalid. The app does not include games, a BIOS, or
save files.

## Build

On a Linux workstation with Tizen Studio (ARM toolchain and a signing profile named `tvdev`), build the signed native package with:

```sh
make -C app -j4 package
make -C launcher
```

The native artifact is `app/out/org.pablo.tizgba-0.1.0-arm.tpk`; the separate
Home launcher is `launcher/out/GBALAUNCH1-1.0.0.wgt`. Both use the `tvdev`
signing profile. The launcher targets the native app ID in
`app/tizen-manifest.xml`. `app/Makefile` pulls TV libraries over the existing
root SSH connection if its local build cache is absent. No build or launch on
the TV is required to compile and sign these artifacts.

## Library and controls

The library lists `Exit TizGBA` first, then `.gba`, `.agb`, and valid-header
`.bin` ROMs from internal storage and mounted USB drives. Scans are bounded;
USB entries are tagged, hot-plug changes rescan the list, and battery saves
stay in app storage. The save filename is SHA-256 of the exact ROM bytes loaded
by the core, so renaming or moving a game does not change its save identity.

Settings use the shared Controllers, Audio, Video, and System sections.
Controller mapping keeps the physical D-pad and analog axes independent; the
guided controller check reports evdev and joydev input. Up to two selected
physical controllers act as alternatives for the same GBA keypad. Exclusive
evdev capture is controlled by the root `tizgba-input` watcher and is active
only while the app process is in the foreground. GBA link play is not
implemented.

## Video options

The Video section persists these options in `tizgba-settings.conf`:

- **Screen size:** Pixel-perfect integer scale (shows the chosen `N×` scale),
  Fit (largest centered 3:2 rectangle), or Fill (stretch to the canvas).
- **Scaling filter:** Sharp / nearest, smooth bilinear, sharp-bilinear,
  Scale2x (AdvMAME2x), Scale3x, Scale4x (Scale2x twice), or the built-in
  edge-directed 2x/3x filter. The edge-directed mode is an original compact
  scaler, not a port of HQ2x/xBR source.
- **Overlay:** Off, LCD grid with adjustable strength, subtle scanlines, or
  strong scanlines.
- **Colour:** Vivid/raw, GBA LCD correction, or darker LCD correction. Both
  correction modes use gamma and channel crosstalk and are independent from
  scaling and overlays.
- **Frame blending:** Off by default; blends the previous frame for games
  that use alternating frames to simulate transparency.

The exact scaler, overlay, colour correction, sizing and fallback behavior is
described in [docs/FILTERS.md](docs/FILTERS.md).

The filter library has bounded 1920×1280 intermediate buffers and conservative
cost classes. When observed filter time repeatedly exceeds its frame budget,
the frontend moves to a cheaper live filter while retaining the saved request.
The performance estimates are not a substitute for a real Q60A benchmark;
all filter modes still need on-device measurement.

## Current validation boundary

The current source builds and signs an ARM TPK, and the launcher WGT was
previously built and signed. The ARM executable's undefined symbols were
compared with the cached TV libraries; no missing exports were found. Neither
the GBA app nor its launcher has been installed or launched on the TV. ROM
loading, controller capture, audio, save persistence, visual output, and filter
speed on the Q60A remain unverified. See `STATUS.md` for the current evidence.

## License

The frontend code in this repository is released under the Unlicense (see `LICENSE`). The vendored mGBA core (`vendor/mGBA/`, MPL-2.0) and any other third-party code keep their own licenses; see `vendor/mGBA/LICENSE`.
