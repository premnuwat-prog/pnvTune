# pnvTune 0.4 validation — 2026-09-07

Built in Release for Apple Silicon arm64 using Apple clang 17 and JUCE 8.0.9.

## Completed

- Apple `auval -v aufx Ptun Prem -strict`: **AU VALIDATION SUCCEEDED**.
  Full log: auval-pnvTune-v0.3.log.
- Apple validator: initialization, mono/stereo rendering, parameter scheduling/ramping,
  factory presets, latency/tail properties and custom Cocoa UI passed.
- AU and VST3 bundles: ad-hoc signatures verified with `codesign --verify --strict`.
- DSP tests: 44.1, 48 and 96 kHz; 64-sample processing blocks.
- Impulse delay: 530 / 576 / 1152 samples respectively, matching declared host latency.
- Input tones with a second harmonic: 95, 147, 225, 435 and 880 Hz corrected to
  the nearest chromatic pitch with under 2 cents measured steady-state error across tested rates.
  Output was measured using an independent autocorrelation estimator.
- All 12 keys and five scale masks checked for allowed-note selection.
- Five Borrowed Chord slots combine their note masks. Each supports eight chord types,
  independent enable/automation, root and quality. G Major + E Major adds G# only.
- Pitch detection now median-filters three observations, smooths valid pitch and requires
  two consecutive frames before changing target; three missing frames are tolerated.
- Vocal Gate prevents sub-threshold pitched bleed from starting correction without muting
  the output. Large pitch jumps require three stable analysis frames, reducing guitar-note
  attacks taking control while an established vocal is present.
- Cubic delay interpolation remains enabled. Air Preserve and its two per-channel filter
  paths were removed completely after listening tests preferred its 0% setting.
- Silence remains zero; identical stereo inputs remain sample-aligned; tested outputs finite.
- CPU: approximately 0.034 seconds to process 10 seconds of synthetic stereo
  audio at 48 kHz / 64 samples (0.34% of a single core, offline benchmark).
  This is not a guarantee of total Logic session CPU or worst-case real-time scheduling.
- Parameter state and factory preset round-trip, including key, borrowed chords,
  Vocal Gate and retune values, passed.
- Real JUCE editor rendered at 2× scale and visually inspected; preview: pnvTune.png.
- UI refresh is limited to 24 Hz; the audio callback still allocates no memory and takes no locks.
- Installed at the original `PremTune.component` and `PremTune.vst3` paths with the
  displayed product name `pnvTune`. The prior binaries remain recoverable in
  `dist/legacy-PremTune-v0.2`.
- AU/VST3 manufacturer and plugin codes plus the state-tree type remain compatible with
  PremTune so existing project instances can resolve the renamed plugin and retain shared parameters.
- Chord slot 1 retains PremTune 0.2's parameter IDs, so its enable/root/type automation
  and saved values can migrate; slots 2–5 are new parameters.
- The editor begins with no chord rows. `+ ADD CHORD` reveals and enables one slot at a
  time; `−` disables and hides the last slot. The visible count is saved with plugin state.

## Limits of verification

- 12 ms is the nominal centre of a variable-delay pitch shifter, not a fixed latency
  for every wet sample. Detection settling and retune response add to perceived correction time.
- No microphone/voice recording was made; singing quality, formants, rapid transitions,
  breath sounds and hardware round-trip latency remain unverified on actual vocals.
- The mixed-source regression uses synthetic voice-like and guitar-like harmonic tones.
  It verifies octave/subharmonic rejection after vocal lock, not full source separation.
- Native AU host compatibility is checked with Apple's validator; listening quality is
  based on the user's real vocal test of the preceding version and the preferred Air = 0 path.
- VST3 was built, generated its module manifest and passed signature checks; it has
  not been run through an independent VST3 validator or DAW session.
- An additional Address/UndefinedBehavior Sanitizer run stalled without output in
  this environment and was cancelled; no sanitizer pass is claimed.
- No Intel/Rosetta, other Macs or notarization testing was performed.
