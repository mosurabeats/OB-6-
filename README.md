# OB-6 Clone for MPC Key 37

A clean-room, OB-6-style polyphonic synth engine for the Akai MPC Key 37
(MPC OS 3.9.1), in two forms:

- **HexaSV**, a native MPC OS plugin built on
  [sd88me/mpc-vst-plugins](https://github.com/sd88me/mpc-vst-plugins). The
  real engine runs live on the MPC, with a touchscreen skin, Q-Links and
  presets. It is installed through that project's USB-stick route on 3.9.1.
- **Keygroup programs** rendered on your computer, which load from a USB
  drive on stock firmware with no root access.

This is an independent project inspired by the Sequential/Oberheim OB-6. It
does not use Sequential code, samples or presets.

## What's here

| Path | Contents |
|---|---|
| `engine/` | Portable C++17 synth engine, no dependencies, allocation-free audio path |
| `mpc/` | MPC OS plugin port: `mpc_engine()` adapter, `vst.json`, generated params/presets |
| `tools/ob6render/` | `ob6render` CLI: patch → `.xpm` + looped WAVs, calibration, demo render, port file generation |
| `patches/` | Starter patches (`.ob6` text files) |
| `tests/` | Engine and renderer tests |
| `docs/MPC_KEY37.md` | **Installing on the Key 37: plugin or keygroups** |
| `docs/PARAMETERS.md` | Every patch parameter, range and MIDI CC |

### Engine (per voice, 6 voices)

- **2 VCOs** with band-limited (polyBLEP) shape morph triangle → saw →
  pulse, pulse width, and hard sync (osc 1 slaved to osc 2). Osc 2 adds
  fine tune, low-frequency mode and key-track off.
- **Sub-oscillator** (square, −1 octave) and **white noise** in the mixer.
- **SEM-style 2-pole state-variable filter.** The mode morphs continuously
  LP → notch → HP, there is a separate band-pass switch, and resonance is
  soft-saturated and never self-oscillates. Envelope amount is bipolar,
  with key tracking and velocity.
- **Filter and amp ADSRs** with analog-style exponential curves.
- **LFO** (sine, triangle, saw, ramp, square, S&H) routable to osc 1/2
  pitch, PW, cutoff, filter mode and amp. The mod wheel and aftertouch add
  depth.
- **X-Mod**: the filter envelope and osc 2 (audio-rate) modulate osc 1
  pitch, shape and PW, the cutoff, and the filter mode.
- **Unison** (1–6 voices, detune spread, last-note legato), glide, pitch
  bend, sustain pedal, and a **vintage** knob for per-voice tuning, filter
  and envelope tolerances plus drift.
- **Stereo chorus** and **ping-pong delay**, and AC coupling on the output.

## Quick start

```sh
cmake -S . -B build && cmake --build build
ctest --test-dir build                                   # 20 tests
./build/ob6render calibrate -o out/OB6-Clone
./build/ob6render keygroup patches/*.ob6 -o out/OB6-Clone
./build/ob6render demo patches/brass.ob6 -o brass.wav    # audition on desktop
```

Copy `out/OB6-Clone` to a USB drive and load the `.xpm` files from the MPC
browser.

The plugin zips (armv7 + aarch64) come from the **MPC plugin** GitHub
Actions workflow, or from `tools/build_port.sh mpc/vst.json` in a checkout
of sd88me/mpc-vst-plugins. [docs/MPC_KEY37.md](docs/MPC_KEY37.md) covers
both install routes.

## Writing patches

Patches are plain text. Any parameter you leave out keeps its default:

```ini
name = My Brass
osc2.fine = 0.12          # semitones
filter.cutoff = 0.42
filter.envamt = 0.42
fenv.attack = 0.38        # 0..1 -> 1 ms .. 20 s
lfo.shape = triangle
osc1.sync = on
```

`ob6render dump <patch>` prints a patch with every parameter filled in.
`ob6render params` lists them all.

## Status and roadmap

Working and tested off-device:

- **Engine:** pitch is within 1 cent and polyBLEP aliasing is at
  textbook level.
- **Plugin port:** passes sd88me's `test_port.sh` with all 163 checks,
  under ASan/UBSan on x86 and under UBSan as an aarch64 binary in QEMU. The
  aarch64 `.so` links with `--no-undefined`, exports only `VSTPluginMain`
  and needs glibc ≤ 2.38.
- **Keygroup renderer:** loop seams measure below one normal sample step.

Nothing has run on a Key 37 yet; see the status column in
`docs/MPC_KEY37.md`.

Next steps:

1. First device run of HexaSV: sd88me's checklist, plus a CPU bench with
   `tools/bench.sh`.
2. A hand-made skin layout (`mpc/layout.conf`) to replace the auto-layout,
   with the artwork tile for the Instruments browser.
3. A catalog listing in mpc-vst-plugins (needs a licence for this repo).
4. Higher-order BLEP or oversampling to cut high-note aliasing further.
5. Arp/sequencer and more OB-6-style FX (reverb, phaser).
6. On-device check of the keygroup XPM (root offset, release curve, loops).
