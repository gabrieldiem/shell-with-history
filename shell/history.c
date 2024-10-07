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
