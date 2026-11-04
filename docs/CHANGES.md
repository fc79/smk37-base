# Changes from Dinghy

SMK BASE starts from [Dinghy](https://github.com/zvenson/dinghy) (zvenson), `main` at commit `22acb91`. This is what
is different.

## The app

- `firmware/src/dinghy.c` is now `firmware/src/app.c`, for the SMK-37 only: the FM-1's branches are gone.
- Kept from Dinghy: the 4-voice sine on the keys with velocity and on the outer keys, the pads as notes (glowing while
  they sound), the pitch wheel's bend, the octave on OCTAVE- / OCTAVE+ kept in flash.
- New: the screen shows the last control touched by its hardware id, the encoders' positions, the faders and the
  wheels (Dinghy: the last note, a keyboard, the MIDI source and clock), a check of every control on a unit; every
  button's LED lit while it is held.
- MIDI in on any channel, out on channel 1. Gone: the MIDI channel setting (PRESETS) and the clock's tempo display.
- Its own name and data: `APP_NAME` "SMK BASE", `STORE_MAGIC` "SB37", `DATA_FAMILY` "SMKBASE".

## The SMK-37's pads (`firmware/hal/smk_input.h`)

Measured and tuned on an SMK-37 Pro (`tests/smk_pad_test.c` covers it):

- **One hit per strike.** After a release, a new hit only after 8 quiet readings in a row (~19 ms) or 400 ms; the
  release itself after 4 low readings (Dinghy: 2). The rubber's rebound and a resting finger no longer hit twice.
- **The peak** is followed while the reading still rises, up to 8 readings (~17 ms; Dinghy: a fixed 3, which caught
  slow strikes still rising).
- **Crosstalk:** a hit that starts within ~14 ms of another pad's and stays under a third of its height is that
  pad's echo, and is dropped (a bottom pad hit also hit the top pad on the same multiplexer address).
- **Velocity:** straight from where a hit starts to a reading of 1000 (a hard hit), instead of the factory record's
  curves, which end at 950 and put every hard hit at 127.

## The platform

- `SMK_MASTER_FROM_APP`: 1 gives K1 to the app, which then sets the volume itself (default 0: K1 is the volume).
- `gfx.c`: the host build on macOS (no `.pool` section there).
- The flash chip of newer units, UC25HQ80 (`0xB36014`), is accepted next to the older parts, by the app and the update
  loader (`FL_JEDEC_OK`, `hal/fm1_flash_map.h`); not yet tried on such a unit.
- Saving while the sound plays: `store_sliced = 1` erases and writes in slices of 400 us, the flash suspended
  between them (`fl_slice_ram`, `fl_sliced`, `fl_write_sliced` in `hal/fm1_flash.h`, from ciapibox); tried on an
  FM-1, not yet on an SMK-37.
- `blobs_load` / `blobs_save`: one record over several blobs, of the length it had when saved.
- The FM-1's code paths in `platform.c`, `firmware/hal/` and `firmware/loader/` are kept, unchanged, and not built.

## Build and packages

- Only the SMK-37: no `--board`; `--model pro|elite|mke` stays.
- The package is `build/smk37-base.fwsc` (`-elite`, `-mke`), the app `build/app.bin`.
- The identity is `SMK-37 Pro_925` (`SMK-37 Elite_925`, `MKE-P37_925`) in every build; a release changes only the
  version on the screen. Dinghy's is `_99N`, N the minor version.
- The release workflow builds and publishes the three SMK-37 packages.

## Tests

- SMK-37 only: the FM-1 app, its TRS MIDI and its update tests are gone.
- New: `tests/smk_pad_test.c` (the pads), `tests/scripts/diag.txt` (every kind of control once),
  `tests/scripts/midi.txt` (USB MIDI in and out).
- Host scripts can check the app's state: `T expect V NAME [A B C D]`, answered by the app's `host_probe`; a run with a
  wrong value exits 1.

## Tools

- `tools/backup/`: a backup of the whole flash over the mask-ROM bootloader without writing anything
  (`uboot_key.py`, `verify_dump.py`, a macOS port for jl-uboot-tool): [BACKUP.md](BACKUP.md).

## Documentation

- New README, this file, [BACKUP.md](BACKUP.md); [API.md](API.md) and [RULES.md](RULES.md) for the SMK-37 only.
- Dinghy's README, `MAKE-IT-YOURS.md` and `SMK37.md` are not here: their parts that apply are in the README and API.md.
- Dinghy's logo is not here: it is not under the GPL.
