# ofShader (myApp)

## Pipeline
- **Input**: Webcam via `ofVideoGrabber` at the configured resolution/FPS.
- **Update**:
  - Motion analysis (`updateMotion`) computes movement intensity + center and feeds the paint trail.
  - If shader mode is **off** (`2`), OpenCV MOG2 builds a background mask (threshold + morph + blur).
- **Draw**:
  - Background image (`bg.jpg`) or a flat gray fallback.
  - Foreground:
    - **Shader mode** (`1`): webcam texture → optional kaleidoscope/halftone → woofer distortion (beat‑synced) → HSV key → posterize + edge boost → optional hue‑pulse → optional saturation → alpha output.
    - **BG‑sub mode** (`2`): composited RGBA mask from MOG2.
  - Paint trail overlay (toggle `c`).

## Key Bindings
- `1` Shader key mode (HSV key + stylize).
- `2` Background subtractor mode (OpenCV MOG2).
- `k` Cycle kaleidoscope modes (off → 4 → 6 → 8 → 10 → 12 → off).
- `Shift+K` Enter MIDI learn mode for kaleidoscope (first pad/knob binds).
- `z` Cycle kaleidoscope zoom (0.9 → 0.7 → 0.5).
- `Shift+Z` Enter MIDI learn mode for kaleidoscope zoom (first pad/knob binds).
- `d` Cycle halftone dots (off → fine → medium → coarse → off).
- `Shift+D` Enter MIDI learn mode for halftone (first pad/knob binds).
- `v` Cycle saturation presets (off → 0.2 → 0.45 → 0.7 → 0.9 → off).
- `Shift+V` Enter MIDI learn mode for saturation (first pad/knob binds).
- `t` Cycle tempo (60 → 80 → 100 → 120).
- `Shift+T` Enter MIDI learn mode for tempo (first pad/knob binds).
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
- Saturation: `saturationScale`
- Kaleidoscope: `kaleidoSegments`, `kaleidoSpin`
- Kaleidoscope framing: `kaleidoZoom` (lower = use more center)
- Halftone: `halftoneScale`, `halftoneEdge`
- Hue pulse: `pulseBpm`, `pulseHueShiftDeg`
- Woofer: `wooferStrength`, `wooferFalloff`
- Paint trail: `trailFade`, `trailSize`, `trailOpacity`, `motionThreshold`

## MIDI
- Uses `ofxMidi` for input.
- Learn: press `Shift+K`, then move a knob (CC flood) or hit a pad (NoteOn).
- Pad binding: cycles kaleidoscope modes.
- Knob binding: sets `kaleidoSegments` continuously (0..16), overriding the cycle.
- Learn kaleido zoom: press `Shift+Z`, then move a knob (CC flood) or hit a pad (NoteOn).
- Kaleido zoom pad: cycles zoom presets.
- Kaleido zoom knob: sets zoom continuously (1.0 → 0.3).
- Learn halftone: press `Shift+D`, then move a knob (CC flood) or hit a pad (NoteOn).
- Halftone pad: cycles the 4 presets.
- Halftone knob: sets `halftoneScale` continuously (wider range than presets).
- Learn saturation: press `Shift+V`, then move a knob (CC flood) or hit a pad (NoteOn).
- Saturation pad: cycles the 4 presets plus off.
- Saturation knob: sets saturation continuously (0 = B/W, 1 = full color).
- Learn tempo: press `Shift+T`, then move a knob (CC flood) or hit a pad (NoteOn).
- Tempo pad: cycles tempo presets.
- Tempo knob: sets BPM continuously (60 → 120).
- Settings are persisted to `bin/data/settings.yaml` and loaded on startup. The device name is matched against available MIDI ports; a warning is logged if no match is found.
