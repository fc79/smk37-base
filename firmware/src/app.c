/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK BASE: the smallest M-Vave SMK-37 firmware to build on (Pro, Elite, MKE-P37). Each part of the platform is used
 * once, as an example, and shows on a unit that it works. This is the file to change; platform.c below it does the
 * rest: updates, the USB rescue, MIDI, USB audio, the keys, pads, knobs, faders, wheels, LEDs, the screen, flash.
 *
 *   keys       a sine, 4 voices, with the key's velocity; the notes go out over USB-MIDI, channel 1
 *   pads       the same: the bottom row = white keys 1-8, the top row 9-16; a pad glows while its note sounds
 *   OCTAVE-/+  the octave, -2..+2 (kept in flash: settings_save)
 *   K1         the volume (platform.c)
 *   pitch      the wheel bends +-2 semitones
 *   buttons    lit while held
 *   MIDI in    notes on any channel play the sine, with their velocity
 *   screen     the last control touched, by its hardware id (keys 0..36, buttons 0..17, pads 0..15, faders 0..3,
 *              wheels 0..1; the encoders by their printed name, K1..K8), the encoders' positions, faders and wheels
 *
 * Rules of the road (the chip has no FPU and no libgcc): integers only, no float, no 64-bit division.
 * tests/run_host.sh runs this file on a computer: the screen as PNG, the sound as WAV.
 * What platform.c offers: docs/API.md. The chip's rules: docs/RULES.md. The board: docs/SMK37.md. */
#define APP_NAME "SMK BASE"
#define STORE_MAGIC 0x37334253u                 /* "SB37": settings and blobs (store.h); a firmware of yours: your own */
static void app_idle(void);
#define LCD_IDLE app_idle                       /* while the screen's DMA runs: MIDI in (platform.c) */
#include "platform.c"
#include "smk_wheel.h"
#ifndef FELUCCA_ID
#define FELUCCA_ID "HOST"                       /* (platform.c gives it to the device build only) */
#endif

/* ---- the sound (runs in the audio ISR: keep it short, no flash, no screen) */
#define NV 4u                                   /* voices */
static struct {
    volatile uint32_t inc;                      /* phase step per frame: f * 2^32 / 44100 (unbent) */
    volatile int32_t gate;                      /* target level, Q15: 0 = released */
    int32_t amp;                                /* the level now: glides to gate, no clicks */
    uint32_t ph, age;
    uint8_t note;
} voice[NV];
static volatile uint32_t bend_q16 = 65536;      /* the pitch wheel: the steps times this / 65536 */

static int32_t sine(uint32_t ph)                /* a full cycle over 2^32 -> -32767..32767 (a refined parabola, ~0.1 %) */
{
    int32_t x = (int32_t)ph >> 16, ax = x < 0 ? -x : x, y, ay;
    y = (x * (32768 - ax)) >> 13;               /* 4x(1-|x|), Q15 */
    ay = y < 0 ? -y : y;
    y += ((((y * ay) >> 15) - y) * 7373) >> 15; /* + 0.225 (y|y| - y) */
    return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}

static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t v, i;
    for (v = 0; v < NV; v++) {
        uint32_t ph = voice[v].ph, step = (uint32_t)(((uint64_t)voice[v].inc * bend_q16) >> 16);   /* (int64 multiply: fine) */
        int32_t amp = voice[v].amp, g = voice[v].gate;
        if (!amp && !g)
            continue;                           /* silent: costs nothing */
        for (i = 0; i < n; i++) {
            int32_t s;
            if (amp < g) { amp += 48; if (amp > g) amp = g; }        /* attack ~5 ms */
            else if (amp > g) { amp -= 6; if (amp < g) amp = g; }    /* release ~45 ms */
            ph += step;
            s = (sine(ph) * amp) >> 15;
            out[2u * i] += s;                   /* L */
            out[2u * i + 1u] += s;              /* R */
        }
        voice[v].ph = ph;
        voice[v].amp = amp;
    }
}

/* ---- notes (main loop) */
static const uint32_t INC_C9[12] = {            /* MIDI 120..131 (C9..B9); lower octaves shift right */
    815363807u, 863847862u, 915214929u, 969636441u, 1027294024u, 1088380105u,
    1153098554u, 1221665363u, 1294309365u, 1371273005u, 1452813141u, 1539201906u};
static uint32_t ages;

static void play(uint32_t note, uint32_t vel)
{
    uint32_t v, pick = 0, best = 0xFFFFFFFFu;
    if (note > 127u || !vel) return;
    for (v = 0; v < NV; v++) {                  /* the same note, else a free voice, else the oldest */
        uint32_t score = voice[v].note == note && voice[v].gate ? 0u
                       : voice[v].gate ? 0x80000000u + voice[v].age : (uint32_t)voice[v].amp + 1u;
        if (score < best) { best = score; pick = v; }
    }
    voice[pick].note = (uint8_t)note;
    voice[pick].age = ++ages;
    voice[pick].inc = INC_C9[note % 12u] >> (10u - note / 12u);
    voice[pick].gate = (int32_t)(vel * 90u);    /* 127 -> 11430: four voices stay under full scale */
}
static void stop(uint32_t note)
{
    uint32_t v;
    for (v = 0; v < NV; v++)
        if (voice[v].note == note)
            voice[v].gate = 0;
}
static int sounding(uint32_t note)
{
    uint32_t v;
    for (v = 0; v < NV; v++)
        if (voice[v].gate && voice[v].note == note) return 1;
    return 0;
}
static int quiet(void)                          /* nothing sounds, not even a release */
{
    uint32_t v;
    for (v = 0; v < NV; v++)
        if (voice[v].gate || voice[v].amp) return 0;
    return 1;
}

/* ---- the screen: the name, the last control touched, the encoders, faders and wheels, the octave */
#define C_ACC RGB(180, 140, 255)                /* lilac */
#define C_TXT RGB(210, 210, 210)
#define C_OFF RGB(90, 90, 90)
static const char *const BTN_NAME[18] = {"KNOB BANK", "FADER BANK", "<", ">", "OCTAVE-", "OCTAVE+", "ARP",
                                         "NOTE REPEAT", "SCALE", "CHORD", "GLOBE", "BT", "PATCH", "PARA", "FX",
                                         "SEQ", "SEQ PLAY", "SEQ REC"};
#ifdef DINGHY_HOST
#define ENC0 0u                                 /* the host has no MASTER reader: encoder 0 is the app's too */
#else
#define ENC0 1u                                 /* encoder 0 (K1) is the volume, seen through master_q12 */
#endif
static char ev[24] = "-", ev2[32] = "touch a control";
static uint32_t ev_n;
static int32_t enc_pos[8];

static char *cat(char *p, const char *s) { while (*s) *p++ = *s++; *p = 0; return p; }
static char *catn(char *p, int32_t v)           /* decimal (libc.c's fmt_int is not in the host build) */
{
    char b[12];
    uint32_t n = 0, u = (uint32_t)(v < 0 ? -v : v);
    do { b[n++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    if (v < 0) *p++ = '-';
    while (n) *p++ = b[--n];
    *p = 0;
    return p;
}

static void event_name(const char *what, uint32_t id, const char *name)   /* "WHAT id" over the name */
{
    catn(cat(ev, what), (int32_t)id);
    cat(ev2, name);
    ev_n++;
}
static void event(const char *what, uint32_t id, const char *lab, int32_t v)   /* "WHAT id" over "lab v" */
{
    event_name(what, id, "");
    catn(cat(ev2, lab), v);
}

static void draw_events(void)
{
    char s[40], *p;
    uint32_t i;
    draw_text_box(0, 82, 240, &FONT_L, ev, C_WHITE, 1);
    draw_text_box(0, 118, 240, &FONT_S, ev2, C_ACC, 1);
    for (i = 0; i < 8u; i += 4u) {              /* the encoders' positions, two rows of four */
        uint32_t j;
        p = s;
        for (j = i; j < i + 4u; j++) {
            p = catn(cat(p, j == i ? "K" : "  K"), (int32_t)j + 1);
            p = catn(cat(p, ":"), enc_pos[j]);
        }
        draw_text_box(0, 146 + 20 * (i / 4u), 240, &FONT_S, s, C_TXT, 1);
    }
    p = cat(s, "F");
    for (i = 0; i < 4u; i++)
        p = catn(cat(p, " "), fm1_in.fader[i]);
    p = catn(cat(p, "  W "), fm1_in.wheel[0]);
    catn(cat(p, " "), fm1_in.wheel[1]);
    draw_text_box(0, 186, 240, &FONT_S, s, C_TXT, 1);
}

static void draw_octave(int32_t oct)
{
    char s[8] = "OCT +0";
    s[4] = oct < 0 ? '-' : '+';
    s[5] = (char)('0' + (oct < 0 ? -oct : oct));
    draw_text_box(0, 214, 240, &FONT_S, s, C_OFF, 1);
}

/* ---- MIDI in: from the main loop, and from app_idle while the screen draws (idle_ok) */
static struct { int8_t oct; } set;              /* what survives a power-off (add yours, up to SECTOR_MAX bytes) */
static uint32_t idle_ok;

static void midi_in(void)
{
    uint8_t st, d1, d2;
    while (midi_get(&st, &d1, &d2)) {           /* any channel */
        uint32_t ty = st & 0xF0u;
        if (ty == 0x90u && d2) play(d1, d2);
        else if (ty == 0x80u || ty == 0x90u) stop(d1);
    }
}
static void app_idle(void)
{
    if (idle_ok) midi_in();
}

/* ---- the main loop */
#define KEY0 53u                                /* the note of F3, key 5 of the keybed, at octave 0 */
static void app_main(void)
{
    static const uint8_t WHITE[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};   /* white key -> F3 + n */
    uint8_t key_note[FM1_NKEY] = {0}, pad_note[16] = {0};   /* the note each key or pad started (0: none) */
    uint32_t prev = 0, k, t_change = 0, dirty = 0, ev_drawn = ~0u, oct_drawn = ~0u;
    uint8_t fader_seen[4] = {fm1_in.fader[0], fm1_in.fader[1], fm1_in.fader[2], fm1_in.fader[3]};
    uint16_t wheel_seen[2] = {fm1_in.wheel[0], fm1_in.wheel[1]};
    smk_pw_t wheel = {0};
#ifndef DINGHY_HOST
    int32_t master_seen = master_q12;
#endif
    smk_pad_events = 1;                         /* the pads as events (smk_pad_take): they play here */
    if (settings_load(&set, sizeof set) || set.oct < -2 || set.oct > 2)
        set.oct = 0;

    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 18, 240, &FONT_L, APP_NAME, C_ACC, 1);
    draw_text_box(0, 52, 240, &FONT_S, APP_VER "  " FELUCCA_ID, C_OFF, 1);

    for (;;) {
        uint32_t keys, down, up, rel, b, base = (uint32_t)((int32_t)KEY0 + 12 * set.oct);
        int32_t e;
        platform_poll();

        keys = fm1_in.notes;                    /* bit n: keybed key F3 + n (n = 0..26), the middle of the keybed */
        down = keys & ~prev;
        up = prev & ~keys;
        prev = keys;
        for (k = 0; k < 27u; k++) {             /* (a key stops the note it started: the octave may change meanwhile) */
            uint32_t key = SMK_KEY_F3 + k, vel = fm1_in.key_vel[key];
            if (down >> k & 1u) {
                key_note[key] = (uint8_t)(base + k);
                play(key_note[key], vel);
                midi_send(0x90u, key_note[key], vel);
                event("KEY ", key, "vel ", (int32_t)vel);
            }
            if (up >> k & 1u && key_note[key]) { stop(key_note[key]); midi_send(0x80u, key_note[key], 0); key_note[key] = 0; }
        }
        while ((e = smk_extkey_take()) >= 0) {  /* the keys left and right of those 27: key << 8 | velocity (0 = up) */
            uint32_t key = (uint32_t)e >> 8, vel = (uint32_t)e & 0x7Fu;
            if (key >= FM1_NKEY) continue;
            if (vel) {
                key_note[key] = (uint8_t)(base + key - SMK_KEY_F3);
                play(key_note[key], vel);
                midi_send(0x90u, key_note[key], vel);
                event("KEY ", key, "vel ", (int32_t)vel);
            } else if (key_note[key]) { stop(key_note[key]); midi_send(0x80u, key_note[key], 0); key_note[key] = 0; }
        }
        while ((e = smk_pad_take()) >= 0) {     /* a pad: hardware pad << 8 | velocity (0 = let go) */
            uint32_t pad = ((uint32_t)e >> 8) & 15u, vel = (uint32_t)e & 0x7Fu;
            uint32_t w = pad < 8u ? pad + 8u : pad - 8u;   /* bottom row = white keys 1-8, top row 9-16 */
            if (vel) {
                pad_note[pad] = (uint8_t)(base + WHITE[w]);
                play(pad_note[pad], vel);
                midi_send(0x90u, pad_note[pad], vel);
                event("PAD ", pad, "vel ", (int32_t)vel);
            } else if (pad_note[pad]) { stop(pad_note[pad]); midi_send(0x80u, pad_note[pad], 0); pad_note[pad] = 0; }
        }
        {
            int32_t bd = smk_pw_bend(&wheel, fm1_in.wheel[0], fm1_ms);   /* 1/4096 semitone */
            bend_q16 = (uint32_t)(65536 + (bd * 3785 >> 12) + (((bd * bd) >> 12) * 109 >> 12));   /* ~2^(s/12) */
        }

        b = fm1_input_edges(&rel);              /* buttons pressed since the last call (bit = button id) */
        for (k = 0; k < 18u; k++)
            if (b >> k & 1u) event_name("BTN ", k, BTN_NAME[k]);
        if (b & (1u << BTN_OCT_DOWN) && set.oct > -2) { set.oct--; dirty = 1; t_change = fm1_ms; }
        if (b & (1u << BTN_OCT_UP) && set.oct < 2) { set.oct++; dirty = 1; t_change = fm1_ms; }
        for (k = ENC0; k < 8u; k++)             /* detents turned since the last call */
            if ((e = fm1_enc_take(k)) != 0) { enc_pos[k] += e; event("K", k + 1u, "turned ", enc_pos[k]); }
#ifndef DINGHY_HOST
        if (master_q12 != master_seen) {        /* K1, encoder 0, is the volume: platform.c reads it */
            master_seen = master_q12;
            if (fm1_ms > 1000u) event("K", 1, "volume ", master_q12 * 100 / 4096);
        }
#endif
        for (k = 0; k < 4u; k++)
            if (fm1_in.fader[k] != fader_seen[k]) { fader_seen[k] = fm1_in.fader[k]; event("FADER ", k, "", fm1_in.fader[k]); }
        for (k = 0; k < 2u; k++) {
            int32_t d = (int32_t)fm1_in.wheel[k] - (int32_t)wheel_seen[k];
            if (d > 8 || d < -8) { wheel_seen[k] = fm1_in.wheel[k]; event("WHEEL ", k, "raw ", fm1_in.wheel[k]); }
        }

        midi_in();

        for (k = 0; k < 18u; k++)               /* a button lit while it is held */
            fm1_led_key(k, fm1_in.buttons >> k & 1u);
        for (k = 0; k < 16u; k++) {             /* a pad glows lilac while its note sounds */
            int on = pad_note[k] && sounding(pad_note[k]);
            padled_set(k, on ? 90 : 4, on ? 60 : 2, on ? 128 : 8);
        }
        padled_flush();                         /* (sends only what changed) */

        idle_ok = 1;                            /* the drawing below may read MIDI meanwhile (LCD_IDLE) */
        if (ev_n != ev_drawn) { ev_drawn = ev_n; draw_events(); }   /* redraw only what changed */
        if ((uint32_t)(set.oct + 2) != oct_drawn) { oct_drawn = (uint32_t)(set.oct + 2); draw_octave(set.oct); }
        idle_ok = 0;

        if (dirty && quiet() && fm1_ms - t_change > 2000u) {   /* 2 s after the change, when silent (an erase stalls the sound) */
            settings_save(&set, sizeof set);
            dirty = 0;
        }
    }
}
