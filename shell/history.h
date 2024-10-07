#ifndef HISTORY_H
#define HISTORY_H

#include "defs.h"

// typedef struct history_data history_data_t;
#define MAX_HISTORY 400

typedef struct history_data {
	char history_vector[MAX_HISTORY][BUFLEN];
	int history_count;
	int history_index;
} history_data_t;

void history_init(history_data_t *history);

void history_move_backwards(history_data_t *history,
                            bool *should_start_moving_index,
                            char *current_cmd_buffer,
                            int *current_cmd_buffer_index,
                            void (*action_when_refreshed)());

void history_move_forwards(history_data_t *history,
                           bool *should_start_moving_index,
                           char *current_cmd_buffer,
                           int *current_cmd_buffer_index,
                           void (*action_when_refreshed)());

void history_add_entry(history_data_t *history, char *new_cmd_buffer);


#endif  // HISTORY_H