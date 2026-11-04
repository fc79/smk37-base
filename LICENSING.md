# Licensing

SMK BASE, like the [Dinghy](https://github.com/zvenson/dinghy) it comes from, is free software under the GNU General
Public License, version 3 only (`GPL-3.0-only`, full text in `LICENSE`). You may use, study, change and share it. If you give someone a firmware built from it, give them its
complete source under the same licence too.

## Who made what

| | |
| --- | --- |
| The platform: `firmware/hal/`, `firmware/loader/`, `crt0.S`, `app.ld`, `firmware/src/` (`usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `gfx.c`, `libc.c`, `bootguard.h`, `uac_fir.h`), `tools/` | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca)), with the changes of [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) |
| The SMK-37 board layer: `firmware/hal/smk_input.h`, `fm1_flash_map.h`, the SMK-37 parts of `fm1_cc.h`, `fm1_flash.h`, `fm1_lcd_hw.h`, `lcd.c`, `ota.c`, `bootguard.h`, `firmware/loader/`, `tools/fm1pkg_make.py`, `tools/fm1_install.py`, `tools/build.py`; `firmware/src/smk_wheel.h`; `tests/ota_test.c`, `ldr_test.c`, `install_test.py`, `smk_wheel_test.c`, `smk_pkg_test.py` | Copyright (C) 2026 M. Geisler (genkigenki/makkuro), from [genkigenki/sloop-fm1](https://github.com/genkigenki/sloop-fm1) branch `smk37` (the reference port of the [fm1-to-smk37](https://github.com/genkigenki/fm1-to-smk37) kit) |
| The FM-1's TRS MIDI IN `firmware/src/midi_uart.c` (not used on the SMK-37) | Felucca's, as sloopDX carries it, with the ring read by content from Felucca [Salt] 0.9-salt14 by ChanceTheMaker (found by keremimo) |
| The update loader's screen: `firmware/loader/ldr_screen.c`, `tests/ldr_screen_test.c` | Copyright (C) 2026 zvenson (from OberVave, the Firmware Hub's look); the idea, a loader that draws, its 5 x 7 letters and the order of things from [Skiff](https://github.com/illuminus5/skiff) by illuminus5, GPL-3.0 |
| From OberVave: the storage (blobs, slots), `LCD_IDLE`, the SMK-37 wiring of `platform.c`, `store.h`, the pad event queue in `smk_input.h`, `tests/smk_flash_test.c`, `tests/run_tests.sh`, the CI workflow | Copyright (C) 2026 zvenson |
| [Dinghy](https://github.com/zvenson/dinghy): its app (`dinghy.c`, from which `firmware/src/app.c` comes), the wiring `platform.c`, the host mode `tests/`, `docs/API.md` and `docs/RULES.md` | zvenson |
| SMK BASE: `firmware/src/app.c` (Dinghy's app for the SMK-37, with the controls on the screen), the pad changes in `firmware/hal/smk_input.h` (re-arm, peak, crosstalk, velocity), `SMK_MASTER_FROM_APP`, `fl_slice_ram` and the sliced saves in `firmware/hal/fm1_flash.h` and `platform.c` (from ciapibox, fc79's FM-1 groovebox, GPL-3.0), the blob records (`blobs_load` / `blobs_save`), the host's `expect` lines, `tools/backup/`, `tests/smk_pad_test.c`, `tests/scripts/diag.txt`, `midi.txt`, `README.md`, `docs/CHANGES.md`, `docs/BACKUP.md`; the changes listed in `docs/CHANGES.md` | Copyright (C) 2026 fc79 |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Terminus font 8x16 (ter-u16n), rendered into `firmware/gen/felucca_font.h` | SIL OFL 1.1 | `assets/fonts/Terminus-LICENSE.txt` |
| `tools/backup/jl-uboot-tool-macos/`: a macOS SCSI port and a patch for [jl-uboot-tool](https://github.com/kagaimiq/jl-uboot-tool) (Copyright (c) 2023 Andrey Grigoryev, kagaimiq), which they extend | MIT, as jl-uboot-tool | the files' headers |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## The logo

Dinghy's logo (the designburg signet, all rights reserved, not under the GPL) is not in this repository: it marks
Dinghy itself, and a fork uses its own. SMK BASE has none.

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments; "M-VAVE", "FM-1", "SMK-37" and "MKE-P37"
belong to their owners. Dinghy and SMK BASE are not made or endorsed by them.
