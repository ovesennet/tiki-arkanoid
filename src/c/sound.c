/* TIKI ARKANOID — sound.c
 * Sound effects via AY-3-8910 PSG.
 */

#include <arch/tiki100.h>
#include "sound.h"

#define NOTE_C5   191
#define NOTE_E5   152
#define NOTE_G5   128
#define NOTE_A4   227
#define NOTE_C6    96

uint8_t g_sound_on = 1;

void sound_init(void) { psg_init(); }

void sound_off(void)
{
    psg_volume(0, 0);
    psg_volume(1, 0);
    psg_volume(2, 0);
}

void sfx_bounce(void)
{
    if (!g_sound_on) return;
    psg_channels(chan0, chanNone);
    psg_tone(0, NOTE_C6);
    psg_volume(0, 10);
    delay(200);
    sound_off();
}

void sfx_brick(void)
{
    if (!g_sound_on) return;
    psg_channels(chan0, chanNone);
    psg_tone(0, NOTE_E5);
    psg_volume(0, 12);
    delay(400);
    psg_tone(0, NOTE_G5);
    delay(300);
    sound_off();
}

void sfx_lose_life(void)
{
    if (!g_sound_on) return;
    psg_channels(chan0, chanNone);
    psg_tone(0, NOTE_A4);
    psg_volume(0, 14);
    delay(2000);
    psg_tone(0, 500);
    delay(2000);
    sound_off();
}

void sfx_launch(void)
{
    if (!g_sound_on) return;
    psg_channels(chan0, chanNone);
    psg_tone(0, NOTE_C5);
    psg_volume(0, 8);
    delay(300);
    sound_off();
}

/* ── Stage intro jingle (Arkanoid: Doh It Again - Start Level) ── */
/* Transcribed from SNES MIDI: 150 BPM, 192 ticks/QN, Eb major       */
/* Note-to-note intervals from tick positions:                        */
/*   0→128=267ms  128→192=133ms  192→576=800ms  576→672=200ms        */
/*   672→768=200  768→864=200    864→960=200    final hold=1500ms    */

struct note { uint16_t period; uint16_t dur; };

/* AY periods: period = 100000 / freq_hz */
#define N_G4    255
#define N_A4    227
#define N_Bb4   215
#define N_F4    287
#define N_G5    128
#define N_A5    114
#define N_Bb5   107
#define N_F5    143

/* Delays proportional to real ms intervals (scale: 600 ≈ 200ms) */
static const struct note s_intro[] = {
    {N_G4,   3200},   /* G4  */
    {N_G4,   1600},   /* G4  */
    {N_Bb4,  9600},   /* Bb4 */
    {N_A4,   2400},   /* A4  */
    {N_G4,   2400},   /* G4  */
    {N_F4,   2400},   /* F4  */
    {N_A4,   2400},   /* A4  */
    {N_G4,  18000},   /* G4  — final held */
};
#define INTRO_LEN (sizeof(s_intro)/sizeof(s_intro[0]))

void sfx_stage_intro(void)
{
    uint8_t i;
    if (!g_sound_on) return;
    psg_channels(chan0 | chan1, chanNone);
    for (i = 0; i < INTRO_LEN; i++) {
        /* ch0 = melody, ch1 = octave up (like SNES tracks 1+2) */
        psg_tone(0, s_intro[i].period);
        psg_tone(1, s_intro[i].period / 2);  /* octave up */
        psg_volume(0, 13);
        psg_volume(1, 10);
        delay(s_intro[i].dur);
    }
    sound_off();
}

/* ── Victory fanfare — ascending triumphant jingle ── */
#define N_C5    191
#define N_D5    170
#define N_E5x   152
#define N_G5x   128
#define N_C6    96
#define N_E6    76
#define N_G6    64

static const struct note s_win[] = {
    {N_C5,   1600},
    {N_E5x,  1600},
    {N_G5x,  1600},
    {N_C6,   3200},
    {N_G5x,  1200},
    {N_C6,   3200},
    {N_E6,   3200},
    {N_G6,   3200},
    {N_E6,   1600},
    {N_G6,  12000},
};
#define WIN_LEN (sizeof(s_win)/sizeof(s_win[0]))

void sfx_win(void)
{
    uint8_t i;
    if (!g_sound_on) return;
    psg_channels(chan0 | chan1 | chan2, chanNone);
    for (i = 0; i < WIN_LEN; i++) {
        psg_tone(0, s_win[i].period);
        psg_tone(1, s_win[i].period / 2);
        psg_tone(2, s_win[i].period * 2);
        psg_volume(0, 14);
        psg_volume(1, 10);
        psg_volume(2, 8);
        delay(s_win[i].dur);
    }
    sound_off();
}

void sfx_laser(void)
{
    if (!g_sound_on) return;
    psg_channels(chan0, chanNone);
    psg_tone(0, 60);   /* high-pitched zap */
    psg_volume(0, 8);
    delay(80);
    sound_off();
}
