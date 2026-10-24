# MPC Key 37 (firmware 3.9.1): installing the synth

There are two ways to get the synth onto the MPC:

| | A. Native plugin (recommended) | B. Keygroup programs |
|---|---|---|
| What runs on the MPC | The real engine, live, as a VST2 instrument | Multisamples rendered on your computer |
| Knobs, Q-Links, presets, automation | Yes, every parameter | Only MPC's own keygroup controls |
| Needs | [sd88me/mpc-vst-plugins](https://github.com/sd88me/mpc-vst-plugins) install route (USB stick on 3.9.1) | Nothing: stock firmware, load from USB |
| Status | Port passes the project's host test on x86 and aarch64; **not yet run on a device** | Rendered and checked on desktop; **not yet loaded on a device** |

**Stay on MPC OS 3.9.1.** sd88me documents that 3.10.0 removes the USB-stick
root route that path A installs through (`docs/USB_STICK.md` in their repo).

---

## A. Native plugin ("HexaSV") via sd88me/mpc-vst-plugins

`mpc/` is a port for sd88me's open MPC plugin host. MPC OS loads native VST2
`.so` plugins with their own touchscreen skins. `mpc/src/mpc_engine.cpp`
implements that project's `mpc_engine()` interface on top of `engine/`. The
plugin is called **HexaSV**, a generic name, following the project's rule
that plugin names and skins do not use other makers' names.

What you get on the MPC:

- 6 skin pages (Osc, Mixer/Filter, Envelopes, LFO/X-Mod, Voice, FX). Each
  page's Q-Links follow the page.
- All 63 parameters, automatable and saved with the project.
- The 8 patches from `patches/` in MPC's PRESET menu.
- Note starts are sample-accurate, so sequenced notes land on their exact
  frame. Pitch bend, mod wheel, aftertouch and sustain pedal all work.
- Builds for **armv7** (Gen1 MPCs and Force) and **aarch64** (Gen2). The
  installer picks the right one with `uname -m`.

### 1. Build the plugin zips

The supported build is sd88me's `tools/build_port.sh`, which needs Docker with
ARM emulation. The easiest route is GitHub Actions:

1. Go to **Actions → MPC plugin → Run workflow** and enter a version.
2. When the run finishes, download the artifact. It holds
   `…-mpc-armv7.zip`, `…-mpc-aarch64.zip` and previews of the skin pages.

To build locally instead, with Docker:

```sh
git clone https://github.com/sd88me/mpc-vst-plugins ../mpc-vst-plugins
../mpc-vst-plugins/tools/test_port.sh mpc/vst.json      # offline host test, prints PASSED
../mpc-vst-plugins/tools/build_port.sh mpc/vst.json     # mpc/build/hexasv.so, mpc/build/aarch64/hexasv.so, skin
```

Then package the build with their `tools/release.py`, following their
`docs/RELEASING.md`.

### 2. Install on the Key 37 (USB stick, no SSH setup beforehand)

On 3.9.1, MPC OS runs a script from a USB stick after asking you on screen.
sd88me's tools use this to get root access without a hack. Follow their
`docs/USB_STICK.md`; in short:

1. Download the official **MPC 3.9.1 update image** for your model. Their
   `make_stick.py`, or the desktop installer app's "Plugin install" mode,
   reads a key from your own firmware to sign the stick.
2. Build a FAT32 stick with SSH enabled, using
   `make_stick.py --firmware <update.img> --out <stick> --ssh-key ~/.ssh/id_ed25519.pub`.
   On Gen2 the SSH login lasts until the next reboot. Re-insert the stick to
   get it back.
3. Insert the stick into the MPC and accept the prompt.
4. On your computer, run their desktop installer app
   (`tools/desktop`, or the installer from their releases). Connect to the
   MPC over SSH, drop in the HexaSV zip, and install. MPC restarts once.
5. HexaSV then appears in the plugin browser.

Back up first. The installer keeps `MPC.settings` backups, but save your
project before installing because MPC restarts.

### 3. Test it, then tell us

There is no device run yet, so please work through sd88me's checklist (their
`docs/PORTING.md` §4):

1. Find the plugin in the list.
2. Insert it on a track.
3. Play it.
4. Check every skin page.
5. Turn the Q-Links (also from track edit → automation).
6. Save and reload a project.

Note anything that sounds or looks off. CPU use has not been measured on
hardware yet; sd88me's `tools/bench.sh` does that on the device.

### Keeping the port in sync

`mpc/params.json` and `mpc/presets.json` are generated from the engine's
parameter table and `patches/` by `mpc/regen.sh`. CI fails if they are
stale. `mpc/params.lock` pins every parameter's index, because saved
projects bind to it. Add new parameters at the **end** of `OB6_PARAMS`, and
never reorder.

---

## B. Keygroup programs (stock firmware, no root)

`ob6render` plays every patch through the engine on your computer and
records multisamples with seamless sustain loops. It then writes an `.xpm`
Keygroup program next to the WAVs, which loads from USB like any other
program.

### Render

```sh
cmake -S . -B build && cmake --build build
./build/ob6render calibrate -o out/OB6-Clone            # once, see "Calibrate"
./build/ob6render keygroup patches/*.ob6 -o out/OB6-Clone
```

Each patch becomes `out/OB6-Clone/<Patch Name>/`, holding
`<Patch Name>.xpm`, its WAVs (Akai naming: MIDI 60 = C3), and a copy of the
`.ob6` patch, which the MPC ignores.

| Option | Default | Notes |
|---|---|---|
| `--low/--high/--step` | 24 / 96 / 3 | Sampled roots. Each sample stretches at most ±2 semitones. |
| `--layers N` | 1 | 1–4 velocity layers. |
| `--loop auto\|on\|off` | auto | `auto` loops patches with amp sustain above 0. |
| `--loop-seconds S` | 2 | Longer loops hide LFO and drift repetition. |
| `--bits 16\|24`, `--rate` | 24, 44100 | |
| `--mono` | off | Halves the size. |
| `--root-offset N` | 1 | See "Calibrate". |

A full patch is about 17–25 MB.

### Copy and load

1. Copy `out/OB6-Clone` to the root of a FAT32 or exFAT USB drive. Keep
   each program's WAVs in its folder.
2. On the MPC, open **Browse**, choose the drive, and load an `.xpm`.

### Calibrate (once)

The XPM format is undocumented, so check its root-note mapping once:

1. Load `OB6 Calibrate/OB6 Calibrate.xpm`.
2. Play C1 to C5 with a tuner on the output. The expected frequencies are
   in `CALIBRATE.txt`.
3. If every key is exactly a semitone off, re-render everything with the
   `--root-offset` value suggested in that file.

### Limits of the sampled approach

- **MPC handles release.** The MPC amp envelope fades the looped sustain
  (`VolumeRelease`, set from `aenv.release` on an approximated curve). The
  filter release sweep and delay tails after note-off are lost.
- **Motion repeats.** LFO and drift repeat every `--loop-seconds`.
- **MPC filter off.** The MPC filter is disabled (`FilterType` 0) so it
  doesn't stack on the rendered one.
- **Not yet confirmed on hardware:**
  - that 3.9.1 accepts the 2.x-format XPM;
  - the `RootNote` offset;
  - the release curve;
  - loop points read from the XPM (they are also embedded in each WAV's
    `smpl` chunk).
