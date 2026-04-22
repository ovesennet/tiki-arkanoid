/* TIKI ARKANOID — enemy.c
 * Enemy spawning, movement, collision, and drawing.
 *
 * Enemies drift downward through the playfield as interference.
 * Ball or paddle contact destroys them for bonus points.
 * Stage completion depends only on breakable bricks, not enemies.
 */

#include "enemy.h"
#include "game.h"
#include "video.h"
#include "sound.h"

Enemy g_enemies[MAX_ENEMIES];

/* Previous positions for flicker-free erase */
static int16_t s_prev_x[MAX_ENEMIES];
static int16_t s_prev_y[MAX_ENEMIES];

/* Spawn timer (counts down each frame) */
static uint16_t s_spawn_timer;

/* Simple PRNG — reuse game's xorshift style */
static uint8_t s_rng = 0xA3;

static uint8_t erng(void)
{
    s_rng ^= (s_rng << 3);
    s_rng ^= (s_rng >> 5);
    s_rng ^= (s_rng << 4);
    return s_rng;
}

static uint16_t erng_range(uint16_t lo, uint16_t hi)
{
    return lo + (erng() % (hi - lo + 1));
}

/* ── Sprite drawing ──
 * Draws a small alien-like shape using rectangles.
 * Type 0: diamond body with antennae (cyan/green).
 * Type 1: wider blocky body (magenta/pink).
 */
/* ── Sprite drawing ──
 * Draws a small alien-like shape using rectangles.
 * Type 0: diamond body with antennae (cyan/green).
 * Type 1: wider blocky body (magenta/pink).
 */
static void draw_enemy_sprite(int16_t x, int16_t y, uint8_t type, uint8_t anim)
{
    if (type == 0) {
        /* Diamond alien — cyan body */
        uint8_t shift = (anim & 4) ? 1 : 0; /* wobble antennae */
        vid_fill_rect(x + 1, y,     6, 1, COL_CYAN);
        vid_fill_rect(x + shift, y + 1, 8, 1, COL_BRCYAN);
        vid_fill_rect(x,     y + 2, 8, 4, COL_CYAN);
        vid_fill_rect(x + 2, y + 3, 1, 2, COL_WHITE);
        vid_fill_rect(x + 5, y + 3, 1, 2, COL_WHITE);
        vid_fill_rect(x + 1, y + 6, 6, 1, COL_BRCYAN);
        vid_fill_rect(x + 2, y + 7, 2, 1, COL_CYAN);
        vid_fill_rect(x + 4 + shift, y + 7, 2, 1, COL_CYAN);
    } else {
        /* Blocky alien — magenta body */
        uint8_t shift = (anim & 4) ? 1 : 0;
        vid_fill_rect(x + 2, y,     4, 1, COL_MAGENTA);
        vid_fill_rect(x + 1, y + 1, 6, 1, COL_PINK);
        vid_fill_rect(x,     y + 2, 8, 4, COL_MAGENTA);
        vid_fill_rect(x + 1, y + 3, 2, 2, COL_WHITE);
        vid_fill_rect(x + 5, y + 3, 2, 2, COL_WHITE);
        vid_fill_rect(x + 1, y + 6, 6, 1, COL_PINK);
        vid_fill_rect(x + shift, y + 7, 3, 1, COL_MAGENTA);
        vid_fill_rect(x + 5 - shift, y + 7, 3, 1, COL_MAGENTA);
    }
}

/* Erase area must be wider than sprite because wobble shift draws
 * up to 1px beyond ENEMY_W on each side. Use 10x9 erase. */
#define ERASE_W  10
#define ERASE_HT  9

static void erase_enemy_rect(int16_t x, int16_t y)
{
    int16_t ex, ew;
    if (x < PF_LEFT || y < PF_TOP) return;
    /* Clamp erase rect to stay inside playfield borders */
    ex = x;
    ew = ERASE_W;
    if (ex < PF_LEFT + 2) {
        ew -= (PF_LEFT + 2 - ex);
        ex = PF_LEFT + 2;
    }
    if (ex + ew > PF_LEFT + PF_WIDTH - 2)
        ew = PF_LEFT + PF_WIDTH - 2 - ex;
    if (ew > 0)
        vid_fill_rect((uint16_t)ex, (uint16_t)y, (uint8_t)ew, ERASE_HT, COL_BLACK);
}

/* Repair border lines that the ASM erase (10x9 black rect) may damage */
static void repair_border(int16_t x, int16_t y)
{
    /* Left border: x = PF_LEFT .. PF_LEFT+1 */
    if (x < PF_LEFT + 2 && x + ERASE_W > PF_LEFT) {
        uint8_t h = ERASE_HT;
        if (y + h > PF_BOTTOM) h = (uint8_t)(PF_BOTTOM - y);
        vid_fill_rect(PF_LEFT, (uint16_t)y, 2, h, COL_DKGREY);
    }
    /* Right border: x = PF_LEFT+PF_WIDTH-2 .. PF_LEFT+PF_WIDTH-1 */
    if (x + ERASE_W > PF_LEFT + PF_WIDTH - 2 && x < PF_LEFT + PF_WIDTH) {
        uint8_t h = ERASE_HT;
        if (y + h > PF_BOTTOM) h = (uint8_t)(PF_BOTTOM - y);
        vid_fill_rect(PF_LEFT + PF_WIDTH - 2, (uint16_t)y, 2, h, COL_DKGREY);
    }
}

/* Redraw any bricks that overlap the erased enemy rectangle */
static void repair_bricks(int16_t x, int16_t y)
{
    int16_t c0, c1, r0, r1;
    int8_t c, r;

    c0 = (x - GRID_LEFT) / BRICK_W;
    c1 = (x + ERASE_W - 1 - GRID_LEFT) / BRICK_W;
    r0 = (y - GRID_TOP) / BRICK_H;
    r1 = (y + ERASE_HT - 1 - GRID_TOP) / BRICK_H;

    if (c0 < 0) c0 = 0;
    if (c1 >= BRICK_COLS) c1 = BRICK_COLS - 1;
    if (r0 < 0) r0 = 0;
    if (r1 >= BRICK_ROWS) r1 = BRICK_ROWS - 1;

    for (r = (int8_t)r0; r <= (int8_t)r1; r++)
        for (c = (int8_t)c0; c <= (int8_t)c1; c++)
            if (g_bricks[r][c] != BRK_EMPTY)
                game_draw_brick((uint8_t)r, (uint8_t)c);
}

/* ── Init / Reset ── */

void enemy_init(void)
{
    uint8_t i;
    for (i = 0; i < MAX_ENEMIES; i++)
        g_enemies[i].active = 0;
    s_spawn_timer = (uint16_t)erng_range(ENEMY_SPAWN_MIN, ENEMY_SPAWN_MAX);
}

void enemy_reset(void)
{
    uint8_t i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (g_enemies[i].active) {
            erase_enemy_rect(s_prev_x[i], s_prev_y[i]);
            repair_border(s_prev_x[i], s_prev_y[i]);
            repair_bricks(s_prev_x[i], s_prev_y[i]);
        }
        g_enemies[i].active = 0;
    }
    s_spawn_timer = (uint16_t)erng_range(ENEMY_SPAWN_MIN, ENEMY_SPAWN_MAX);
}

/* ── Spawn ── */

static uint8_t enemy_hits_brick(int16_t ex, int16_t ey);

static void try_spawn(void)
{
    uint8_t i;
    int16_t sx;

    /* Find a free slot */
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!g_enemies[i].active)
            break;
    }
    if (i >= MAX_ENEMIES) return; /* all slots busy */

    /* Random x within playfield, away from border (erase rect is 10px wide) */
    sx = PF_LEFT + 4 + (int16_t)erng_range(0, (uint16_t)(PF_WIDTH - ERASE_W - 6));

    /* Find a spawn Y that doesn't overlap bricks */
    {
        int16_t sy = PF_TOP + 3;
        while (sy + ENEMY_HT < PADDLE_Y && enemy_hits_brick(sx, sy))
            sy += BRICK_H;
        if (sy + ENEMY_HT >= PADDLE_Y) return; /* no room */
        g_enemies[i].y = sy;
    }

    g_enemies[i].x = sx;
    g_enemies[i].dy = 1;         /* move down */
    g_enemies[i].dx = (erng() & 1) ? 1 : -1;
    g_enemies[i].active = 1;
    g_enemies[i].type = erng() & 1;
    g_enemies[i].anim = 0;
    g_enemies[i].steps = (uint8_t)erng_range(6, 14); /* sideways steps before drop */
    s_prev_x[i] = g_enemies[i].x;
    s_prev_y[i] = g_enemies[i].y;

    s_spawn_timer = (uint16_t)erng_range(ENEMY_SPAWN_MIN, ENEMY_SPAWN_MAX);
}

/* ── AABB overlap test ── */

static uint8_t rects_overlap(int16_t ax, int16_t ay, uint8_t aw, uint8_t ah,
                             int16_t bx, int16_t by, uint8_t bw, uint8_t bh)
{
    if (ax + aw <= bx) return 0;
    if (bx + bw <= ax) return 0;
    if (ay + ah <= by) return 0;
    if (by + bh <= ay) return 0;
    return 1;
}

/* ── Brick collision for enemies ──
 * Check if an enemy-sized rectangle at (ex,ey) overlaps any brick.
 * Returns 1 if blocked, 0 if clear. */
static uint8_t enemy_hits_brick(int16_t ex, int16_t ey)
{
    int16_t c0, c1, r0, r1;
    int8_t c, r;

    /* Map enemy corners to brick grid */
    c0 = (ex - GRID_LEFT) / BRICK_W;
    c1 = (ex + ENEMY_W - 1 - GRID_LEFT) / BRICK_W;
    r0 = (ey - GRID_TOP) / BRICK_H;
    r1 = (ey + ENEMY_HT - 1 - GRID_TOP) / BRICK_H;

    /* Clamp to valid grid range */
    if (c0 < 0) c0 = 0;
    if (c1 >= BRICK_COLS) c1 = BRICK_COLS - 1;
    if (r0 < 0) r0 = 0;
    if (r1 >= BRICK_ROWS) r1 = BRICK_ROWS - 1;

    for (r = (int8_t)r0; r <= (int8_t)r1; r++)
        for (c = (int8_t)c0; c <= (int8_t)c1; c++)
            if (g_bricks[r][c] != BRK_EMPTY)
                return 1;
    return 0;
}

/* ── Update ── */

void enemy_update(void)
{
    uint8_t i;

    /* Spawn timer */
    if (s_spawn_timer > 0)
        s_spawn_timer--;
    if (s_spawn_timer == 0)
        try_spawn();

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!g_enemies[i].active)
            continue;

        g_enemies[i].anim++;

        /* Slow movement: only move every 4th frame */
        if ((g_enemies[i].anim & 3) == 0) {
            int16_t nx, ny;

            if (g_enemies[i].steps > 0) {
                /* ── Sideways phase ── */
                nx = g_enemies[i].x + g_enemies[i].dx;
                if (nx < PF_LEFT + 2 || nx + ERASE_W > PF_LEFT + PF_WIDTH - 2
                    || enemy_hits_brick(nx, g_enemies[i].y)) {
                    g_enemies[i].dx = -g_enemies[i].dx;
                } else {
                    g_enemies[i].x = nx;
                }
                g_enemies[i].steps--;
            } else {
                /* ── Vertical step (the "square corner") ── */
                /* Always prefer downward */
                g_enemies[i].dy = 1;
                ny = g_enemies[i].y + g_enemies[i].dy;
                if (enemy_hits_brick(g_enemies[i].x, ny)) {
                    /* Blocked below — try upward once */
                    g_enemies[i].dy = -1;
                    ny = g_enemies[i].y - 1;
                    if (ny > PF_TOP + 2 && !enemy_hits_brick(g_enemies[i].x, ny))
                        g_enemies[i].y = ny;
                } else {
                    g_enemies[i].y = ny;
                }
                /* Very short sideways runs = fast descent */
                g_enemies[i].steps = (uint8_t)erng_range(1, 4);
                if ((erng() & 3) == 0)
                    g_enemies[i].dx = -g_enemies[i].dx;
            }
        }

        /* Ball collision */
        if (g_ball.active &&
            rects_overlap(g_enemies[i].x, g_enemies[i].y, ENEMY_W, ENEMY_HT,
                          (int16_t)g_ball.x, (int16_t)g_ball.y, BALL_SIZE, BALL_SIZE))
        {
            /* Award points */
            g_score += ENEMY_SCORE;
            g_dirty |= DIRTY_SCORE;

            /* Bounce ball vertically */
            g_ball.dy = -g_ball.dy;

            /* Erase and deactivate */
            erase_enemy_rect(s_prev_x[i], s_prev_y[i]);
            repair_border(s_prev_x[i], s_prev_y[i]);
            repair_bricks(s_prev_x[i], s_prev_y[i]);
            erase_enemy_rect(g_enemies[i].x, g_enemies[i].y);
            repair_border(g_enemies[i].x, g_enemies[i].y);
            repair_bricks(g_enemies[i].x, g_enemies[i].y);
            g_enemies[i].active = 0;
            sfx_brick();
            continue;
        }

        /* Paddle collision */
        if (rects_overlap(g_enemies[i].x, g_enemies[i].y, ENEMY_W, ENEMY_HT,
                          (int16_t)g_paddle_x, PADDLE_Y, g_paddle_w, PADDLE_H))
        {
            g_score += ENEMY_SCORE;
            g_dirty |= DIRTY_SCORE;
            erase_enemy_rect(s_prev_x[i], s_prev_y[i]);
            repair_border(s_prev_x[i], s_prev_y[i]);
            repair_bricks(s_prev_x[i], s_prev_y[i]);
            erase_enemy_rect(g_enemies[i].x, g_enemies[i].y);
            repair_border(g_enemies[i].x, g_enemies[i].y);
            repair_bricks(g_enemies[i].x, g_enemies[i].y);
            g_enemies[i].active = 0;
            sfx_bounce();
            continue;
        }

        /* Off-screen bottom */
        if (g_enemies[i].y > PF_BOTTOM) {
            erase_enemy_rect(s_prev_x[i], s_prev_y[i]);
            repair_border(s_prev_x[i], s_prev_y[i]);
            repair_bricks(s_prev_x[i], s_prev_y[i]);
            g_enemies[i].active = 0;
            continue;
        }
    }
}

/* ── Draw (flicker-free: erase old + draw new in single VRAM bank switch) ── */

void enemy_draw(void)
{
    uint8_t i, frame;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!g_enemies[i].active) continue;

        frame = (g_enemies[i].anim & 4) ? 1 : 0;
        vid_move_enemy(s_prev_x[i], s_prev_y[i],
                       g_enemies[i].x, g_enemies[i].y,
                       g_enemies[i].type, frame);
        repair_border(s_prev_x[i], s_prev_y[i]);
        repair_bricks(s_prev_x[i], s_prev_y[i]);

        s_prev_x[i] = g_enemies[i].x;
        s_prev_y[i] = g_enemies[i].y;
    }
}

void enemy_draw_all(void)
{
    uint8_t i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!g_enemies[i].active) continue;
        draw_enemy_sprite(g_enemies[i].x, g_enemies[i].y,
                          g_enemies[i].type, g_enemies[i].anim);
        s_prev_x[i] = g_enemies[i].x;
        s_prev_y[i] = g_enemies[i].y;
    }
}

void enemy_erase_all(void)
{
    uint8_t i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (g_enemies[i].active) {
            erase_enemy_rect(s_prev_x[i], s_prev_y[i]);
            repair_border(s_prev_x[i], s_prev_y[i]);
            repair_bricks(s_prev_x[i], s_prev_y[i]);
        }
    }
}
