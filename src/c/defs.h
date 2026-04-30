/* TIKI ARKANOID — defs.h
 * Shared definitions, types, and constants.
 */

#ifndef DEFS_H
#define DEFS_H

#include <stdint.h>

/* ── Screen: 256x256 mode 3 (16 colours, 128 bytes/scanline) ── */
#define SCREEN_W       256
#define SCREEN_H       256

/* ── Playfield layout (C64-style: left portion of screen) ─── */
/* Playfield area in pixels (bricks + ball area) */
#define PF_LEFT        4      /* playfield left edge */
#define PF_TOP         16     /* playfield top edge */
#define PF_WIDTH       212    /* 13 bricks * 16px + 4px walls */
#define PF_BOTTOM      240    /* death line */

/* Brick grid */
#define BRICK_COLS     13
#define BRICK_ROWS     24     /* max rows in a stage */
#define BRICK_W        16     /* pixels wide (14 visible + 2px right border) */
#define BRICK_H        8      /* pixels tall (7 visible + 1px bottom border) */
#define GRID_LEFT      (PF_LEFT + 2)
#define GRID_TOP       (PF_TOP + 2)

/* Paddle */
#define PADDLE_Y       228
#define PADDLE_W       34     /* normal width */
#define PADDLE_H       10
#define PADDLE_WIDE_W  52     /* enlarged */
#define PADDLE_MIN_X   (PF_LEFT + 2)
#define PADDLE_MAX_X   (PF_LEFT + PF_WIDTH - 2)
#define PADDLE_SPEED   3

/* Ball */
#define BALL_SIZE      4      /* 4x4 pixel ball */
#define BALL_SPEED_INIT 2     /* initial speed (8.8 fixed = 0x0200) */

/* Sidebar info area */
#define SIDE_X         220
#define SIDE_Y         16

/* ── Brick types ───────────────────────────────────────── */
#define BRK_EMPTY      0
#define BRK_RED        1
#define BRK_ORANGE     2
#define BRK_YELLOW     3
#define BRK_GREEN      4
#define BRK_CYAN       5
#define BRK_BLUE       6
#define BRK_MAGENTA    7
#define BRK_WHITE      8
#define BRK_SILVER     9      /* multi-hit */
#define BRK_GOLD       10     /* indestructible */
#define NUM_BRICK_TYPES 11

/* ── Palette indices (same CGA-like as TikiTetris) ─────── */
#define COL_BLACK      0
#define COL_BLUE       1
#define COL_GREEN      2
#define COL_CYAN       3
#define COL_RED        4
#define COL_MAGENTA    5
#define COL_ORANGE     6
#define COL_LTGREY     7
#define COL_DKGREY     8
#define COL_BRBLUE     9
#define COL_BRGREEN    10
#define COL_BRCYAN     11
#define COL_BRRED      12
#define COL_PINK       13
#define COL_YELLOW     14
#define COL_WHITE      15

/* ── Game states ───────────────────────────────────────── */
#define STATE_TITLE    0
#define STATE_PLAY     1
#define STATE_LAUNCH   2  /* ball attached to paddle */
#define STATE_GAMEOVER 3

/* ── Input keys ────────────────────────────────────────── */
#define KEY_LEFT       'a'
#define KEY_RIGHT      'd'
#define KEY_FIRE       ' '
#define KEY_QUIT       'q'
#define KEY_PAUSE      'p'

/* ── Scoring ───────────────────────────────────────────── */
#define START_LIVES    5
#define EXTRA_LIFE_PTS 20000UL

/* ── Power-up capsule types ────────────────────────────── */
#define CAP_NONE       0
#define CAP_SLOW       1   /* S — slow ball */
#define CAP_CATCH      2   /* C — catch & relaunch */
#define CAP_ENLARGE    3   /* E — widen paddle */
#define CAP_DISRUPT    4   /* D — 3 balls */
#define CAP_LASER      5   /* L — paddle shoots */
#define CAP_BREAK      6   /* B — skip stage */
#define CAP_PLAYER     7   /* P — extra life */
#define NUM_CAP_TYPES  8

/* Capsule visual */
#define CAP_W          10    /* pixels wide */
#define CAP_H          6     /* pixels tall */
#define CAP_FALL_SPEED 1     /* pixels per frame */

/* ── Paddle mode (active power-up) ─────────────────────── */
#define PMODE_NORMAL   0
#define PMODE_CATCH    1
#define PMODE_LASER    2
#define PMODE_ENLARGE  3

/* ── Capsule struct ────────────────────────────────────── */
typedef struct {
    uint8_t  type;     /* CAP_xxx, 0 = inactive */
    uint16_t x;        /* pixel x */
    uint16_t y;        /* pixel y */
} Capsule;

/* ── Multi-ball (for Disruption) ───────────────────────── */
#define MAX_BALLS      3

typedef struct {
    uint8_t  active;
    uint16_t x, y;
    int8_t   dx, dy;
} Ball;

/* ── Laser bolts ───────────────────────────────────────── */
#define MAX_LASERS     2
#define LASER_W        2      /* pixels wide */
#define LASER_H        6      /* pixels tall */
#define LASER_SPEED    3      /* pixels per frame */
#define LASER_COOLDOWN 8      /* frames between shots */

typedef struct {
    uint8_t  active;
    uint16_t x, y;
} Laser;

/* ── Enemies ───────────────────────────────────────────── */
#define MAX_ENEMIES    2
#define ENEMY_W        8      /* pixels wide */
#define ENEMY_HT       8      /* pixels tall */
#define ENEMY_SCORE    100    /* points per kill */
#define ENEMY_SPAWN_MIN 300   /* min frames between spawns */
#define ENEMY_SPAWN_MAX 600   /* max frames between spawns */

typedef struct {
    int16_t  x;
    int16_t  y;
    int8_t   dx;
    int8_t   dy;
    uint8_t  active;
    uint8_t  type;       /* sprite variant 0..1 */
    uint8_t  anim;       /* animation frame counter */
    uint8_t  steps;      /* sideways steps remaining before vertical step */
} Enemy;

#endif /* DEFS_H */
