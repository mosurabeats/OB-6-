# Loading on the MPC Key 37 (firmware 3.9.1, USB)

Stock MPC firmware cannot run third-party synth code, so the clone reaches
the MPC Key 37 as **Keygroup programs**. `ob6render` plays every patch
through the OB-6 engine on your computer, records multisamples (with
seamless sustain loops), and writes an `.xpm` program next to the WAVs. The
MPC loads these from a USB drive like any other keygroup program. No mods,
no root access, and nothing on the MPC's system partition is touched.

## 1. Render

```sh
cmake -S . -B build && cmake --build build
./build/ob6render calibrate -o out/OB6-Clone            # once, see step 3
./build/ob6render keygroup patches/*.ob6 -o out/OB6-Clone
```

Each patch becomes `out/OB6-Clone/<Patch Name>/` holding `<Patch Name>.xpm`,
its WAVs (`<Patch Name>_<midi>_<note>.wav`, Akai naming: MIDI 60 = C3) and
a copy of the source `.ob6` patch for reference. The MPC ignores the
`.ob6` file.

Useful options:

| Option | Default | Notes |
|---|---|---|
| `--low/--high/--step` | 24 / 96 / 3 | Sampled roots. Each sample stretches at most ±2 semitones. |
| `--layers N` | 1 | 1–4 velocity layers, each rendered at that layer's top velocity. |
| `--loop auto\|on\|off` | auto | `auto` loops patches whose amp sustain is above 0 and renders plucks as one-shots. |
| `--loop-seconds S` | 2 | Longer loops hide LFO and drift repetition; they cost more space. |
| `--bits 16\|24`, `--rate` | 24, 44100 | 44.1 kHz matches the MPC engine. |
| `--mono` | off | Halves the size; FX stereo is lost. |
| `--root-offset N` | 1 | See calibration. |

A full patch (25 zones, stereo, 24-bit) is about 17–25 MB.

## 2. Copy to USB

1. Use a FAT32 or exFAT formatted USB drive.
2. Copy the whole `out/OB6-Clone` folder to the drive root. Keep each
   program's WAVs in the same folder as its `.xpm`, because the program
   finds its samples by name in its own folder.
3. Eject the drive safely, then plug it into the MPC Key 37's USB-A port.

## 3. Load and calibrate (do this once)

1. On the MPC, open **Browse**, select the USB drive, open
   `OB6-Clone/OB6 Calibrate/` and load `OB6 Calibrate.xpm`.
2. Put a tuner on the output and play C1, C2, C3, C4 and C5. The expected
   frequencies are listed in `CALIBRATE.txt` (C3 = MIDI 60 = 261.63 Hz).
3. If every key is in tune, the defaults are right. If every key is
   exactly one semitone off, Akai's undocumented `RootNote` offset differs
   from ours. Re-render everything with the `--root-offset` value that
   `CALIBRATE.txt` suggests.

Calibration exists because the XPM format is not publicly documented. It
checks the one mapping that would silently detune every program.

## 4. Play

Load any `OB6 <Name>.xpm` from the same folder. Once loaded, the programs
behave like ordinary keygroup programs. You can save them into a project,
copy them to internal storage, and route them through MPC effects.

## What is rendered and what the MPC does

| Feature | Where it lives |
|---|---|
| Oscillators, sync, X-Mod, mixer, filter + filter envelope, attack/decay of the amp envelope, LFO, vintage slop, chorus/delay | Rendered into the samples |
| Note release | MPC amp envelope (`VolumeRelease`, set from `aenv.release`) |
| Polyphony, pitch bend range, glide, mod wheel, aftertouch | Set on the MPC program. Unison patches are flagged `Mono`. |
| Sequencing, arpeggiator, Q-Link tweaks | MPC |

Known limitations of the sampled approach:

- The filter envelope's release movement and the delay tail after
  note-off are lost, because the MPC fades the looped sustain instead.
  Use MPC delay/reverb for tails.
- LFO and drift motion repeat every `--loop-seconds`.
- The MPC's own filter is disabled (`FilterType` 0) so it does not stack on
  the rendered OB-6 filter. You can still turn it on per program.

## Unverified on hardware (please report back)

These were written from the community-known XPM layout and have not yet
been checked on a Key 37 running 3.9.1:

- That 3.9.1 loads the 2.x-format XPM without complaint, including the
  elements we leave out for the MPC to default.
- The `RootNote` offset. Calibration checks it.
- The `VolumeRelease` curve. `mpcEnvelopeTime()` in
  `tools/ob6render/xpm.cpp` uses a log approximation, so release times may
  feel slightly long or short.
- That loop points are taken from the XPM layer tags. They are also
  embedded as a WAV `smpl` chunk as a fallback.

## Live (non-sampled) path, later

The engine in `engine/` is plain C++17 with no allocation in `process()`.
It can therefore also run as a real-time instrument with MIDI CC control
(see `docs/PARAMETERS.md`). Running it on the MPC itself would need a
community-modified OS that allows user binaries, plus a cross-compile for
the MPC's ARM CPU. That is outside stock 3.9.1, so it is a roadmap item and
not the default path.
