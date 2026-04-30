/* TIKI ARKANOID — game.c
 * Core game logic: paddle, ball, bricks, collision, drawing.
 */

#include "game.h"
#include "levels.h"
#include "video.h"
#include "font.h"
#include "sound.h"
#include "enemy.h"
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
Capsule  g_capsule;
uint8_t  g_paddle_mode;
Laser    g_lasers[MAX_LASERS];

/* Power-up duration timer (frames) */
static uint16_t s_powerup_timer;
#define POWERUP_DURATION 600   /* ~12 seconds at 50 Hz */

/* Extra balls for Disruption power-up (indices 0..1) */
static Ball s_extra_balls[MAX_BALLS - 1];
static uint16_t s_extra_prev_x[MAX_BALLS - 1];
static uint16_t s_extra_prev_y[MAX_BALLS - 1];

/* Previous paddle position for delta redraw */
static uint16_t s_prev_paddle_x;
static uint8_t  s_prev_paddle_w;

/* Previous ball position for flicker-free move */
static uint16_t s_prev_ball_x;
static uint16_t s_prev_ball_y;

/* Laser cooldown counter */
static uint8_t s_laser_cooldown;

/* ── Simple PRNG (xorshift8) ── */
static uint8_t s_rng = 0x5A;

static uint8_t rng_next(void)
{
    s_rng ^= (s_rng << 3);
    s_rng ^= (s_rng >> 5);
    s_rng ^= (s_rng << 4);
    return s_rng;
}

/* Random number in range [lo..hi] inclusive */
static uint8_t rng_range(uint8_t lo, uint8_t hi)
{
    return lo + (rng_next() % (hi - lo + 1));
}

/* ── Capsule state ── */
static uint8_t s_bricks_until_cap;  /* bricks to break before next capsule */
static uint16_t s_prev_cap_x;       /* previous capsule draw position */
static uint16_t s_prev_cap_y;

/* Capsule colour per type (background colour of capsule) */
static const uint8_t cap_colour[] = {
    COL_BLACK,    /* 0: none */
    COL_ORANGE,   /* 1: S slow */
    COL_GREEN,    /* 2: C catch */
    COL_BLUE,     /* 3: E enlarge */
    COL_RED,      /* 4: D disruption */
    COL_LTGREY,   /* 5: L laser */
    COL_BRCYAN,   /* 6: B break */
    COL_MAGENTA,  /* 7: P player */
};

/* Letter per capsule type */
static const char cap_letter[] = {
    ' ', 'S', 'C', 'E', 'D', 'L', 'B', 'P'
};

static void capsule_reset_counter(void)
{
    s_bricks_until_cap = rng_range(7, 15);
}

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
    COL_CYAN,     /* 10: gold (indestructible, dark cyan) */
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
    s_prev_paddle_w = g_paddle_w;
    g_paddle_mode = PMODE_NORMAL;
    s_powerup_timer = 0;
    g_capsule.type = CAP_NONE;
    capsule_reset_counter();
    g_lasers[0].active = 0;
    g_lasers[1].active = 0;
    s_laser_cooldown = 0;
    g_dirty = DIRTY_ALL;
    enemy_init();
    game_load_stage(1);
    game_reset_ball();
}

void game_load_stage(uint8_t stage)
{
    uint8_t r, c;
    const uint8_t *src;

    memset(g_bricks, BRK_EMPTY, sizeof(g_bricks));
    g_bricks_left = 0;
    g_capsule.type = CAP_NONE;
    g_paddle_mode = PMODE_NORMAL;
    g_paddle_w = PADDLE_W;
    s_powerup_timer = 0;
    s_extra_balls[0].active = 0;
    s_extra_balls[1].active = 0;
    g_lasers[0].active = 0;
    g_lasers[1].active = 0;
    s_laser_cooldown = 0;
    capsule_reset_counter();
    enemy_reset();

    if (stage < 1) stage = 1;
    if (stage > NUM_LEVELS) stage = ((stage - 1) % NUM_LEVELS) + 1;
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
        vid_fill_rect(px, py, BRICK_W, BRICK_H, COL_BLACK);
    } else {
        uint8_t c = brick_colour[b];
        /* Draw 14x7 visible area, leave 2px right + 1px bottom as black border */
        vid_fill_rect(px, py, BRICK_W - 2, BRICK_H - 1, c);
        /* Black right border (2px) */
        vid_fill_rect(px + BRICK_W - 2, py, 2, BRICK_H, COL_BLACK);
        /* Black bottom border (1px) */
        vid_hline(px, px + BRICK_W - 3, py + BRICK_H - 1, COL_BLACK);
        if (b == BRK_SILVER || b == BRK_GOLD) {
            vid_hline(px, px + BRICK_W - 3, py, COL_WHITE);
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

    draw_text_centred(4, "TIKI ARKANOID BY ARCTIC RETRO", COL_GREEN);
}

/* Paddle body colour based on current mode */
static uint8_t paddle_colour(void)
{
    switch (g_paddle_mode) {
    case PMODE_CATCH:   return COL_GREEN;
    case PMODE_LASER:   return COL_RED;
    case PMODE_ENLARGE: return COL_BLUE;
    default:            return COL_BRCYAN;
    }
}

/* Flicker-free paddle: full erase + full redraw in single VRAM bank switch. */
void game_draw_paddle(void)
{
    uint16_t old_x = s_prev_paddle_x;
    uint16_t new_x = g_paddle_x;
    uint8_t old_w = s_prev_paddle_w;
    uint8_t w = g_paddle_w;
    uint8_t pc = paddle_colour();

    if (old_x == new_x && old_w == w) return;

    vid_move_paddle(old_x, new_x, old_w, w, PADDLE_H, PADDLE_Y, pc);

    s_prev_paddle_x = new_x;
    s_prev_paddle_w = w;
}

void game_draw_paddle_force(void)
{
    uint8_t pc = paddle_colour();
    vid_hline(g_paddle_x, g_paddle_x + g_paddle_w - 1, PADDLE_Y, COL_WHITE);
    vid_fill_rect(g_paddle_x, PADDLE_Y + 1, g_paddle_w, PADDLE_H - 1, pc);
    s_prev_paddle_x = g_paddle_x;
    s_prev_paddle_w = g_paddle_w;
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
        vid_fill_rect(SIDE_X, SIDE_Y + 10, 36, FONT_CH, COL_BLACK);
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
    vid_fill_rect(px, py, BRICK_W - 2, BRICK_H - 1, COL_WHITE);
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

    /* Capsule spawn check — only from normal (non-silver) bricks */
    if (g_capsule.type == CAP_NONE) {
        if (s_bricks_until_cap > 0)
            s_bricks_until_cap--;
        if (s_bricks_until_cap == 0) {
            /* Spawn a random capsule at the destroyed brick's position */
            g_capsule.type = rng_range(1, NUM_CAP_TYPES - 1);
            g_capsule.x = GRID_LEFT + (uint16_t)c * BRICK_W;
            g_capsule.y = GRID_TOP + (uint16_t)r * BRICK_H;
            s_prev_cap_x = g_capsule.x;
            s_prev_cap_y = g_capsule.y;
            capsule_reset_counter();
        }
    }
}

/* ── Capsule drawing ── */

static void capsule_draw(uint16_t x, uint16_t y, uint8_t type)
{
    uint8_t col = cap_colour[type];
    char letter[2];
    vid_fill_rect(x, y, CAP_W, CAP_H, col);
    /* Draw letter centred in capsule */
    letter[0] = cap_letter[type];
    letter[1] = '\0';
    draw_text(x + 3, y, letter, COL_WHITE);
}

static void capsule_erase(uint16_t x, uint16_t y)
{
    vid_fill_rect(x, y, CAP_W, FONT_CH, COL_BLACK);
}

static void capsule_apply(uint8_t type)
{
    sfx_bounce();  /* audible feedback for any pickup */
    switch (type) {
    case CAP_SLOW:
        /* Reduce ball speed */
        if (g_ball.dy < -1) g_ball.dy = -1;
        else if (g_ball.dy > 1) g_ball.dy = 1;
        if (g_ball.dx < -1) g_ball.dx = -1;
        else if (g_ball.dx > 1) g_ball.dx = 1;
        g_paddle_mode = PMODE_NORMAL;
        goto redraw_paddle;
    case CAP_ENLARGE:
        g_paddle_mode = PMODE_ENLARGE;
        s_powerup_timer = POWERUP_DURATION;
        /* Erase old paddle, widen, redraw */
        vid_fill_rect(g_paddle_x, PADDLE_Y, g_paddle_w, PADDLE_H, COL_BLACK);
        g_paddle_w = PADDLE_WIDE_W;
        /* Clamp paddle position */
        if (g_paddle_x + g_paddle_w > PADDLE_MAX_X)
            g_paddle_x = PADDLE_MAX_X - g_paddle_w;
        s_prev_paddle_x = g_paddle_x;
        game_draw_paddle_force();
        break;
    case CAP_PLAYER:
        g_lives++;
        g_dirty |= DIRTY_LIVES;
        break;
    case CAP_CATCH:
        g_paddle_mode = PMODE_CATCH;
        s_powerup_timer = POWERUP_DURATION;
        goto redraw_paddle;
    case CAP_LASER:
        g_paddle_mode = PMODE_LASER;
        s_powerup_timer = POWERUP_DURATION;
        goto redraw_paddle;
    case CAP_DISRUPT:
        /* Spawn 2 extra balls from current ball position */
        s_extra_balls[0].active = 1;
        s_extra_balls[0].x = g_ball.x;
        s_extra_balls[0].y = g_ball.y;
        s_extra_balls[0].dx = -g_ball.dx;  /* mirror horizontal */
        s_extra_balls[0].dy = g_ball.dy;
        s_extra_prev_x[0] = g_ball.x;
        s_extra_prev_y[0] = g_ball.y;

        s_extra_balls[1].active = 1;
        s_extra_balls[1].x = g_ball.x;
        s_extra_balls[1].y = g_ball.y;
        s_extra_balls[1].dx = (g_ball.dx > 0) ? -2 : 2;
        s_extra_balls[1].dy = g_ball.dy;
        s_extra_prev_x[1] = g_ball.x;
        s_extra_prev_y[1] = g_ball.y;
        break;
    case CAP_BREAK:
        /* Clear all breakable bricks — stage will advance on next update */
        {
            uint8_t br, bc;
            for (br = 0; br < BRICK_ROWS; br++)
                for (bc = 0; bc < BRICK_COLS; bc++)
                    if (g_bricks[br][bc] != BRK_EMPTY &&
                        g_bricks[br][bc] != BRK_GOLD) {
                        g_bricks[br][bc] = BRK_EMPTY;
                        game_draw_brick(br, bc);
                    }
            g_bricks_left = 0;
        }
        break;
    }
    return;
redraw_paddle:
    /* Erase old (possibly wide) paddle, reset width if leaving enlarge */
    vid_fill_rect(g_paddle_x, PADDLE_Y, g_paddle_w, PADDLE_H, COL_BLACK);
    if (g_paddle_mode != PMODE_ENLARGE)
        g_paddle_w = PADDLE_W;
    game_draw_paddle_force();
}

/* Redraw any bricks that overlap the given pixel rectangle */
static void repair_bricks(uint16_t px, uint16_t py, uint8_t pw, uint8_t ph)
{
    int16_t r0, r1, c0, c1;
    int16_t r, c;

    /* Convert pixel coords to grid coords (clamped) */
    c0 = ((int16_t)px - GRID_LEFT) / BRICK_W;
    c1 = ((int16_t)px + pw - 1 - GRID_LEFT) / BRICK_W;
    r0 = ((int16_t)py - GRID_TOP) / BRICK_H;
    r1 = ((int16_t)py + ph - 1 - GRID_TOP) / BRICK_H;

    if (c0 < 0) c0 = 0;
    if (r0 < 0) r0 = 0;
    if (c1 >= BRICK_COLS) c1 = BRICK_COLS - 1;
    if (r1 >= BRICK_ROWS) r1 = BRICK_ROWS - 1;

    for (r = r0; r <= r1; r++)
        for (c = c0; c <= c1; c++)
            if (g_bricks[r][c] != BRK_EMPTY)
                game_draw_brick((uint8_t)r, (uint8_t)c);
}

void game_update_capsule(void)
{
    uint8_t col;
    char letter[2];

    if (g_capsule.type == CAP_NONE) return;

    /* Move down */
    g_capsule.y += CAP_FALL_SPEED;

    /* Off-screen? */
    if (g_capsule.y >= PF_BOTTOM) {
        capsule_erase(s_prev_cap_x, s_prev_cap_y);
        repair_bricks(s_prev_cap_x, s_prev_cap_y, CAP_W, FONT_CH);
        g_capsule.type = CAP_NONE;
        return;
    }

    /* Paddle collision? */
    if (g_capsule.y + CAP_H >= PADDLE_Y &&
        g_capsule.y < PADDLE_Y + PADDLE_H &&
        g_capsule.x + CAP_W >= g_paddle_x &&
        g_capsule.x <= g_paddle_x + g_paddle_w) {
        /* Erase capsule at both previous and current positions */
        capsule_erase(s_prev_cap_x, s_prev_cap_y);
        repair_bricks(s_prev_cap_x, s_prev_cap_y, CAP_W, FONT_CH);
        capsule_erase(g_capsule.x, g_capsule.y);
        repair_bricks(g_capsule.x, g_capsule.y, CAP_W, FONT_CH);
        /* Clear any pixels that leaked below the paddle */
        vid_fill_rect(g_capsule.x, PADDLE_Y + PADDLE_H,
                      CAP_W, CAP_H, COL_BLACK);
        /* Erase full paddle area before apply (capsule erase may
           have punched holes in the paddle body) */
        vid_fill_rect(s_prev_paddle_x, PADDLE_Y,
                      s_prev_paddle_w, PADDLE_H, COL_BLACK);
        capsule_apply(g_capsule.type);
        /* Full redraw at current position */
        game_draw_paddle_force();
        g_capsule.type = CAP_NONE;
        return;
    }

    /* Flicker-free move: erase old + draw new body in one bank switch */
    col = cap_colour[g_capsule.type];
    vid_move_capsule(s_prev_cap_x, s_prev_cap_y,
                     g_capsule.x, g_capsule.y,
                     CAP_W, CAP_H, col);

    /* Repair any bricks damaged by erasing the old capsule position */
    repair_bricks(s_prev_cap_x, s_prev_cap_y, CAP_W, CAP_H);

    /* Overlay letter (needs its own bank switch but is small) */
    letter[0] = cap_letter[g_capsule.type];
    letter[1] = '\0';
    draw_text(g_capsule.x + 3, g_capsule.y, letter, COL_WHITE);

    s_prev_cap_x = g_capsule.x;
    s_prev_cap_y = g_capsule.y;
}

/* ── Laser ── */

void game_fire_laser(void)
{
    uint8_t i;
    if (g_paddle_mode != PMODE_LASER) return;
    if (s_laser_cooldown > 0) { s_laser_cooldown--; return; }

    for (i = 0; i < MAX_LASERS; i++) {
        if (!g_lasers[i].active) {
            g_lasers[i].active = 1;
            /* Shoot from left edge of paddle */
            g_lasers[i].x = g_paddle_x + 2;
            g_lasers[i].y = PADDLE_Y - LASER_H;
            /* Find second slot for right bolt */
            {
                uint8_t j;
                for (j = i + 1; j < MAX_LASERS; j++) {
                    if (!g_lasers[j].active) {
                        g_lasers[j].active = 1;
                        g_lasers[j].x = g_paddle_x + g_paddle_w - 4;
                        g_lasers[j].y = PADDLE_Y - LASER_H;
                        break;
                    }
                }
            }
            s_laser_cooldown = LASER_COOLDOWN;
            sfx_laser();
            return;
        }
    }
}

void game_update_lasers(void)
{
    uint8_t i;
    int16_t cx, cy;
    uint8_t gr, gc;

    for (i = 0; i < MAX_LASERS; i++) {
        if (!g_lasers[i].active) continue;

        /* Erase old position */
        vid_fill_rect(g_lasers[i].x, g_lasers[i].y, LASER_W, LASER_H, COL_BLACK);
        repair_bricks(g_lasers[i].x, g_lasers[i].y, LASER_W, LASER_H);

        /* Move up */
        if (g_lasers[i].y < PF_TOP + 2 + LASER_SPEED) {
            g_lasers[i].active = 0;
            continue;
        }
        g_lasers[i].y -= LASER_SPEED;

        /* Check brick collision */
        cx = (int16_t)g_lasers[i].x - GRID_LEFT;
        cy = (int16_t)g_lasers[i].y - GRID_TOP;
        if (cx >= 0 && cx < BRICK_COLS * BRICK_W &&
            cy >= 0 && cy < BRICK_ROWS * BRICK_H) {
            gc = (uint8_t)(cx / BRICK_W);
            gr = (uint8_t)(cy / BRICK_H);
            if (g_bricks[gr][gc] != BRK_EMPTY) {
                hit_brick(gr, gc);
                g_lasers[i].active = 0;
                continue;
            }
        }

        /* Draw at new position */
        vid_fill_rect(g_lasers[i].x, g_lasers[i].y, LASER_W, LASER_H, COL_BRRED);
    }
}

/* ── Extra balls (Disruption) ── */

static void erase_extra_balls(void)
{
    uint8_t i;
    for (i = 0; i < MAX_BALLS - 1; i++) {
        if (s_extra_balls[i].active) {
            vid_fill_rect(s_extra_prev_x[i], s_extra_prev_y[i],
                          BALL_SIZE, BALL_SIZE, COL_BLACK);
            vid_fill_rect(s_extra_balls[i].x, s_extra_balls[i].y,
                          BALL_SIZE, BALL_SIZE, COL_BLACK);
            s_extra_balls[i].active = 0;
        }
    }
}

static void update_extra_balls(void)
{
    uint8_t i;
    int16_t nx, ny, cx, ty;
    uint8_t gr, gc;

    for (i = 0; i < MAX_BALLS - 1; i++) {
        if (!s_extra_balls[i].active) continue;

        nx = (int16_t)s_extra_balls[i].x + s_extra_balls[i].dx;
        ny = (int16_t)s_extra_balls[i].y + s_extra_balls[i].dy;

        /* Wall collisions */
        if (nx <= PF_LEFT + 2) {
            s_extra_balls[i].dx = -s_extra_balls[i].dx;
            nx = PF_LEFT + 3;
        }
        if (nx + BALL_SIZE >= PF_LEFT + PF_WIDTH - 2) {
            s_extra_balls[i].dx = -s_extra_balls[i].dx;
            nx = PF_LEFT + PF_WIDTH - 2 - BALL_SIZE - 1;
        }
        if (ny <= PF_TOP + 2) {
            s_extra_balls[i].dy = -s_extra_balls[i].dy;
            ny = PF_TOP + 3;
        }

        /* Death — extra ball just disappears */
        if (ny >= PF_BOTTOM) {
            vid_fill_rect(s_extra_balls[i].x, s_extra_balls[i].y,
                          BALL_SIZE, BALL_SIZE, COL_BLACK);
            vid_fill_rect(s_extra_prev_x[i], s_extra_prev_y[i],
                          BALL_SIZE, BALL_SIZE, COL_BLACK);
            s_extra_balls[i].active = 0;
            continue;
        }

        /* Paddle collision */
        if (s_extra_balls[i].dy > 0 &&
            ny + BALL_SIZE >= PADDLE_Y && ny < PADDLE_Y + PADDLE_H &&
            nx + BALL_SIZE >= (int16_t)g_paddle_x &&
            nx <= (int16_t)(g_paddle_x + g_paddle_w)) {
            s_extra_balls[i].dy = -s_extra_balls[i].dy;
            ny = PADDLE_Y - BALL_SIZE - 1;
        }

        /* Brick collision (simplified — centre point check) */
        cx = nx + BALL_SIZE / 2 - GRID_LEFT;
        ty = ny + BALL_SIZE / 2 - GRID_TOP;
        if (cx >= 0 && cx < BRICK_COLS * BRICK_W &&
            ty >= 0 && ty < BRICK_ROWS * BRICK_H) {
            gc = (uint8_t)(cx / BRICK_W);
            gr = (uint8_t)(ty / BRICK_H);
            if (g_bricks[gr][gc] != BRK_EMPTY) {
                hit_brick(gr, gc);
                s_extra_balls[i].dy = -s_extra_balls[i].dy;
                ny = (int16_t)s_extra_balls[i].y;
            }
        }

        /* Draw: move ball */
        vid_move_ball(s_extra_prev_x[i], s_extra_prev_y[i],
                      (uint16_t)nx, (uint16_t)ny, BALL_SIZE);

        s_extra_balls[i].x = (uint16_t)nx;
        s_extra_balls[i].y = (uint16_t)ny;
        s_extra_prev_x[i] = (uint16_t)nx;
        s_extra_prev_y[i] = (uint16_t)ny;
    }
}

/* ── Update ── */

void game_update(void)
{
    int16_t nx, ny, bx, by;
    int16_t cx, ty;
    uint8_t gr, gc;

    /* Update falling capsule */
    game_update_capsule();

    /* Update laser bolts */
    game_update_lasers();

    /* Update extra balls (disruption) */
    update_extra_balls();

    /* Power-up timer expiry */
    if (g_paddle_mode != PMODE_NORMAL && s_powerup_timer > 0) {
        s_powerup_timer--;
        if (s_powerup_timer == 0) {
            /* Erase old paddle, reset to normal */
            vid_fill_rect(g_paddle_x, PADDLE_Y, g_paddle_w, PADDLE_H, COL_BLACK);
            g_paddle_mode = PMODE_NORMAL;
            g_paddle_w = PADDLE_W;
            game_draw_paddle_force();
        }
    }

    /* Update enemies */
    enemy_update();

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
        /* Erase ball at current and last drawn position */
        vid_fill_rect(g_ball.x, g_ball.y, BALL_SIZE, BALL_SIZE, COL_BLACK);
        vid_fill_rect(s_prev_ball_x, s_prev_ball_y, BALL_SIZE, BALL_SIZE, COL_BLACK);
        g_ball.active = 0;

        /* If extra balls alive, promote one instead of losing a life */
        {
            uint8_t ei;
            for (ei = 0; ei < MAX_BALLS - 1; ei++) {
                if (s_extra_balls[ei].active) {
                    g_ball.active = 1;
                    g_ball.x = s_extra_balls[ei].x;
                    g_ball.y = s_extra_balls[ei].y;
                    g_ball.dx = s_extra_balls[ei].dx;
                    g_ball.dy = s_extra_balls[ei].dy;
                    s_prev_ball_x = s_extra_prev_x[ei];
                    s_prev_ball_y = s_extra_prev_y[ei];
                    s_extra_balls[ei].active = 0;
                    return;
                }
            }
        }

        g_lives--;
        g_dirty |= DIRTY_LIVES;
        /* Reset paddle to normal on death */
        if (g_paddle_mode == PMODE_ENLARGE) {
            vid_fill_rect(g_paddle_x, PADDLE_Y, g_paddle_w, PADDLE_H, COL_BLACK);
            g_paddle_w = PADDLE_W;
        }
        g_paddle_mode = PMODE_NORMAL;
        /* Erase any active capsule */
        if (g_capsule.type != CAP_NONE) {
            capsule_erase(s_prev_cap_x, s_prev_cap_y);
            g_capsule.type = CAP_NONE;
        }
        /* Clear enemies on death */
        enemy_reset();
        /* Clear extra balls */
        erase_extra_balls();
        /* Clear laser bolts */
        g_lasers[0].active = 0;
        g_lasers[1].active = 0;
        sfx_lose_life();
        if (g_lives == 0) {
            g_state = STATE_GAMEOVER;
        } else {
            /* Erase old paddle, reset to centre */
            vid_fill_rect(g_paddle_x, PADDLE_Y, g_paddle_w, PADDLE_H, COL_BLACK);
            g_paddle_x = (PF_LEFT + PF_WIDTH / 2) - (g_paddle_w / 2);
            s_prev_paddle_x = g_paddle_x;
            game_draw_paddle_force();
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
            /* Catch mode: ball sticks to paddle */
            if (g_paddle_mode == PMODE_CATCH) {
                g_ball.active = 0;
                g_state = STATE_LAUNCH;
            }
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
