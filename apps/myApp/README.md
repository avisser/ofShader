# ofShader (myApp)

## Pipeline
- **Input**: Webcam via `ofVideoGrabber` at the configured resolution/FPS.
- **Update**:
  - Motion analysis (`updateMotion`) computes movement intensity + center and feeds the paint trail.
  - If shader mode is **off** (`2`), OpenCV MOG2 builds a background mask (threshold + morph + blur).
- **Draw**:
  - Background image (`bg.jpg`) or a flat gray fallback.
  - Foreground:
    - **Shader mode** (`1`): webcam texture → optional kaleidoscope/halftone → woofer distortion (beat‑synced) → HSV key → posterize + edge boost → optional hue‑pulse → alpha output.
    - **BG‑sub mode** (`2`): composited RGBA mask from MOG2.
  - Paint trail overlay (toggle `c`).

## Key Bindings
- `1` Shader key mode (HSV key + stylize).
- `2` Background subtractor mode (OpenCV MOG2).
- `k` Cycle kaleidoscope modes (off → 4 → 6 → 8 → 10 → 12 → off).
- `d` Cycle halftone dots (off → fine → medium → coarse → off).
- `c` Toggle paint trail.
- `b` Cycle woofer distortion (off → on → on → off).
- `r` Reset background model.
- `e` Toggle morph (bg‑sub mode).
- `s` Toggle shadow detection (bg‑sub mode).
- `+` / `-` Adjust mask threshold (bg‑sub mode).
- `[` / `]` Previous/next camera device.
- `f` Toggle fullscreen.
- `Esc` Quit.

## Handy Tweaks (in `src/ofApp.h`)
- Green key: `keyHueDeg`, `keyHueRangeDeg`, `keyMinSat`, `keyMinVal`
- Stylize: `posterizeLevels`, `edgeStrength`
- Kaleidoscope: `kaleidoSegments`, `kaleidoSpin`
- Halftone: `halftoneScale`, `halftoneEdge`
- Hue pulse: `pulseBpm`, `pulseHueShiftDeg`
- Woofer: `wooferStrength`, `wooferFalloff`
- Paint trail: `trailFade`, `trailSize`, `trailOpacity`, `motionThreshold`
