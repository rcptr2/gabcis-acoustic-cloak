# Acoustic Cloak — Changelog

Versioning: `0.<phase>.<patch>`. Each development phase bumps the **MINOR**
version — the middle number (Phase 1 shipped as 0.1.0, Phase 2 as 0.2.0, and
so on). A smaller fix or refinement within a phase bumps the **PATCH**
version instead of starting a new phase entry. The **MAJOR** version stays
`0` throughout development; `1.0.0` is reserved for the finished plugin (all
5 blueprint phases complete, tested, and packaged with the final PDF
overviews).

## v0.1.0 — 2026-08-05 — Phase 1: Bus Layout & Sidechain Setup

- `PluginProcessor.h/.cpp` (`AcousticCloakAudioProcessor`): `BusesProperties`
  with Main Input (stereo, the Bass), Main Output (stereo), and an optional
  Sidechain Input bus (disabled by default, accepts mono or stereo when a
  host enables it, the Kick) — same routing shape as the sibling PhaseLock
  Sub project's Phase 1.
- `isBusesLayoutSupported()` enforces Main In == Main Out (mono or stereo)
  and restricts the Sidechain bus to disabled/mono/stereo.
- APVTS parameters, per the blueprint's Section 3: `Cloak Depth` (0–100%,
  default 100%), `Lookahead` (0–20ms, default 0ms — unlike the sibling
  PhaseLock Sub project's Lookahead *toggle*, the blueprint explicitly asks
  for a continuous ms slider here), `Target Freq Low`/`Target Freq High`
  (20–200Hz / 30–300Hz, default 20/150Hz — the blueprint's own working
  band), `Transient Sensitivity` (0–100%, default 50%), `Bypass`.
- `Lookahead` is realised as a plain delay line (`juce::dsp::DelayLine`) on
  the Main path, reserving temporal margin for Phase 3's rotator — reported
  to the host via `setLatencySamples()`. Since changing reported latency
  mid-playback causes dropouts in most DAWs if done from the audio thread,
  the actual host notification is deferred to the message thread via
  `AsyncUpdater` (same technique, and same reasoning, as the sibling
  PhaseLock Sub project's own Lookahead handling); the DSP-side delay target
  updates immediately since that's just a ramp target, not a host-facing
  call.
- `Bypass` crossfades (~20ms `SmoothedValue`) rather than switching
  instantly, and the dry copy is captured **after** the Lookahead delay so
  the reported latency stays constant regardless of Bypass state — both
  lessons learned the hard way in the sibling PhaseLock Sub project's own
  post-release fixes, applied here from the start instead of being
  rediscovered.
- `createEditor()` returns a `juce::GenericAudioProcessorEditor` placeholder
  until Phase 4's custom Holographic Radar GUI.
- CMake scaffold: JUCE 9.0.0 (`FetchContent`), C++20, `VST3` on Windows and
  macOS, plus `AU`/`Standalone` on macOS. This project folder's name has no
  apostrophe, so the sibling projects' build-directory guard isn't needed
  here.

No phase-correction DSP yet — the analysis engine and the actual rotation
land in Phases 2–3. This phase is scaffolding + verified I/O routing only.

## v0.2.0 — 2026-08-05 — Phase 2: Phase-Correlation Analysis Engine

- **Design decision, deviating from the blueprint's own primary suggestion:**
  the blueprint offers a choice between an STFT/overlap-add phase vocoder or
  a dynamically-tuned All-Pass filter network, and separately flags the STFT
  route's own CPU/window-size risk explicitly ("Ha túl nagy az ablak, az
  effekt lassú; ha túl kicsi, nincs elég felbontás a sub-basszusnál"). At
  20–150Hz, resolving individual FFT bins would need an impractically large
  window (e.g. ~23Hz/bin at a 2048-sample window @ 48kHz — too coarse to
  isolate the target band from anything else nearby) or an impractically
  long one for finer resolution, adding both latency and pre-ringing risk.
  Chose the All-Pass-network branch instead, built around a new
  **complex one-pole resonator** (`Source/ComplexResonator.h/.cpp`):
  `y[n] = p·y[n-1] + (1-r)·x[n]` with `p = r·e^{jω0}`. Fed a real signal, it
  produces an approximate analytic (I/Q) signal concentrated at its centre
  frequency — one mechanism that both band-limits (a bandpass filter) AND
  exposes instantaneous phase, at ~4 multiplies + 4 adds/sample. This is the
  same building block Phase 3 will reuse to actually rotate the Main
  signal's target-band content (rotate its own analytic output by e^{jθ}
  and take the real part), so the analysis and the correction share one
  coherent, cheap mechanism instead of two.
- New `Source/PhaseCorrelationAnalyzer.h/.cpp`: one `ComplexResonator` each
  for Sidechain (Kick) and Main (Bass, both channels summed to mono, same
  convention as the sibling PhaseLock Sub project's analyzer), retuned once
  per block from the smoothed `Target Freq Low/High` sliders. Computes
  `z = side·conj(main)` per sample — its angle is the instantaneous
  Sidechain-minus-Main phase difference, its magnitude is `|side|·|main|`,
  both from one complex multiply. Continuously tracked (not a discrete
  onset-triggered snapshot like the sibling project's cross-correlation
  analyzer — Phase 3's rotator needs a running estimate, not a periodic
  one), via an exponentially-smoothed unit vector (`cos`/`sin` of the phase
  difference, renormalised each sample to counteract the shrinkage that
  blending two unit vectors as a straight line otherwise causes) instead of
  smoothing the raw angle directly, which avoids ever having to unwrap
  across the ±π seam.
- **`Transient Sensitivity`**, per the blueprint's own parameter: a
  fast-attack/slow-release envelope follower on the Sidechain resonator's
  own band-limited magnitude (not the raw broadband input) decides, sample
  by sample, whether a Kick transient is "active" against an adaptive
  threshold (`slowFloor × ratio`, where higher sensitivity lowers the
  ratio). While active, the smoothed phase estimate adapts quickly (~5ms);
  while quiet, it drifts slowly (~300ms) instead of chasing noise between
  hits — a continuous analogue of the sibling project's discrete
  "onset → capture → Hold" behaviour.
- `PluginProcessor::processBlock()` feeds the (post-Lookahead-delay) Main
  buffer and the Sidechain buffer into the analyzer every block and
  publishes the result via thread-safe atomics (`displayCorrelation`,
  `displayPhaseDeltaRadians`, `displayCorrelationValid`) for Phase 4's GUI.
  Still analysis only — the Main signal passes through unmodified; that's
  Phase 3.
- Added `Tests/ComplexResonatorTests.cpp` (settles to ~unity magnitude at
  its own centre frequency; attenuates content far outside its band; stays
  bounded on silence) and `Tests/PhaseCorrelationAnalyzerTests.cpp`
  (converges to correlation ≈ +1 for in-phase tones, ≈ -1 for anti-phase
  tones, and the correct measured angle for a quarter-cycle offset;
  confirms silence never reports a valid result).

## v0.2.1 — 2026-08-05 — Phase 2 Revision: Architecture Correction from Design Review

An external AI design review of v0.2.0 raised three specific, valid risks in
the single-`ComplexResonator` design before Phase 3 could build on it. All
three are addressed here, before any actual audio correction is added.

- **Risk 1 — a single resonance point can't cover the whole Target
  Frequency Range evenly:** tuning one resonator to the band's midpoint
  leaves the band's own edges attenuated and phase-distorted relative to
  the centre (the default 20-150Hz band's low end sits nearly 1.5 octaves
  below the geometric centre). **Fix:** new `Source/ResonatorBank.h/.cpp`
  splits `[lowHz, highHz]` into `kNumBands = 3` geometrically-spaced
  sub-bands, each with its own `ComplexResonator`, combined by summing
  their complex outputs — every point in the range now stays close to some
  sub-band's own centre.
- **Risk 2 — reconstruction/coloration risk:** the v0.2.0 design's Phase 3
  plan (run the broadband signal through the resonator, subtract-and-add
  -back to recombine) is not a magnitude-flat operation on its own, and
  risks colouring content outside the user's chosen band. **Fix:** new
  `Source/BandIsolator.h/.cpp` — a 3-way Linkwitz-Riley crossover (two
  chained 4th-order splits, reusing the sibling PhaseLockSub project's
  proven single-filter dual-output technique) isolates `[lowHz, highHz]`
  from the Main signal via a genuinely flat-summing complementary pair
  (`targetBand + residual` reconstructs the input exactly) BEFORE any
  resonator touches it. `PluginProcessor` now owns `mainTargetBandBuffer`/
  `mainResidualBuffer` (ready for Phase 3 to rotate the former and recombine
  with the latter) and does the same for the Sidechain. The reviewer's own
  suggested fix was a wideband Hilbert transform on the isolated band; that
  was evaluated and rejected for this specific band (see below) in favour
  of reusing the already-validated `ComplexResonator`/`ResonatorBank`
  underneath the new crossover isolation instead.
  - **Why not the reviewer's literal Hilbert-transform suggestion:** a
    windowed-sinc FIR Hilbert transformer needs to span several cycles of
    the LOWEST frequency it must handle accurately. At 20Hz, even after a
    modest 8x decimation (48kHz→6kHz), one cycle is 300 samples — a
    practical filter length (tens to ~100 taps) can't resolve that, and
    reducing decimation to fix it directly conflicts with needing the
    decimated Nyquist to stay safely above 150Hz. The 20-150Hz range spans
    too many octaves (≈2.9) for one wideband Hilbert transformer to cover
    cheaply. The sub-banded resonator approach (Risk 1's fix) sidesteps this
    by never needing single-filter accuracy across the whole range.
- **Risk 3 — transient smearing:** measuring Transient Sensitivity from the
  resonator's own (resonant, therefore ringing) magnitude makes a short
  Kick transient's detected envelope decay artificially slowly, since the
  filter itself keeps ringing after the transient ends. **Fix:**
  `PhaseCorrelationAnalyzer`'s activity envelope follower now reads the RAW
  broadband Sidechain signal (passed as a separate, unfiltered parameter to
  `processBlock()`), decoupled entirely from the band-limited analysis
  signal — a plain envelope follower on a broadband signal has no resonant
  ringing to smear the reading.
- `PhaseCorrelationAnalyzer::processBlock()` signature changed to take three
  buffers (`sidechainRawBuffer`, `sidechainBandBuffer`, `mainBandBuffer`)
  instead of two, reflecting that band isolation is now the caller's
  responsibility (via `BandIsolator`), not something this class does
  itself on raw input.
- Added `Tests/BandIsolatorTests.cpp`: verifies the flat-summing guarantee
  directly — `targetBand + residual` RMS gain stays within 2% of unity
  across an 8-tone sweep from 10Hz to 8kHz for the default 20-150Hz band
  (this is the concrete, measured proof for Risk 2's fix, not just an
  assertion) — plus a confinement check that an in-band tone lands
  overwhelmingly in `targetBand`, not `residual`.
- Updated `Tests/PhaseCorrelationAnalyzerTests.cpp` for the new
  `processBlock()` signature (in-band test tones stand in for their own
  already-isolated versions, since a real `BandIsolator` passes them
  through near enough to 0dB ripple at those frequencies per the new
  `BandIsolatorTests.cpp`). All previously-passing phase/correlation
  assertions still pass unchanged. Full suite: 20 assertions, 9 test cases.

## v0.3.0 — 2026-08-05 — Phase 3: Phase Rotator Motor

The actual audio-path correction, arrived at after a SECOND round of design
review found a real problem with the most obvious way to build it. Recorded
in full because the dead end is as informative as the destination.

- **A second design review (on v0.2.1) approved the BandIsolator/broadband
  -envelope fixes but flagged a new risk**: using `ResonatorBank` (which
  sums 3 overlapping resonant filters) to actually reconstruct audio, not
  just measure a phase angle, risks a "lumpy" combined magnitude response —
  each resonator's gentle (6dB/oct) skirt bleeds into its neighbours'
  passbands, which a raw sum doesn't correct for the way a real
  complementary crossover pair does. The review's own suggested fix was a
  broadband IIR all-pass Hilbert network; not adopted here, for the same
  category of reason the FIR route was already rejected in v0.2.1 —
  hand-deriving allpass corner frequencies accurate across a ~3-octave span
  without a proper filter-optimisation toolchain (Remez/equiripple) risks
  an unverifiable, subtly-wrong design.
- **First attempted fix, built, tested, and discarded**: `SubBandSplitter`,
  a further 3-way Linkwitz-Riley split INSIDE the already-isolated target
  band (reusing BandIsolator's own proven chained-filter technique one
  level deeper), so each of 3 sub-bands could get its own well-matched
  resonator without ResonatorBank's raw-summing problem. Its own flat
  -summing test failed hard on a real tone: **35Hz measured 0.787 RMS gain
  (a 21% error), not a settling-time artefact** (quadrupling the test's
  measurement window made no difference). Root cause, confirmed by a
  targeted diagnostic added to `BandIsolatorTests.cpp`: chaining two
  Linkwitz-Riley splits is only exactly flat when each stage's own
  low+high truly reconstructs its input, which requires the SECOND
  filter's own allpass phase shift to be negligible at the frequencies
  where the first filter's output still has energy — true when the two
  cutoffs are far apart (BandIsolator's 20Hz/150Hz, a 7.5x ratio: the same
  diagnostic measured <2.2% error even right at its edges) but false when
  they're close (SubBandSplitter's internal ~39Hz/~77Hz split, only ~2x
  apart in an already-narrow 20-150Hz range). This is a known, real
  limitation of naive cascaded multi-way crossovers — real 3-way loudspeaker
  crossovers keep their split points widely separated for exactly this
  reason — and this project's 20-150Hz target range is too narrow to fit
  two internal splits that way. `Tests/BandIsolatorTests.cpp` gained a
  permanent regression test from this finding (`"BandIsolator stays flat
  close to its own crossover points, not just far from them"`) —
  `SubBandSplitter.h/.cpp` and its test were deleted.
- **The actual fix**: new `Source/PhaseRotator.h/.cpp`, one
  `ComplexResonator` per channel (no summing, no chained crossover — the
  two things just ruled out), matched to the WHOLE target band (not 3
  sub-bands). The applied correction is `2 * (rotated.real -
  unrotated.real)`, added back onto the original (already-isolated)
  signal — not a replacement of it. At Cloak Depth 0% (`thetaRadians ==
  0`), `rotated == unrotated` exactly, so the correction is exactly zero
  BY CONSTRUCTION regardless of the resonator's own shape — the same
  "safe at the identity case" pattern BandIsolator already relies on,
  applied one level in. With only one filter in the signal path, there is
  nothing for it to interfere with — Gemini's specific "lumpy response"
  risk cannot arise here the way it could for a sum of three.
  - The `2 ×` factor is derived, not guessed: for small theta,
    `rotated.real - analytic.real` is a first-order approximation of
    `-theta * analytic.imag`; matching a true small-angle phase shift of
    `sin(w0 t)` — `sin(w0 t - theta) ≈ x - theta·cos(w0 t)` — exactly
    requires doubling this term, given a real input's amplitude splits
    into two equal phasors (so the resonator's real part alone only
    captures ~half of it). In practice this only holds approximately: the
    already-documented negative-frequency image leakage at low absolute
    centre frequencies (`ComplexResonator.h`'s own doc note) breaks the
    clean-single-phasor assumption, measured directly by
    `Tests/PhaseRotatorTests.cpp` at ~65% relative error against an ideal
    small-angle rotation — bounded and in the right direction, not exact.
    Documented honestly rather than asserting unrealistic precision.
- `PhaseRotator::setRotationRadians()` is smoothed internally via its own
  cos/sin pair (10ms ramp, renormalised each sample — the same unwrap-free
  technique `PhaseCorrelationAnalyzer` already uses), so a sudden jump in
  the measured phase delta (e.g. the Sidechain starting/stopping) doesn't
  click.
- `PluginProcessor::processBlock()`: `thetaRadians = phaseDeltaRadians ×
  (CloakDepth / 100)` (0 when there's no valid measurement), rotates
  `mainTargetBandBuffer` in place, then recombines with `mainResidualBuffer`
  exactly as before — Bypass now genuinely mutes something, since this is
  the first phase where the processed and dry paths can actually differ.
- Added `Tests/PhaseRotatorTests.cpp`: exact-transparency-at-zero (byte
  -for-byte, not just close, since this is guaranteed algebraically) and
  the small-angle accuracy check above. Full suite: 10272 assertions
  (the jump is mostly the transparency test's per-sample exact comparison
  across many blocks), 12 test cases, all passing.

## v0.3.1 — 2026-08-05 — Phase 3 Correction: the "Dummy All-Pass" Architecture

A THIRD design review, on v0.3.0's own resonator-based `PhaseRotator`, found
the small-angle accuracy fix wasn't good enough on its own — and led to the
actual final architecture, credited to Gemini.

- **The problem, quantified**: a review measured v0.3.0's real-world
  rotation error at ~65% relative to an ideal phase shift — unacceptable
  for a plugin whose entire purpose is phase alignment, and one that (per
  the review) would only get worse at large correction angles. Confirmed:
  it did. Replacing the resonator with `AllpassHilbertNetwork` (a proper
  two-branch interleaved allpass Hilbert transformer — magnitude-exact by
  construction, |H(f)|=1 always, no image leakage — reducing small-angle
  error to ~27-35%) still broke down catastrophically at large angles
  (112% error at 86 degrees, 155% at 172 degrees).
- **Root cause, found by directly measuring what the old formula assumed
  away**: the correction `2·(rotated - branchA)` added onto the RAW input
  assumed branch A is a clean stand-in for the input's own phase. It isn't
  — measured branch A's own phase relative to the raw input: **-24 degrees
  at 20Hz, +130 at 63Hz, -118 at 150Hz**. Wildly frequency-dependent, not
  small, not linear. Mixing a correction computed in that warped reference
  frame back into the UNwarped raw input is where the large-angle error
  came from — a completely different failure mode from the resonator's
  amplitude-leakage problem, exposed only once that first problem was
  fixed enough to reveal the second one underneath.
- **The fix ("Dummy All-Pass")**: stop referencing the raw input entirely.
  `BandIsolator`'s residual (the "High" content the target band is
  eventually recombined with) is now warped by `DummyAllpass` — an EXACT
  duplicate of branch A's own cascade (`AllpassHilbertNetwork::
  computeBreakpoints()` is now a shared static method specifically so the
  two copies can't drift apart), applied to a different signal with its
  own independent filter state. Because filtering is linear,
  `filter(Low) + filter(High) == filter(Low + High)` for the SAME filter on
  both — so `Ilow + Ihigh` stays exactly as flat as `BandIsolator`'s own
  already-proven flat-summing guarantee, REGARDLESS of the rotation angle,
  not just at theta=0. `PhaseRotator::process()` now takes and mutates both
  the target-band and residual buffers together (previously just the
  target band); `PluginProcessor::processBlock()` updated to match.
- **What changed for the three properties every review has cared about**:
  - *Transparency at Cloak Depth 0%*: `Rlow == Ilow` exactly when
    theta=0 (unchanged guarantee), so `output == Ilow + Ihigh` — a
    magnitude-flat allpass-warped version of the original, not
    bit-identical to it (that distinction, and why it's sonically
    irrelevant for static sub-bass phase shifts, is Gemini's own
    reasoning). True bit-exact passthrough remains a separate,
    already-implemented concern: the Bypass parameter's dry
    -buffer/crossfade, untouched by any of this.
  - *Accuracy*: re-measured by comparing output-at-theta against
    output-at-theta=0 (not against the raw input, which was the whole
    mistake) — small/medium angles (0.3-1.5 rad) match the requested theta
    within a documented, bounded margin tied to the network's own ~80-84
    -degree (not exactly 90) quadrature accuracy; near-exact-antiphase
    (~180 degrees) at the extreme band edge is a genuine numerically
    -degenerate corner, excluded from the sweep and documented as such
    rather than papered over with a huge tolerance.
  - *No comb-filtering*: verified directly —
    `Tests/PhaseRotatorTests.cpp`'s flat-recombination sweep now checks
    RMS gain across theta values from 0 to 1.5 rad AND across tones
    spanning both the target band and the 150Hz crossover into the
    residual, confirming the "Dummy All-Pass" guarantee holds where a
    naive design would show the worst comb-filtering.
- Full suite: 78 assertions, 14 test cases, all passing (the count drops
  from v0.3.0's 10272 because that number came almost entirely from the
  discarded delta-formula's per-sample byte-exact transparency test, which
  no longer applies the same way to a flat-magnitude-but-not-bit-identical
  guarantee).

## v0.4.0 — 2026-08-05 — Phase 4: Holographic Radar GUI

- New `Source/AcousticCloakLookAndFeel.h/.cpp`: dark-green phosphor
  "holographic radar / medical monitor" theme, per the blueprint's own
  Phase 4 concept -- a custom rotary-knob paint routine (glowing arc,
  phosphor green with an amber pointer) plus themed colours for
  buttons/labels, same division of labour as the sibling MorphicPhaser
  project's own LookAndFeel.
- New `Source/PhaseRadarComponent.h/.cpp`: the live phase-correlation
  radar. Rather than adding a new raw-waveform capture buffer (the sibling
  PhaseLockSub project's oscilloscope approach), this reads
  `PluginProcessor`'s ALREADY-published thread-safe atomics
  (`getDisplayCorrelation()`, `getDisplayPhaseDeltaRadians()`,
  `isDisplayCorrelationValid()`, `isSidechainConnected()`) directly --
  exactly the data this needs, no new DSP-side plumbing required. Angle =
  measured phase delta (12 o'clock = perfectly aligned); a short fading
  trail (small ring buffer of recent points) gives it a living,
  radar-sweep quality, with the dot's colour blending from phosphor green
  (good correlation) to amber (poor/negative) . Shows "NO SIDECHAIN
  CONNECTED" / "AWAITING KICK TRANSIENT..." when there's nothing valid to
  plot. Repaints from a 30Hz message-thread `Timer`, never the audio
  thread.
- New `Source/AboutPanel.h/.cpp` (same overlay pattern as the sibling
  MorphicPhaser/PhaseLockSub projects) and `Source/PluginEditor.h/.cpp`
  replace the `GenericAudioProcessorEditor` placeholder used since Phase
  1: header (wordmark + About button), the radar, and all 6 APVTS
  parameters as knobs (Cloak Depth large and central; Lookahead, Target
  Freq Low/High, Transient Sensitivity smaller) plus the Bypass toggle,
  all bound via the standard `SliderAttachment`/`ButtonAttachment` classes.
  All UI labels in English per the user's requirement. Fixed size
  (640x620) -- the radar's fixed-radius polar layout doesn't need
  resizing to stay legible.
- **Verified visually, not just by compiling**: built and launched the
  Standalone target, used computer-use to screenshot the running GUI --
  confirmed the layout renders as designed, the radar's grid/labels/
  "NO SIDECHAIN CONNECTED" state display correctly, all 5 knobs read live
  default parameter values, and the About panel opens showing the correct
  version string and a correctly-rendered signature (no mojibake, matching
  the sibling projects' own past lesson on this). Caught and fixed one
  real layout bug this way: the radar's "ALIGNED" label was drawn 16px
  above the outer ring, which exceeded the component's own top edge and
  got silently clipped by JUCE's automatic per-component clip region --
  invisible in the source, only visible on screen. Fixed by reserving a
  16px top margin before computing the grid's centre/radius.
- **Post-review polish, per direct user feedback on the first screenshot**:
  added a soft phosphor-glow bloom to every outline in the UI -- the
  radar's outer ring and axis lines (multi-pass widening low-alpha
  strokes, the same technique the knobs' value arc already used), plus a
  value-proportional radial halo behind each knob (brighter as the knob is
  turned up, using a `ColourGradient` radial fill) and a matching glow on
  the Bypass/About button outlines. Verified visually: at Cloak Depth
  100% the halo is clearly brighter than at Lookahead's default 0ms.

## v1.0.0 — 2026-08-05 — Closing Release: Phase 5 Complete

All 5 blueprint phases complete, tested, and packaged with EN/HU PDF
overviews and an NFO — the condition this file's own versioning rule (top
of this document) reserves `1.0.0` for.

- **Real CPU profiling, not guesswork**: added `Tests/LoadTest.cpp`, a
  standalone console app (built Release — a Debug build's numbers are
  meaningless noise) that runs `BandIsolator` (Main + Sidechain),
  `PhaseCorrelationAnalyzer`, and `PhaseRotator` in the same order
  `PluginProcessor::processBlock()` does, on synthetic audio with periodic
  Kick-like transients, for a fixed 15-second window. **Measured: 0.522%
  estimated single-instance CPU at real-time playback (48kHz/512-sample
  blocks), 191.7x faster than real-time.**
- Full regression suite re-run at this exact version before closing: 78
  assertions across 14 test cases, all passing.
- **Deliverables now live in the project's own `Build/` folder**: Release
  builds of `Build/VST3/Acoustic Cloak.vst3` and
  `Build/Standalone/Acoustic Cloak.app` (previous phases' builds were
  Debug; these are the first Release-configuration binaries). The
  installed copy at `~/Library/Audio/Plug-Ins/VST3/Acoustic Cloak.vst3`
  was also updated to match, for DAW testing.
- **`Gabcis_AcousticCloak_Overview_EN.pdf`** and
  **`Gabcis_AcousticCloak_Attekintes_HU.pdf`** added to the project root:
  professionally styled 4-page product overviews (built as HTML rendered
  to PDF via headless Chrome, `--headless=new --no-pdf-header-footer` —
  the same method the sibling PhaseLockSub project used, refined here to
  actually suppress Chrome's own page header/footer, which the older
  `--print-to-pdf-no-header` flag didn't). Each covers: the core
  ducking-vs-phase-rotation concept, an inline SVG architecture diagram
  (`docs/architecture_diagram.svg`, hand-built — not a screenshot — showing
  the Main/Sidechain signal paths, the `PhaseRotator`'s internal
  `AllpassHilbert`/`DummyAllpass` split, and where the Holographic Radar's
  data comes from), a table of the 5 architectures considered and why 4
  were rejected (with their actual measured error numbers), and a
  measured-not-assumed quality/performance section (the CPU number above,
  test counts, `BandIsolator`/`AllpassHilbertNetwork` accuracy figures,
  and lines-of-code counts).
- **`AcousticCloak.nfo`** added to the project root: a classic scene-style
  NFO with a deterministically-generated (small Python block-letter font
  script, not hand-drawn — avoids the transcription errors a hand-crafted
  ASCII banner risks) "ACOUSTIC CLOAK" header, plus a condensed
  description, parameter table, key technology list, measured quality
  figures, sources/references, creator credits, and exact code-size counts
  (`Source/`: 2143 lines across 23 files; `Tests/`: 690 lines across 6
  files; 2833 total).
- **Windows 11 x64**: not cross-compiled from this Mac (no reliable
  MSVC cross-toolchain available here) — the project's `CMakeLists.txt` is
  already platform-generic (JUCE 9 `FetchContent`, `FORMATS VST3
  Standalone` on non-Apple platforms, no macOS-only APIs anywhere in
  `Source/`), so it's expected to configure and build natively on Windows
  with a standard CMake + Visual Studio toolchain — documented in both PDF
  overviews' System Requirements section, but not verified on actual
  Windows hardware in this session.
- Project version bumped to 1.0.0 in both `CMakeLists.txt`
  (`project(AcousticCloak VERSION 1.0.0 ...)`) and
  `PluginProcessor::kVersionString`, so the in-app About panel and the
  PDF/NFO version references all agree.

**Summary of what shipped:** a real-time Kick/Bass phase-alignment VST3
/Standalone plugin built entirely around measured engineering decisions —
three independent design-review cycles each caught a real, quantified flaw
(STFT impracticality, resonator-bank magnitude "lumpiness," nested
-crossover interference, single-resonator ~65% rotation error) before
converging on a Linkwitz-Riley band isolation + allpass Hilbert transformer
+ "Dummy All-Pass" recombination architecture with exact magnitude
preservation at any rotation angle, a live Holographic Radar GUI, 0.52% CPU
at real-time playback, and a 78-assertion regression suite — all
documented, in both English and Hungarian, with the actual numbers instead
of marketing claims.
