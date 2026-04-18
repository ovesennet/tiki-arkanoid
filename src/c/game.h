/* TIKI ARKANOID — game.h
 * Core game state and logic.
 */

#ifndef GAME_H
#define GAME_H

#include "defs.h"

/* ── Ball state (plain pixel coords + integer velocity) ── */
typedef struct {
    uint16_t x, y;     /* pixel position */
    int8_t   dx, dy;   /* velocity in pixels/frame */
    uint8_t  active;
} Ball;

/* ── Game state ── */
extern uint8_t  g_bricks[BRICK_ROWS][BRICK_COLS];
extern uint8_t  g_state;
extern uint8_t  g_lives;
extern uint8_t  g_round;
extern uint32_t g_score;
extern uint16_t g_paddle_x;    /* pixel position of paddle left edge */
extern uint8_t  g_paddle_w;    /* current paddle width */
extern Ball     g_ball;
extern uint8_t  g_bricks_left; /* breakable bricks remaining */
extern uint8_t  g_dirty;       /* bitmask: 1=score 2=lives 4=round */

#define DIRTY_SCORE  1
#define DIRTY_LIVES  2
#define DIRTY_ROUND  4
#define DIRTY_ALL    7

void game_init(void);
void game_load_stage(uint8_t stage);
void game_reset_ball(void);
void game_launch_ball(void);
void game_update(void);
void game_draw_playfield(void);
void game_draw_paddle(void);
void game_draw_paddle_force(void);
void game_draw_ball(void);
void game_move_ball(void);
void game_erase_ball(void);
void game_draw_sidebar_labels(void);
void game_draw_sidebar_values(void);
void game_draw_brick(uint8_t row, uint8_t col);

#endif
