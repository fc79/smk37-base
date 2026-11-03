# API

Everything [platform.c](../firmware/src/platform.c) gives your app. One compilation unit: `app.c` includes
`platform.c` first, so all of this is just there. Nothing here is a library you link against; read the source
when in doubt, it is short. The names start with `fm1_`: the platform comes from the M-Vave FM-1, which has the same
chip, and the SMK-37 keeps its calls.

- [The contract](#the-contract)
- [Time](#time)
- [Keys, pads, buttons, encoders, faders, wheels](#keys-pads-buttons-encoders-faders-wheels)
- [LEDs](#leds)
- [Screen](#screen)
- [Sound](#sound)
- [MIDI](#midi)
- [Storage: settings, blobs, slots](#storage-settings-blobs-slots)
- [Memory and flash](#memory-and-flash)
- [Build options](#build-options)

## The contract

Your app supplies two functions:

```c
static void app_audio(int32_t *out, uint32_t n);   /* the audio interrupt: fill n stereo frames */
static void app_main(void);                        /* your main loop; never returns */
```

`app_main` must call **`platform_poll()`** often: every few milliseconds, and never less than once every
couple of seconds. It feeds the watchdog (8 s, then the keyboard resets), runs USB (MIDI, the updater, a computer
asking for the update mode) and turns K1 into the volume. A long job (drawing a lot, saving) is fine; a loop that
waits for something must poll.

```c
static void app_main(void)
{
    /* set up */
    for (;;) {
        platform_poll();
        /* read the panel, MIDI; draw what changed */
    }
}
```

Names you can set before `#include "platform.c"`: `APP_NAME` (the rescue and crash screens), `STORE_MAGIC` (your
data, see [Storage](#storage-settings-blobs-slots)). `APP_VER` comes from `./build.sh --release`, `FELUCCA_ID` (the
package identity) from `tools/build.py`.

## Time

| | |
| --- | --- |
| `fm1_ms` | milliseconds since power-on (`volatile uint32_t`, counted by the 10 kHz timer) |
| `fm1_ticks()` | a free-running 24 MHz counter: `FM1_TICKS_PER_US` (24) per microsecond; wraps in ~179 s, so subtract: `fm1_ticks() - t0` |
| `fm1_micros()` | microseconds, from the same counter |
| `fm1_delay_ms(ms)`, `fm1_delay_us(us)` | busy waits; keep them short (they do not poll) |

To time your audio code: `t0 = fm1_ticks()` before, `(fm1_ticks() - t0) / FM1_TICKS_PER_US` after.

## Keys, pads, buttons, encoders, faders, wheels

The panel is scanned in the background and debounced.

| | |
| --- | --- |
| `fm1_in.notes` | the middle 27 keys held now: bit 0 = F3 ... bit 26 = G5 (keybed keys `SMK_KEY_F3` (5) .. 31) |
| `fm1_input_note_edges()` | those keys pressed since the last call (bits): a press shorter than a loop pass still counts |
| `fm1_in.key_vel[key]` | the velocity (1..127) of a keybed key's last press (key 0..36, 0 = the lowest C), from the unit's factory curves |
| `smk_extkey_take()` | the outer keys (C3..E3, G#5..C6): `key << 8 \| velocity` (0 = let go, key 0..36), -1 = none |
| `fm1_in.keys` | all 37 keys held now (64 bits, bit 0 = the lowest C) |
| `fm1_in.buttons` | the 18 buttons held now: bit = `BTN_...` |
| `fm1_input_edges(&released)` | buttons **pressed** since the last call (bits), and those released into `released` |
| `fm1_enc_take(ENC_...)` | detents turned since the last call, + = clockwise |
| pads | by default the 16 white keys F3..G5 (they set `fm1_in.notes`); `smk_pad_events = 1` makes them events instead: `smk_pad_take()` = `pad << 8 \| velocity` (0 = let go), -1 = none. Pads 0..7 are the top row, 8..15 the bottom row |
| `fm1_in.pad_press[pad]` | a held pad's pressure, 0..127 |
| `fm1_in.fader[0..3]` | the faders, 0..127 |
| `fm1_in.wheel[0]`, `[1]` | the pitch and the mod wheel, raw 0..1023; `smk_pw_bend()` ([smk_wheel.h](../firmware/src/smk_wheel.h)) makes the pitch wheel a self-calibrating bend |

Buttons (ids 0..17), by the names printed on the panel:

| id | | panel | id | | panel |
| --- | --- | --- | --- | --- | --- |
| 0 | `BTN_KNOB_BANK` | KNOB BANK | 9 | `BTN_ENV` | CHORD |
| 1 | `BTN_FADER_BANK` | FADER BANK | 10 | `BTN_GLO` | GLOBE |
| 2 | `BTN_LEFT` | < | 11 | `BTN_SAVE` | BT |
| 3 | `BTN_RIGHT` | > | 12 | `BTN_HOME` | PATCH |
| 4 | `BTN_OCT_DOWN` | OCTAVE- | 13 | `BTN_EDIT` | PARA |
| 5 | `BTN_OCT_UP` | OCTAVE+ | 14 | `BTN_FX` | FX |
| 6 | `BTN_ARP` | ARP | 15 | `BTN_SEQ` | SEQ |
| 7 | `BTN_LFO` | NOTE REPEAT | 16 | `BTN_PLAY` | SEQ PLAY |
| 8 | `BTN_SEL` | SCALE | 17 | `BTN_REC` | SEQ REC |

`BTN_OCT_DOWN` held at power-on is the USB rescue; your app sees it like any button otherwise.

Encoders: `ENC_MASTER` (K1, the volume: platform.c reads it), `ENC_SELECT` (K2), `ENC_ALGORITHM` (K3),
`ENC_PRESETS` (K4), `ENC_KNOB1` .. `ENC_KNOB4` (K5..K8). To use K1 yourself, `#define SMK_MASTER_FROM_APP 1` before
`#include "platform.c"` and set the volume (`smk_master`) on your own.

A key down and up, the usual way:

```c
uint32_t keys = fm1_in.notes, down = keys & ~prev, up = prev & ~keys;
prev = keys;
```

## LEDs

| | |
| --- | --- |
| `fm1_led_key(BTN_..., on)` | a button's LED lit or not (the keys have no LEDs) |
| `padled_set(pad, r, g, b)`, `padled_flush()` | the pads' colours (0..255 each); the flush sends only what changed (~1 ms) |

## Screen

240 x 240 pixels, 16-bit colour (`RGB(r, g, b)` with 0..255 each; `C_BLACK`, `C_WHITE`). The screen keeps
what you draw: draw only what changed.

Straight to the screen:

| | |
| --- | --- |
| `lcd_fill(x, y, w, h, colour)` | a rectangle |
| `draw_text_box(x, y, w, font, "text", colour, align)` | text in a box of width `w`, cleared to black first; align 0 left, 1 centre, 2 right |

Fonts: `FONT_S` (16 px, ASCII 32..126) and `FONT_L` (32 px, ASCII 32..95: capitals, digits, signs; small
letters show as `?`).

For anything more, a canvas: draw into a buffer, then send it in one go (no flicker):

```c
cv_begin(240, 56, C_BLACK);                 /* w x h, cleared; w * h <= CV_MAX (240 x 64) */
cv_rect(x, y, w, h, colour);
cv_line(x0, y0, x1, y1, colour);
cv_pset(x, y, colour);
cv_text(x, y, &FONT_S, "text", colour);     /* returns x after the text; text_w(&FONT_S, "text") measures */
cv_blit(0, 140);                            /* to the screen at (0, 140) */
```

A bigger canvas: `#define CV_MAX (240u * 120u)` before `#include "platform.c"` (2 bytes a pixel, in the pool:
240 x 240 is 113 KB of 336).

**Nothing waits for a redraw.** A canvas goes out by DMA, a strip of 240 x 64 in some 3 ms. Define `LCD_IDLE` before
`#include "platform.c"` and lcd.c calls it again and again while it waits for the DMA, so your app reads MIDI (or the
keys) meanwhile. `app.c` does it:

```c
static void app_idle(void);
#define LCD_IDLE app_idle
#include "platform.c"
...
static void app_idle(void) { if (idle_ok) midi_in(); }   /* idle_ok: set only around your own drawing */
```

Guard it with a flag of your own: the platform draws too (the update and crash screens), and the function must not
run inside itself.

## Sound

```c
static void app_audio(int32_t *out, uint32_t n)
```

- Runs in the audio interrupt, every 5.8 ms, with `n` = 256 frames at **44.1 kHz** (`FS`).
- `out` is `L R L R ...`, **already zero**: add your sound to it. Full scale is **±32767** (Q15); more is clipped.
- After you: the volume, the clip, the USB audio stream (the computer records exactly this), the DAC. The DAC gets it
  6 dB under its full scale, so the output stage does not distort.
- Keep it fast and simple: no flash, no screen, no waiting. Talk to the main loop through `volatile` variables
  (one writer each). If a render runs late the sound breaks up; see [RULES.md](RULES.md).

Pitch: a 32-bit phase that wraps is an oscillator with no division at run time: `inc = f * 2^32 / 44100`,
`ph += inc` each frame. `app.c` keeps a table for one octave and shifts it for the others.

## MIDI

USB-MIDI, class compliant. The SMK-37 has no MIDI jack.

| | |
| --- | --- |
| `midi_get(&status, &d1, &d2)` | the next message, 0 = none. Call it from the main loop until 0 |
| `midi_send(status, d1, d2)` | send one message over USB (from the main loop only) |

`status` includes the channel: `0x90` note on channel 1, `0x93` channel 4; a note on with velocity 0 is a note off.
Channel messages come through, and of the system messages the clock and transport ones: `0xF8` clock (24 a quarter
note), `0xFA` start, `0xFB` continue, `0xFC` stop (`d1`, `d2` are 0). SysEx is the platform's (the updater) and does
not reach the app; other system messages are dropped.

A channel of your own, for both ways:

```c
if (ch && (st & 15u) != ch - 1u) continue;          /* in: not ours */
midi_send(0x90u | (ch ? ch - 1u : 0u), note, vel);  /* out */
```

The tempo of an incoming clock: the time between every 24th `0xF8`, `60000 / ms` BPM. `midi_send(0xF8, 0, 0)` sends
a clock, if your firmware is the master.

## Storage: settings, blobs, slots

```c
static int settings_load(void *p, uint32_t n);         /* 0 = loaded; else *p is left alone */
static int settings_save(const void *p, uint32_t n);   /* 0 = saved */
static int blob_load(uint32_t i, void *p, uint32_t n);         /* blob i of NBLOB (8): one sector each */
static int blob_save(uint32_t i, const void *p, uint32_t n);
static int slot_load(uint32_t i, void *p, uint32_t n);         /* slot i of NSLOT (8): SLOT_SECT (4) sectors each */
static int slot_save(uint32_t i, const void *p, uint32_t n);
```

| | how much | where |
| --- | --- | --- |
| settings | up to `SECTOR_MAX` (4084 B) | 0x9D000 |
| blobs 0..7 (presets, patterns) | up to 4084 B each | 0x9E000.. |
| slots 0..7 (songs, samples) | up to `SLOT_MAX` (16372 B) each | 0xC2000.. |

Each is kept with a magic, its length and a CRC32 ([store.h](../firmware/src/store.h)). A change of size, another
firmware's data or a broken save reads as "nothing saved": start from your defaults then. `settings_load` and
`blob_load` leave `p` alone when they fail; `slot_load` reads straight into `p` (no buffer that big), so on -1 it holds
rubbish. Give your firmware its own `STORE_MAGIC`.

**The store's owner.** The SMK-37 firmwares built on the [fm1-to-smk37](https://github.com/genkigenki/fm1-to-smk37)
kit share the store: a tag at 0xAF000 ("SMK37DATA" + `DATA_FAMILY` from tools/build.py, 1..7 of A-Z 0-9) says whose
data it is. At boot, anything that is not yours (the stock settings, another firmware's data) is cleared once and the
tag written; it is never read.

**Save when nothing sounds.** Erasing holds the interrupts off for some 50 ms a sector (a slot: four): the sound
drops out. `app.c` saves 2 s after the last change, once all voices are silent. Flash wears out after about
100,000 erases of a sector: never save in a loop.

## Memory and flash

| | | |
| --- | --- | --- |
| Code + constants | 605 KB | runs straight from flash (`const` tables cost no RAM) |
| RAM `.data` + `.bss` | 96 KB | your globals and statics |
| Pool | 336 KB | big buffers, zeroed at boot: `static int16_t delay[65536] __attribute__((section(".pool")));` |
| Stack | 24 KB | keep big arrays off it: `static` |

The build prints what you use and fails above the limits.

Flash (1 MiB): the head (0x0-0x3FFF), the app (0x4000-0x9CFFF), the store above it, the update staging area
(0xBB000-0xBFFFF), the slots (0xC2000-). **Never touched:** the head, BT pairing and the key / pad calibration
(0xC0000-0xC1FFF), the stock resources (0xF4000-0xFDFFF) and key_mac (0xFF000-). The store's allow-list
(`FL_RANGE_OK`, store.h) keeps a bug from erasing anything else, and the RAM flash driver refuses the protected ranges
on its own as well (`FL_GUARD`, hal/fm1_flash_map.h; tests/smk_flash_test.c checks every sector).

## Build options

`./build.sh` takes `--release X.Y` and `--model pro|elite|mke` (or `MODEL=`): the SMK-37 Pro, Elite and MKE-P37 are
one keyboard under three names, and M-Vave's updater only takes a unit's own factory package back, so each name gets
its own package. `DINGHY_DEFS="-DFELUCCA_UAC=0" ./build.sh` adds defines for a test build. In platform.c (or before
`#include "platform.c"` where it says `#ifndef`):

| | | |
| --- | --- | --- |
| `FELUCCA_UAC` | 1 | USB audio input (the output to the computer); 0 = MIDI only |
| `FELUCCA_CDC` | 0 | a USB serial console for debugging (some hosts then hide the audio; see usb.c) |
| `FELUCCA_OTA` | 1 | the updater. **Leave it on**: without it nothing can install over your firmware but the rescue |
| `CV_MAX` | 240 x 64 | the biggest canvas, in pixels |
| `SMK_MASTER_FROM_APP` | 0 | 1: K1 is the app's, the app sets the volume |

The app is built with `-Os`. Leave it so: with `-O2` (the unity build puts usb.c in it) the USB-MIDI IN babbled on an
FM-1 and the host dropped the port.

In tools/build.py, for your firmware: `IDENT` (the identity's number), `DATA_FAMILY` (the store's owner tag).

On the host: `tests/run_host.sh [script]`. A script line can press keys (`T keys BITS`, `T vel V` for their
velocity), a button (`T btn B`), turn an encoder (`T enc E N`), hit a pad (`T pad P V`), move a fader (`T fader F V`)
or a wheel (`T wheel W RAW`), play an outer key (`T xkey K V`), send MIDI (`T midi S D1 D2`) and take a screenshot
(`T shot`); [tests/host.c](../tests/host.c) has the full list, [tests/scripts/](../tests/scripts) the examples.
