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

/* Flicker-free paddle move: full erase+redraw in single VRAM bank switch */
void vid_move_paddle(uint16_t old_x, uint16_t new_x, uint8_t ow, uint8_t w,
                     uint8_t h, uint8_t y, uint8_t colour);

/* Flicker-free capsule move: erase old + draw new body in single bank switch */
void vid_move_capsule(uint16_t ox, uint16_t oy, uint16_t nx, uint16_t ny,
                      uint8_t w, uint8_t h, uint8_t colour);

/* Wait for next vertical blank interrupt (~50 Hz) */
void wait_vsync(void);

/* Flicker-free enemy move: erase old + draw sprite in single VRAM bank switch */
void vid_move_enemy(int16_t old_x, int16_t old_y,
                    int16_t new_x, int16_t new_y,
                    uint8_t type, uint8_t frame);

/* Batched VRAM access — bracket multiple fill_rect_nr calls
   to avoid per-call bank switching (flicker-free drawing). */
void vid_begin_vram(void);
void vid_end_vram(void);
void vid_fill_rect_nr(uint16_t x, uint16_t y, uint8_t w, uint8_t h, uint8_t colour);

#endif /* VIDEO_H */
