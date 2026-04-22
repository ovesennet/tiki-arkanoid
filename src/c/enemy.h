/* TIKI ARKANOID — enemy.h
 * Enemy sprites that drift down through the playfield.
 */

#ifndef ENEMY_H_
#define ENEMY_H_

#include "defs.h"

extern Enemy g_enemies[MAX_ENEMIES];

void enemy_init(void);
void enemy_reset(void);
void enemy_update(void);
void enemy_draw(void);
void enemy_draw_all(void);
void enemy_erase_all(void);

#endif /* ENEMY_H_ */
