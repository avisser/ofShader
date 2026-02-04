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
- `Shift+K` Enter MIDI learn mode for kaleidoscope (first pad/knob binds).
- `d` Cycle halftone dots (off → fine → medium → coarse → off).
- `Shift+D` Enter MIDI learn mode for halftone (first pad/knob binds).
- `c` Toggle paint trail.
- `b` Cycle woofer distortion (off → on → on → off).
- `p` Cycle MIDI input ports.
- `o` Toggle MIDI test output (random Note On + CC).
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

## MIDI
- Uses `ofxMidi` for input.
- Learn: press `Shift+K`, then move a knob (CC flood) or hit a pad (NoteOn).
- Pad binding: cycles kaleidoscope modes.
- Knob binding: sets `kaleidoSegments` continuously (0..16), overriding the cycle.
- Learn halftone: press `Shift+D`, then move a knob (CC flood) or hit a pad (NoteOn).
- Halftone pad: cycles the 4 presets.
- Halftone knob: sets `halftoneScale` continuously (wider range than presets).
