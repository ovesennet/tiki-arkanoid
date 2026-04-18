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
#define PF_LEFT        32     /* playfield left edge */
#define PF_TOP         16     /* playfield top edge */
#define PF_WIDTH       160    /* 13 bricks * 12px + 4px walls */
#define PF_BOTTOM      240    /* death line */

/* Brick grid */
#define BRICK_COLS     13
#define BRICK_ROWS     18     /* max rows in a stage */
#define BRICK_W        12     /* pixels wide */
#define BRICK_H        6      /* pixels tall */
#define GRID_LEFT      (PF_LEFT + 2)
#define GRID_TOP       (PF_TOP + 2)

/* Paddle */
#define PADDLE_Y       230
#define PADDLE_W       24     /* normal width */
#define PADDLE_H       4
#define PADDLE_WIDE_W  36     /* enlarged */
#define PADDLE_MIN_X   (PF_LEFT + 2)
#define PADDLE_MAX_X   (PF_LEFT + PF_WIDTH - 2)
#define PADDLE_SPEED   4

/* Ball */
#define BALL_SIZE      4      /* 4x4 pixel ball */
#define BALL_SPEED_INIT 2     /* initial speed (8.8 fixed = 0x0200) */

/* Sidebar info area */
#define SIDE_X         200
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

#endif /* DEFS_H */
