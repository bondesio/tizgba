# TizGBA video filters

`app/filters.c` is a freestanding C filter pipeline. It receives the mGBA
240×160 frame and writes a bounded intermediate image for Evas. The pipeline
order is colour correction, optional frame blending, scaler, then LCD or
scanline overlay. Settings are applied live and persisted by the frontend.

## Options

| Setting | Behavior |
|---|---|
| Sharp pixels (nearest) | Keeps source pixels hard-edged; integer screen sizing uses the largest whole-number scale that fits. |
| Smooth (bilinear) | Lets Evas blend neighboring pixels when the output size is fractional. |
| Sharp bilinear | Replicates pixels to an integer intermediate size, then uses bilinear sampling for the remaining scale. |
| Scale2x (AdvMAME2x) | Applies the edge-aware 2× neighborhood rule to reduce blockiness while retaining hard edges. |
| Scale3x (AdvMAME3x) | Applies the corresponding 3× neighborhood rule. |
| Scale4x (Scale2x chain) | Applies Scale2x twice for a 4× intermediate image. |
| Edge-directed 2x/3x | Uses a compact original edge-directed rule; it is not vendored HQ2x/xBR code. |
| LCD grid | Adds a strength-adjustable RGB subpixel grid, where the intermediate scale permits it. |
| Scanlines (subtle/strong) | Darkens alternating source rows at the output scale. |
| GBA LCD correction | Applies a gamma and channel-crosstalk model to approximate the original LCD response. |
| GBA LCD correction (dark) | Uses a darker gamma variant of the same colour model. |
| Frame blending | Blends the previous image into the current image to soften games that use alternating frames for transparency. Off by default. |

Screen sizing is a separate option: **Pixel-perfect integer** chooses the
largest integer `N` such that `240N × 160N` fits the canvas; **Fit height
(3:2)** uses the largest centered 3:2 rectangle; **Fill screen** stretches to
the full canvas. The frontend reads the current Evas canvas dimensions.

## Limits and cost fallback

Intermediate dimensions are capped at 1920×1280. The filter planner reduces
scale factors or disables an overlay if the requested combination exceeds
those bounds. It reports the effective mode and adjustments in the runtime
log. A repeated presentation cost over one third of the frame interval steps
down the configured cost ladder and keeps the user's saved request for the
next game launch.

The source includes a provisional Cortex-A72 cost model: colour correction
and blending are estimated per 240×160 source pixel; scaler and overlay costs
are estimated per output pixel. Those constants have not been calibrated on
the target and are **estimates, not measured performance**. There is no GBA
image-golden or filter timing harness in this repository yet. Each mode's
visual output and worst-case timing still need review before calling it
validated on the Q60A.

## Attribution and implementation

The Scale2x/Scale3x pixel rules are implemented from their published
neighborhood definitions; this project does not include code copied from the
GPL Scale2x implementation. The colour correction matrix, LCD grid, scanline
overlay, edge-directed scaler and frame blending are original implementations
in this project. See the source header and the mGBA `LICENSE`/`UPSTREAM.md` for
core attribution.
