/* TIKI ARKANOID — sound.h */

#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

extern void delay(uint16_t n);
extern uint8_t g_sound_on;

void sound_init(void);
void sound_off(void);
void sfx_bounce(void);
void sfx_brick(void);
void sfx_lose_life(void);
void sfx_launch(void);
void sfx_stage_intro(void);
void sfx_win(void);
void sfx_laser(void);

#endif
