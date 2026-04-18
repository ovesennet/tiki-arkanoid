#ifndef LEVELS_H
#define LEVELS_H

#include <stdint.h>

#define NUM_LEVELS     32
#define LEVEL_ROWS     18
#define LEVEL_COLS     13

/* Each level is stored as: start_row (1 byte), num_rows (1 byte),
   then num_rows * LEVEL_COLS bytes of brick data.
   levels[i] points to the data for level i. */
extern const uint8_t * const levels[NUM_LEVELS];

#endif
