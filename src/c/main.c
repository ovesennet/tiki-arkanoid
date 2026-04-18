/* TIKI ARKANOID — main.c
 * Title screen, main game loop, state machine.
 *
 * Controls: A=left, D=right, SPACE=launch/fire, Q=quit
 */

#include <string.h>
#include "defs.h"
#include "video.h"
#include "font.h"
#include "sound.h"
#include "game.h"
#include "input.h"

/* ── Delay loop ── */
void delay(uint16_t n)
{
    volatile uint16_t i;
    for (i = 0; i < n; i++) ;
}

/* Centre text within the playfield area */
static void draw_pf_centred(uint16_t y, const char *str, uint8_t colour)
{
    uint8_t len = 0;
    const char *p = str;
    uint16_t w, x;
    while (*p++) len++;
    w = (uint16_t)len * (FONT_W + FONT_SPACING) - FONT_SPACING;
    x = PF_LEFT + (PF_WIDTH - w) / 2;
    draw_text(x, y, str, colour);
}

/* ── ARKANOID logo bitmap (8px wide, 9 rows per glyph) ── */
static const uint8_t logo_bmp[] = {
    /* A */ 0x7E,0xC3,0xC3,0xFF,0xC3,0xC3,0xC3,0xC3,0xC3,
    /* R */ 0xFE,0xC3,0xC3,0xFE,0xCC,0xC6,0xC3,0xC3,0xC3,
    /* K */ 0xC6,0xCC,0xD8,0xF0,0xD8,0xCC,0xC6,0xC3,0xC3,
    /* N */ 0xC3,0xE3,0xF3,0xDB,0xCF,0xC7,0xC3,0xC3,0xC3,
    /* O */ 0x7E,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0x7E,
    /* I */ 0xFF,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0xFF,
    /* D */ 0xFC,0xC6,0xC3,0xC3,0xC3,0xC3,0xC3,0xC6,0xFC
};
static const uint8_t logo_map[] = {0,1,2,0,3,4,5,6}; /* A R K A N O I D */
static const uint8_t logo_grad[] = {
    COL_BLUE, COL_BRBLUE, COL_BRCYAN, COL_WHITE, COL_WHITE,
    COL_BRCYAN, COL_BRBLUE, COL_LTGREY, COL_DKGREY
};

#define LG_ROWS 9
#define LG_SC   3
#define LG_N    8
#define LG_GAP  3
#define LG_CW   (8 * LG_SC)
#define LG_TW   (LG_N * LG_CW + (LG_N - 1) * LG_GAP)
#define LG_X    ((256 - LG_TW) / 2)
#define LG_Y    6

static void draw_logo(void)
{
    uint8_t li, row, bit, run;
    uint16_t x, y;
    const uint8_t *lp;

    for (li = 0; li < LG_N; li++) {
        lp = &logo_bmp[logo_map[li] * LG_ROWS];
        x = LG_X + li * (LG_CW + LG_GAP);
        for (row = 0; row < LG_ROWS; row++) {
            uint8_t d = lp[row];
            uint8_t col = logo_grad[row];
            y = LG_Y + (uint16_t)row * LG_SC;
            bit = 0;
            while (bit < 8) {
                if (d & (0x80 >> bit)) {
                    run = 1;
                    while (bit + run < 8 && (d & (0x80 >> (bit + run))))
                        run++;
                    vid_fill_rect(x + bit * LG_SC, y,
                                  run * LG_SC, LG_SC, col);
                    bit += run;
                } else {
                    bit++;
                }
            }
        }
    }
}

/* ── Spaceship (Vaus mothership) drawn with rectangles ── */
static void draw_ship(void)
{
    /* Antenna */
    vid_fill_rect(126, 42, 4, 6, COL_LTGREY);

    /* Bridge */
    vid_fill_rect(104, 48, 48, 10, COL_LTGREY);
    vid_fill_rect(108, 50, 40, 6, COL_DKGREY);
    vid_fill_rect(116, 51, 24, 4, COL_BRCYAN);

    /* Upper hull */
    vid_fill_rect(80, 58, 96, 8, COL_DKGREY);
    vid_fill_rect(84, 60, 88, 4, COL_LTGREY);

    /* Main hull */
    vid_fill_rect(56, 66, 144, 36, COL_DKGREY);

    /* Hull panels */
    vid_fill_rect(60,  70, 28, 10, COL_LTGREY);
    vid_fill_rect(62,  72, 24,  6, COL_DKGREY);
    vid_fill_rect(168, 70, 28, 10, COL_LTGREY);
    vid_fill_rect(170, 72, 24,  6, COL_DKGREY);
    vid_fill_rect(100, 68, 56, 16, COL_LTGREY);
    vid_fill_rect(104, 72, 48,  8, COL_DKGREY);

    /* Accent lights */
    vid_fill_rect(92,  70, 4, 4, COL_BRGREEN);
    vid_fill_rect(160, 70, 4, 4, COL_BRGREEN);

    /* Lower hull bands */
    vid_fill_rect(60, 86, 136, 4, COL_LTGREY);
    vid_fill_rect(60, 94, 136, 4, COL_LTGREY);
    vid_fill_rect(64,  88, 4, 4, COL_YELLOW);
    vid_fill_rect(188, 88, 4, 4, COL_YELLOW);
    vid_fill_rect(100, 96, 4, 2, COL_BRGREEN);
    vid_fill_rect(152, 96, 4, 2, COL_BRGREEN);

    /* Wing pods */
    vid_fill_rect(40,  78, 16, 20, COL_DKGREY);
    vid_fill_rect(42,  80, 12, 16, COL_LTGREY);
    vid_fill_rect(200, 78, 16, 20, COL_DKGREY);
    vid_fill_rect(202, 80, 12, 16, COL_LTGREY);
    vid_fill_rect(44,  84, 4, 4, COL_BRGREEN);
    vid_fill_rect(204, 84, 4, 4, COL_BRGREEN);

    /* Engine section */
    vid_fill_rect(56,  102, 144, 14, COL_DKGREY);
    vid_fill_rect(60,  104, 40,  10, COL_LTGREY);
    vid_fill_rect(156, 104, 40,  10, COL_LTGREY);
    vid_fill_rect(108, 104, 40,  10, COL_LTGREY);

    /* Engine pods */
    vid_fill_rect(60,  116, 30, 18, COL_DKGREY);
    vid_fill_rect(166, 116, 30, 18, COL_DKGREY);
    vid_fill_rect(112, 116, 32, 14, COL_DKGREY);

    /* Engine nozzles */
    vid_fill_rect(64,  134, 22, 6, COL_LTGREY);
    vid_fill_rect(170, 134, 22, 6, COL_LTGREY);
    vid_fill_rect(116, 130, 24, 6, COL_LTGREY);

    /* Engine glow */
    vid_fill_rect(68,  140, 14, 5, COL_YELLOW);
    vid_fill_rect(174, 140, 14, 5, COL_YELLOW);
    vid_fill_rect(120, 136, 16, 5, COL_YELLOW);
    vid_fill_rect(70,  145, 10, 4, COL_BRRED);
    vid_fill_rect(176, 145, 10, 4, COL_BRRED);
    vid_fill_rect(122, 141, 12, 4, COL_BRRED);
}

/* ── Title screen ── */
static void draw_title(void)
{
    vid_clear();
    draw_logo();
    draw_ship();

    draw_text_centred(165, "FOR TIKI-100", COL_LTGREY);
    draw_text_centred(185, "A/D MOVE  SPACE FIRE  Q QUIT", COL_BRCYAN);
    draw_text_centred(220, "PRESS ANY KEY TO START", COL_WHITE);
}

static void wait_key(void)
{
    input_wait_key();
}

static uint8_t g_autoplay = 0;

/* ── Main loop ── */
int main(void)
{
    uint8_t running;

    vid_init();
    sound_init();
    input_init();
    input_init();

    /* Title screen */
    draw_title();
    wait_key();

    /* Init game */
    vid_clear();
    game_init();
    game_draw_playfield();
    game_draw_paddle_force();
    game_draw_ball();
    game_draw_sidebar_labels();
    game_draw_sidebar_values();
    sfx_stage_intro();

    input_flush();

    running = 1;
    while (running) {
        /* Poll keyboard matrix directly */
        input_poll();

        if (input_pressed(KBIT_QUIT)) {
            running = 0;
            break;
        }

        /* Toggle autoplay with '0' */
        if (input_pressed(KBIT_AUTO)) {
            g_autoplay ^= 1;
            delay(200);
            input_flush();
        }

        /* Next level with '+' */
        if (input_pressed(KBIT_NEXT)) {
            g_round++;
            g_dirty |= DIRTY_ROUND;
            game_load_stage(g_round);
            g_state = STATE_LAUNCH;
            g_paddle_x = (PF_LEFT + PF_WIDTH / 2) - (g_paddle_w / 2);
            game_reset_ball();
            vid_clear();
            game_draw_playfield();
            game_draw_sidebar_labels();
            game_draw_paddle_force();
            g_dirty = DIRTY_ALL;
            delay(200);
            input_flush();
        }

        if (g_autoplay) {
            /* AI: move paddle centre toward ball x */
            uint16_t pad_cx = g_paddle_x + (g_paddle_w >> 1);
            uint16_t ball_cx = g_ball.x + (BALL_SIZE >> 1);
            if (ball_cx + 2 < pad_cx) {
                if (g_paddle_x > PADDLE_MIN_X + PADDLE_SPEED)
                    g_paddle_x -= PADDLE_SPEED;
                else
                    g_paddle_x = PADDLE_MIN_X;
            } else if (ball_cx > pad_cx + 2) {
                if (g_paddle_x + g_paddle_w + PADDLE_SPEED < PADDLE_MAX_X)
                    g_paddle_x += PADDLE_SPEED;
                else
                    g_paddle_x = PADDLE_MAX_X - g_paddle_w;
            }
            /* Auto-launch */
            if (g_state == STATE_LAUNCH) {
                game_launch_ball();
                g_state = STATE_PLAY;
            }
        } else {
            /* Held keys - continuous paddle movement */
            if (input_held(KBIT_LEFT)) {
                if (g_paddle_x > PADDLE_MIN_X + PADDLE_SPEED)
                    g_paddle_x -= PADDLE_SPEED;
                else
                    g_paddle_x = PADDLE_MIN_X;
            }
            if (input_held(KBIT_RIGHT)) {
                if (g_paddle_x + g_paddle_w + PADDLE_SPEED < PADDLE_MAX_X)
                    g_paddle_x += PADDLE_SPEED;
                else
                    g_paddle_x = PADDLE_MAX_X - g_paddle_w;
            }

            /* Edge-triggered keys */
            if (input_pressed(KBIT_FIRE) && g_state == STATE_LAUNCH) {
                game_launch_ball();
                g_state = STATE_PLAY;
            }
        }
        if (input_pressed(KBIT_PAUSE)) {
            draw_text(SIDE_X, SIDE_Y + 78, "PAUSE", COL_WHITE);
            /* Wait for P to be released */
            do { input_poll(); } while (input_held(KBIT_PAUSE));
            /* Wait for any key press */
            wait_key();
            vid_fill_rect(SIDE_X, SIDE_Y + 78, 50, FONT_CH, COL_BLACK);
            /* Wait for that key to be released */
            do { input_poll(); } while (input_any());
            input_flush();
        }

        /* Update logic */
        game_update();

        /* Stage cleared? */
        if (g_bricks_left == 0 && g_state == STATE_PLAY) {
            g_round++;
            g_dirty |= DIRTY_ROUND;
            game_load_stage(g_round);
            g_state = STATE_LAUNCH;
            game_reset_ball();
            vid_clear();
            game_draw_playfield();
            game_draw_sidebar_labels();
            g_dirty = DIRTY_ALL;
            sfx_stage_intro();
        }

        /* Game over? */
        if (g_state == STATE_GAMEOVER) {
            draw_pf_centred(120, "GAME OVER", COL_BRRED);
            if (!g_autoplay) {
                draw_pf_centred(140, "PRESS ANY KEY", COL_WHITE);
                wait_key();
            } else {
                delay(500);
            }
            draw_title();
            wait_key();
            vid_clear();
            game_init();
            game_draw_playfield();
            game_draw_sidebar_labels();
            sfx_stage_intro();
        }

        /* Draw only what changed */
        game_draw_paddle();
        game_move_ball();
        game_draw_sidebar_values();

        /* Frame pacing */
        delay(80);
    }

    sound_off();
    input_shutdown();
    vid_shutdown();
    return 0;
}
