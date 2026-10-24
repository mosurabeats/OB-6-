# OB-6 Clone for MPC Key 37

A clean-room, OB-6-style polyphonic synth engine, plus a tool that turns its
patches into multisampled **Keygroup programs** for the Akai MPC Key 37.
The programs load from a USB drive on stock firmware 3.9.1.

This is an independent project inspired by the Sequential/Oberheim OB-6. It
does not use Sequential code, samples or presets.

## What's here

| Path | Contents |
|---|---|
| `engine/` | Portable C++17 synth engine, no dependencies, allocation-free audio path |
| `tools/ob6render/` | `ob6render` CLI: patch → `.xpm` + looped WAVs, calibration, demo render |
| `patches/` | Starter patches (`.ob6` text files) |
| `tests/` | Engine and renderer tests |
| `docs/MPC_KEY37.md` | **USB loading and calibration on the MPC** |
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
browser. [docs/MPC_KEY37.md](docs/MPC_KEY37.md) has the full walkthrough,
including the one-time tuning check.

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

Working and tested on desktop: the engine, rendering, loop crossfades
(seams measured below one normal sample step), the XPM writer, and tuning
to within 1 cent.

Not yet verified on a Key 37: see "Unverified on hardware" in
`docs/MPC_KEY37.md`.

Next steps:

1. On-device check of the XPM on 3.9.1 (root offset, release curve, loops).
2. Round-robin layers that capture the vintage slop note-to-note.
3. Q-Link assignments in the XPM, and an "open filter" render mode that
   hands cutoff/resonance to the MPC filter for live tweaking.
4. Higher-order BLEP or oversampling to cut high-note aliasing further
   (currently −28 dB at ~2.5 kHz, textbook polyBLEP).
5. `.xpn` expansion packaging, more OB-6-style FX, and an arp/sequencer
   for the live engine.
6. A real-time host (MIDI in/audio out) for desktop, and for the MPC if a
   community-modified OS is used.
