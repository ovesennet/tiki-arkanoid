/* TIKI ARKANOID — game.c
 * Core game logic: paddle, ball, bricks, collision, drawing.
 */

#include "game.h"
#include "levels.h"
#include "video.h"
#include "font.h"
#include "sound.h"
#include <string.h>

/* ── State ── */
uint8_t  g_bricks[BRICK_ROWS][BRICK_COLS];
uint8_t  g_state;
uint8_t  g_lives;
uint8_t  g_round;
uint32_t g_score;
uint16_t g_paddle_x;
uint8_t  g_paddle_w;
Ball     g_ball;
uint8_t  g_bricks_left;
uint8_t  g_dirty;

/* Previous paddle position for delta redraw */
static uint16_t s_prev_paddle_x;

/* Previous ball position for flicker-free move */
static uint16_t s_prev_ball_x;
static uint16_t s_prev_ball_y;

/* Brick colour lookup (index = brick type) */
static const uint8_t brick_colour[] = {
    COL_BLACK,    /* 0: empty */
    COL_RED,      /* 1 */
    COL_ORANGE,   /* 2 */
    COL_YELLOW,   /* 3 */
    COL_GREEN,    /* 4 */
    COL_CYAN,     /* 5 */
    COL_BLUE,     /* 6 */
    COL_MAGENTA,  /* 7 */
    COL_WHITE,    /* 8 */
    COL_LTGREY,   /* 9: silver */
    COL_BRCYAN,   /* 10: gold (indestructible, light blue) */
};

/* Score per brick type */
static const uint8_t brick_points[] = {
    0, 5, 5, 5, 5, 5, 5, 5, 5, 10, 0
};



/* ── Helpers ── */

static void u32_to_str(uint32_t val, char *buf)
{
    char tmp[11];
    uint8_t i = 0, j;
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    while (val > 0) { tmp[i++] = '0' + (char)(val % 10); val /= 10; }
    for (j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
    buf[i] = '\0';
}

static void u8_to_str(uint8_t val, char *buf)
{
    u32_to_str((uint32_t)val, buf);
}

/* ── Init ── */

void game_init(void)
{
    g_state = STATE_LAUNCH;
    g_lives = START_LIVES;
    g_round = 1;
    g_score = 0;
    g_paddle_w = PADDLE_W;
    g_paddle_x = (PF_LEFT + PF_WIDTH / 2) - (g_paddle_w / 2);
    s_prev_paddle_x = g_paddle_x;
    g_dirty = DIRTY_ALL;
    game_load_stage(1);
    game_reset_ball();
}

void game_load_stage(uint8_t stage)
{
    uint8_t r, c;
    const uint8_t *src;

    memset(g_bricks, BRK_EMPTY, sizeof(g_bricks));
    g_bricks_left = 0;

    if (stage < 1) stage = 1;
    if (stage > NUM_LEVELS) stage = NUM_LEVELS;
    src = levels[stage - 1];
    {
        uint8_t start_row = *src++;
        uint8_t num_rows  = *src++;
        for (r = 0; r < num_rows; r++) {
            for (c = 0; c < BRICK_COLS; c++) {
                uint8_t b = *src++;
                g_bricks[start_row + r][c] = b;
                if (b != BRK_EMPTY && b != BRK_GOLD)
                    g_bricks_left++;
            }
        }
    }
}

void game_reset_ball(void)
{
    g_ball.active = 0;
    g_ball.x = g_paddle_x + g_paddle_w / 2;
    g_ball.y = PADDLE_Y - BALL_SIZE - 1;
    g_ball.dx = 1;
    g_ball.dy = -2;
    s_prev_ball_x = g_ball.x;
    s_prev_ball_y = g_ball.y;
}

void game_launch_ball(void)
{
    g_ball.active = 1;
    g_ball.dx = 1;
    g_ball.dy = -2;
    sfx_launch();
}

/* ── Drawing ── */

void game_draw_brick(uint8_t row, uint8_t col)
{
    uint8_t b = g_bricks[row][col];
    uint16_t px = GRID_LEFT + (uint16_t)col * BRICK_W;
    uint16_t py = GRID_TOP  + (uint16_t)row * BRICK_H;

    if (b == BRK_EMPTY) {
        vid_fill_rect(px, py, BRICK_W - 1, BRICK_H - 1, COL_BLACK);
    } else {
        uint8_t c = brick_colour[b];
        vid_fill_rect(px, py, BRICK_W - 1, BRICK_H - 1, c);
        if (b == BRK_SILVER || b == BRK_GOLD) {
            vid_hline(px, px + BRICK_W - 2, py, COL_WHITE);
            vid_vline(px, py, py + BRICK_H - 2, COL_WHITE);
        }
    }
}

void game_draw_playfield(void)
{
    uint8_t r, c;

    vid_fill_rect(PF_LEFT, PF_TOP, 2, (uint8_t)(PF_BOTTOM - PF_TOP), COL_DKGREY);
    vid_fill_rect(PF_LEFT + PF_WIDTH - 2, PF_TOP, 2, (uint8_t)(PF_BOTTOM - PF_TOP), COL_DKGREY);
    vid_fill_rect(PF_LEFT, PF_TOP, (uint8_t)PF_WIDTH, 2, COL_DKGREY);

    for (r = 0; r < BRICK_ROWS; r++)
        for (c = 0; c < BRICK_COLS; c++)
            if (g_bricks[r][c] != BRK_EMPTY)
                game_draw_brick(r, c);
}

void game_draw_paddle(void)
{
    uint16_t old_x = s_prev_paddle_x;
    uint16_t new_x = g_paddle_x;
    uint8_t w = g_paddle_w;

    if (old_x == new_x) return;

    if (new_x > old_x) {
        /* Moved right: erase left strip, draw right strip */
        uint8_t strip = (uint8_t)(new_x - old_x);
        vid_fill_rect(old_x, PADDLE_Y, strip, PADDLE_H, COL_BLACK);
        vid_fill_rect(old_x + w, PADDLE_Y, strip, PADDLE_H, COL_BRCYAN);
    } else {
        /* Moved left: erase right strip, draw left strip */
        uint8_t strip = (uint8_t)(old_x - new_x);
        vid_fill_rect(old_x + w - strip, PADDLE_Y, strip, PADDLE_H, COL_BLACK);
        vid_fill_rect(new_x, PADDLE_Y, strip, PADDLE_H, COL_BRCYAN);
    }
    /* Redraw highlight on top row */
    vid_hline(new_x, new_x + w - 1, PADDLE_Y, COL_WHITE);

    s_prev_paddle_x = new_x;
}

void game_draw_paddle_force(void)
{
    vid_fill_rect(g_paddle_x, PADDLE_Y, g_paddle_w, PADDLE_H, COL_BRCYAN);
    vid_hline(g_paddle_x, g_paddle_x + g_paddle_w - 1, PADDLE_Y, COL_WHITE);
    s_prev_paddle_x = g_paddle_x;
}

void game_erase_ball(void)
{
    vid_fill_rect(g_ball.x, g_ball.y, BALL_SIZE, BALL_SIZE, COL_BLACK);
}

void game_draw_ball(void)
{
    /* Draw round ball using the move routine (erases+draws at same pos) */
    s_prev_ball_x = g_ball.x;
    s_prev_ball_y = g_ball.y;
    vid_move_ball(g_ball.x, g_ball.y,
                  g_ball.x, g_ball.y, BALL_SIZE);
}

/* Flicker-free: erase old + draw new in one VRAM bank switch */
void game_move_ball(void)
{
    vid_move_ball(s_prev_ball_x, s_prev_ball_y,
                  g_ball.x, g_ball.y, BALL_SIZE);
    s_prev_ball_x = g_ball.x;
    s_prev_ball_y = g_ball.y;
}

/* Static labels — draw once */
void game_draw_sidebar_labels(void)
{
    draw_text(SIDE_X, SIDE_Y, "SCORE", COL_WHITE);
    draw_text(SIDE_X, SIDE_Y + 26, "LIVES", COL_WHITE);
    draw_text(SIDE_X, SIDE_Y + 52, "ROUND", COL_WHITE);
}

/* Values — only when dirty */
void game_draw_sidebar_values(void)
{
    char buf[12];

    if (!g_dirty) return;

    if (g_dirty & DIRTY_SCORE) {
        u32_to_str(g_score, buf);
        vid_fill_rect(SIDE_X, SIDE_Y + 10, 50, FONT_CH, COL_BLACK);
        draw_text(SIDE_X, SIDE_Y + 10, buf, COL_YELLOW);
    }
    if (g_dirty & DIRTY_LIVES) {
        u8_to_str(g_lives, buf);
        vid_fill_rect(SIDE_X, SIDE_Y + 36, 20, FONT_CH, COL_BLACK);
        draw_text(SIDE_X, SIDE_Y + 36, buf, COL_BRGREEN);
    }
    if (g_dirty & DIRTY_ROUND) {
        u8_to_str(g_round, buf);
        vid_fill_rect(SIDE_X, SIDE_Y + 62, 20, FONT_CH, COL_BLACK);
        draw_text(SIDE_X, SIDE_Y + 62, buf, COL_BRCYAN);
    }
    g_dirty = 0;
}

/* ── Ball/brick collision ── */

static void flash_brick(uint8_t r, uint8_t c)
{
    uint16_t px = GRID_LEFT + (uint16_t)c * BRICK_W;
    uint16_t py = GRID_TOP  + (uint16_t)r * BRICK_H;
    vid_fill_rect(px, py, BRICK_W - 1, BRICK_H - 1, COL_WHITE);
    delay(60);
    game_draw_brick(r, c);
}

static void hit_brick(uint8_t r, uint8_t c)
{
    uint8_t b = g_bricks[r][c];
    if (b == BRK_GOLD) {
        flash_brick(r, c);
        sfx_bounce();
        return;
    }

    if (b == BRK_SILVER) {
        flash_brick(r, c);
        g_bricks[r][c] = BRK_WHITE;
        game_draw_brick(r, c);
        sfx_bounce();
        return;
    }

    g_score += brick_points[b] * 10;
    g_dirty |= DIRTY_SCORE;
    g_bricks[r][c] = BRK_EMPTY;
    g_bricks_left--;
    game_draw_brick(r, c);
    sfx_brick();
}

/* ── Update ── */

void game_update(void)
{
    int16_t nx, ny, bx, by;
    int16_t cx, ty;
    uint8_t gr, gc;

    if (!g_ball.active) {
        g_ball.x = g_paddle_x + g_paddle_w / 2;
        g_ball.y = PADDLE_Y - BALL_SIZE - 1;
        return;
    }

    /* Compute next position */
    nx = (int16_t)g_ball.x + g_ball.dx;
    ny = (int16_t)g_ball.y + g_ball.dy;

    /* Wall collisions */
    if (nx <= PF_LEFT + 2) {
        g_ball.dx = -g_ball.dx;
        nx = PF_LEFT + 3;
        sfx_bounce();
    }
    if (nx + BALL_SIZE >= PF_LEFT + PF_WIDTH - 2) {
        g_ball.dx = -g_ball.dx;
        nx = PF_LEFT + PF_WIDTH - 2 - BALL_SIZE - 1;
        sfx_bounce();
    }
    if (ny <= PF_TOP + 2) {
        g_ball.dy = -g_ball.dy;
        ny = PF_TOP + 3;
        sfx_bounce();
    }

    /* Death */
    if (ny >= PF_BOTTOM) {
        /* Erase ball at its current position */
        vid_fill_rect(g_ball.x, g_ball.y, BALL_SIZE, BALL_SIZE, COL_BLACK);
        g_ball.active = 0;
        g_lives--;
        g_dirty |= DIRTY_LIVES;
        sfx_lose_life();
        if (g_lives == 0) {
            g_state = STATE_GAMEOVER;
        } else {
            g_state = STATE_LAUNCH;
            game_reset_ball();
        }
        return;
    }

    /* Paddle collision */
    if (g_ball.dy > 0 && ny + BALL_SIZE >= PADDLE_Y && ny < PADDLE_Y + PADDLE_H) {
        if (nx + BALL_SIZE >= (int16_t)g_paddle_x && nx <= (int16_t)(g_paddle_x + g_paddle_w)) {
            int16_t hit = nx + BALL_SIZE / 2 - (int16_t)g_paddle_x;
            int16_t pw = g_paddle_w;
            g_ball.dy = -g_ball.dy;
            ny = PADDLE_Y - BALL_SIZE - 1;

            if (hit < pw / 5)          g_ball.dx = -2;
            else if (hit < pw * 2 / 5) g_ball.dx = -1;
            else if (hit < pw * 3 / 5) g_ball.dx = g_ball.dx > 0 ? 1 : -1;
            else if (hit < pw * 4 / 5) g_ball.dx = 1;
            else                        g_ball.dx = 2;
            sfx_bounce();
        }
    }

    bx = nx;
    by = ny;

    /* Vertical brick collision (top/bottom edge of ball) */
    cx = bx + BALL_SIZE / 2 - GRID_LEFT;
    if (cx >= 0 && cx < BRICK_COLS * BRICK_W) {
        gc = (uint8_t)(cx / BRICK_W);

        ty = by - GRID_TOP;
        if (ty >= 0 && ty < BRICK_ROWS * BRICK_H) {
            gr = (uint8_t)(ty / BRICK_H);
            if (g_bricks[gr][gc] != BRK_EMPTY) {
                hit_brick(gr, gc);
                g_ball.dy = -g_ball.dy;
                ny = (int16_t)g_ball.y;
                goto done;
            }
        }
        ty = by + BALL_SIZE - GRID_TOP;
        if (ty >= 0 && ty < BRICK_ROWS * BRICK_H) {
            gr = (uint8_t)(ty / BRICK_H);
            if (g_bricks[gr][gc] != BRK_EMPTY) {
                hit_brick(gr, gc);
                g_ball.dy = -g_ball.dy;
                ny = (int16_t)g_ball.y;
                goto done;
            }
        }
    }

    /* Horizontal brick collision (left/right edge of ball) */
    ty = by + BALL_SIZE / 2 - GRID_TOP;
    if (ty >= 0 && ty < BRICK_ROWS * BRICK_H) {
        gr = (uint8_t)(ty / BRICK_H);

        cx = bx - GRID_LEFT;
        if (cx >= 0 && cx < BRICK_COLS * BRICK_W) {
            gc = (uint8_t)(cx / BRICK_W);
            if (g_bricks[gr][gc] != BRK_EMPTY) {
                hit_brick(gr, gc);
                g_ball.dx = -g_ball.dx;
                nx = (int16_t)g_ball.x;
                goto done;
            }
        }
        cx = bx + BALL_SIZE - GRID_LEFT;
        if (cx >= 0 && cx < BRICK_COLS * BRICK_W) {
            gc = (uint8_t)(cx / BRICK_W);
            if (g_bricks[gr][gc] != BRK_EMPTY) {
                hit_brick(gr, gc);
                g_ball.dx = -g_ball.dx;
                nx = (int16_t)g_ball.x;
                goto done;
            }
        }
    }

    /* Corner collision checks (catches diagonal hits) */
    {
        int16_t cxc, cyc;
        /* Top-left */
        cxc = bx - GRID_LEFT;
        cyc = by - GRID_TOP;
        if (cxc >= 0 && cxc < BRICK_COLS * BRICK_W &&
            cyc >= 0 && cyc < BRICK_ROWS * BRICK_H) {
            gr = (uint8_t)(cyc / BRICK_H);
            gc = (uint8_t)(cxc / BRICK_W);
            if (g_bricks[gr][gc] != BRK_EMPTY) {
                hit_brick(gr, gc);
                g_ball.dy = -g_ball.dy;
                g_ball.dx = -g_ball.dx;
                nx = (int16_t)g_ball.x;
                ny = (int16_t)g_ball.y;
                goto done;
            }
        }
        /* Top-right */
        cxc = bx + BALL_SIZE - GRID_LEFT;
        cyc = by - GRID_TOP;
        if (cxc >= 0 && cxc < BRICK_COLS * BRICK_W &&
            cyc >= 0 && cyc < BRICK_ROWS * BRICK_H) {
            gr = (uint8_t)(cyc / BRICK_H);
            gc = (uint8_t)(cxc / BRICK_W);
            if (g_bricks[gr][gc] != BRK_EMPTY) {
                hit_brick(gr, gc);
                g_ball.dy = -g_ball.dy;
                g_ball.dx = -g_ball.dx;
                nx = (int16_t)g_ball.x;
                ny = (int16_t)g_ball.y;
                goto done;
            }
        }
        /* Bottom-left */
        cxc = bx - GRID_LEFT;
        cyc = by + BALL_SIZE - GRID_TOP;
        if (cxc >= 0 && cxc < BRICK_COLS * BRICK_W &&
            cyc >= 0 && cyc < BRICK_ROWS * BRICK_H) {
            gr = (uint8_t)(cyc / BRICK_H);
            gc = (uint8_t)(cxc / BRICK_W);
            if (g_bricks[gr][gc] != BRK_EMPTY) {
                hit_brick(gr, gc);
                g_ball.dy = -g_ball.dy;
                g_ball.dx = -g_ball.dx;
                nx = (int16_t)g_ball.x;
                ny = (int16_t)g_ball.y;
                goto done;
            }
        }
        /* Bottom-right */
        cxc = bx + BALL_SIZE - GRID_LEFT;
        cyc = by + BALL_SIZE - GRID_TOP;
        if (cxc >= 0 && cxc < BRICK_COLS * BRICK_W &&
            cyc >= 0 && cyc < BRICK_ROWS * BRICK_H) {
            gr = (uint8_t)(cyc / BRICK_H);
            gc = (uint8_t)(cxc / BRICK_W);
            if (g_bricks[gr][gc] != BRK_EMPTY) {
                hit_brick(gr, gc);
                g_ball.dy = -g_ball.dy;
                g_ball.dx = -g_ball.dx;
                nx = (int16_t)g_ball.x;
                ny = (int16_t)g_ball.y;
                goto done;
            }
        }
    }

done:
    g_ball.x = (uint16_t)nx;
    g_ball.y = (uint16_t)ny;
}
