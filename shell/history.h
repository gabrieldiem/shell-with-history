#ifndef HISTORY_H
#define HISTORY_H

#include "defs.h"

typedef struct history_data {
	char **history_vector;
	size_t history_vector_size;
	int history_count;
	int history_index;
	char histfile_location[FNAMESIZE];
	FILE *history_file;
	bool initialized;
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

void history_print_last_n(history_data_t *history, unsigned int n, int *status);

void history_print_all(history_data_t *history, int *status);

bool history_is_empty(history_data_t *history);

void history_append_last_cmd(history_data_t *history,
                             char *cmd_buffer,
                             int *cmd_buffer_index,
                             int max_cmd_buff_len);

void history_append_last_nth_cmd(history_data_t *history,
                                 int n,
                                 char *cmd_buffer,
                                 int *cmd_buffer_index,
                                 int max_cmd_buff_len);

void history_destroy(history_data_t *history);

#endif  // HISTORY_H