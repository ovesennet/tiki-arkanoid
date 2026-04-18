/* TIKI ARKANOID — video.h
 * Video subsystem — mode set, direct VRAM access for drawing.
 */

#ifndef VIDEO_H
#define VIDEO_H

#include "defs.h"

void vid_init(void);
void vid_shutdown(void);
void vid_clear(void);
void vid_hline(uint16_t x1, uint16_t x2, uint16_t y, uint8_t colour);
void vid_vline(uint16_t x, uint16_t y1, uint16_t y2, uint8_t colour);
void vid_fill_rect(uint16_t x, uint16_t y, uint8_t w, uint8_t h, uint8_t colour);
void vid_plot(uint16_t x, uint16_t y, uint8_t colour);
void vid_draw_text(uint16_t x, uint16_t y, const char *str, uint8_t colour);

/* Flicker-free ball move: erase old + draw new in single VRAM bank switch */
void vid_move_ball(uint16_t ox, uint16_t oy, uint16_t nx, uint16_t ny, uint8_t sz);

#endif /* VIDEO_H */
