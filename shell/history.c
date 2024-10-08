#include "history.h"

#include <string.h>

void
history_init(history_data_t *history)
{
	history->history_count = 0;
	history->history_index = 1;
}

static void
history_load_current(history_data_t *history,
                     char *current_cmd_buffer,
                     int *current_cmd_buffer_index)
{
	strcpy(current_cmd_buffer,
	       history->history_vector[history->history_index]);
	*current_cmd_buffer_index =
	        strlen(history->history_vector[history->history_index]);
}

void
history_move_backwards(history_data_t *history,
                       bool *should_start_moving_index,
                       char *current_cmd_buffer,
                       int *current_cmd_buffer_index,
                       void (*action_when_refreshed)())
{
	if (history->history_index > 0 && *should_start_moving_index) {
		(history->history_index)--;
	}

	if (history->history_index >= 0) {
		history_load_current(history,
		                     current_cmd_buffer,
		                     current_cmd_buffer_index);
		action_when_refreshed();
	}
}

void
history_move_forwards(history_data_t *history,
                      bool *should_start_moving_index,
                      char *current_cmd_buffer,
                      int *current_cmd_buffer_index,
                      void (*action_when_refreshed)())
{
	if (history->history_index < history->history_count - 1) {
		(history->history_index)++;
	} else if (history->history_index == history->history_count - 1) {
		current_cmd_buffer[0] = END_STRING;
		*current_cmd_buffer_index = 0;
		action_when_refreshed();
		*should_start_moving_index = false;
		return;
	}

	if (history->history_index >= 0 && *should_start_moving_index) {
		history_load_current(history,
		                     current_cmd_buffer,
		                     current_cmd_buffer_index);
		action_when_refreshed();
		*should_start_moving_index = true;
	}
}

void
history_add_entry(history_data_t *history, char *new_cmd_buffer)
{
	if (history->history_count < MAX_HISTORY && strlen(new_cmd_buffer) > 0) {
		strcpy(history->history_vector[history->history_count],
		       new_cmd_buffer);
		history->history_count++;
		history->history_index = history->history_count - 1;
	}
}

static void
history_print_from_index_i(history_data_t *history, int index_i)
{
	char cmd_num[SMALL_BUFLEN] = { END_LINE };

	for (int i = index_i; i <= history->history_index; i++) {
		snprintf(cmd_num, SMALL_BUFLEN, "[%d]", i);
		printf("%6s %s\n", cmd_num, history->history_vector[i]);
		cmd_num[0] = END_LINE;
	}
}

void
history_print_last_n(history_data_t *history, unsigned int n, int *status)
{
	int normalized_n = n;
	if (normalized_n > history->history_count) {
		normalized_n = history->history_count;
	}

	int index_start = history->history_count - normalized_n;
	history_print_from_index_i(history, index_start);

	*status = EXIT_SUCCESS;
}

void
history_print_all(history_data_t *history, int *status)
{
	history_print_from_index_i(history, 0);
	*status = EXIT_SUCCESS;
}

bool
history_is_empty(history_data_t *history)
{
	return history->history_count == 0;
}

void
history_append_last_cmd(history_data_t *history,
                        char *cmd_buffer,
                        int *cmd_buffer_index,
                        int max_cmd_buff_len)
{
	if (history->history_count <= 0) {
		return;
	}
	int size = max_cmd_buff_len - (*cmd_buffer_index) + 1;
	int printed_amount =
	        snprintf(cmd_buffer + *cmd_buffer_index,
	                 size,
	                 "%s",
	                 history->history_vector[history->history_count - 1]);

	(*cmd_buffer_index) += printed_amount;
}

void
history_append_last_nth_cmd(history_data_t *history,
                            int n,
                            char *cmd_buffer,
                            int *cmd_buffer_index,
                            int max_cmd_buff_len)
{
	if (history->history_count <= 0) {
		return;
	}

	int absolute_index = history->history_count - n;

	if (absolute_index < 0) {
		absolute_index = 0;
	}

	if (absolute_index > history->history_index) {
		absolute_index = history->history_index;
	}

	int size = max_cmd_buff_len - (*cmd_buffer_index) + 1;
	int printed_amount = snprintf(cmd_buffer + *cmd_buffer_index,
	                              size,
	                              "%s",
	                              history->history_vector[absolute_index]);

	(*cmd_buffer_index) += printed_amount;
}
