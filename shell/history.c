#include "history.h"

#include <string.h>

static const bool DEFAULT = true, NOT_DEFAULT = false;
static const char READ_AND_WRITE_APPENDING_FILE_PERMS[] = "a+";
static const size_t HISTORY_VECTOR_GROW_FACTOR = 2;

static void
open_histfile_from_existing_location(history_data_t *history)
{
	history->history_file = fopen(history->histfile_location,
	                              READ_AND_WRITE_APPENDING_FILE_PERMS);
	if (history->history_file == NULL) {
		perror("Error while opening history file");
		exit(EXIT_FAILURE);
	}

	// Make sure read position is at the beginning of file
	rewind(history->history_file);
}

static int
count_commands_in_file(history_data_t *history)
{
	int cmd_count = 0;
	char char_read = END_STRING;
	while ((char_read = fgetc(history->history_file)) != EOF) {
		if (char_read == END_LINE) {
			cmd_count++;
		}
	}
	rewind(history->history_file);

	return cmd_count;
}

static void
remove_break_line(char *line)
{
	char *ptr = strchr(line, END_LINE);
	if (ptr == NULL) {
		return;
	}

	*ptr = END_STRING;
}

static bool
is_non_empty(char *str)
{
	int max_iter = MIN(BUFLEN, strlen(str));

	for (int i = 0; i < max_iter; i++) {
		if (str[i] != SPACE && str[i] != END_LINE) {
			return true;
		}
	}

	return false;
}

static void
load_entire_history_from_file(history_data_t *history)
{
	int cmd_count = count_commands_in_file(history);

	// Get memory for vector
	char **temp_vector = calloc(cmd_count, sizeof(char *));
	if (temp_vector == NULL) {
		perror("Error while allocating memory");
		exit(EXIT_FAILURE);
	}

	history->history_vector = temp_vector;
	history->history_index = 0;

	// Load file content
	char *line_read = NULL;
	size_t len = 0;
	int i = 0;

	while (getline(&line_read, &len, history->history_file) !=
	               GENERIC_ERROR_CODE &&
	       i < cmd_count) {
		if (is_non_empty(line_read)) {
			remove_break_line(line_read);

			char *temp_string = calloc(BUFLEN, sizeof(char));
			if (temp_string == NULL) {
				perror("Error while allocating memory");
				exit(EXIT_FAILURE);
			}
			temp_vector[i] = temp_string;

			strncpy(history->history_vector[i], line_read, BUFLEN);
			i++;
		}
	}

	free(line_read);
	history->history_count = i;
	history->history_vector_size = cmd_count;
	history->history_index = history->history_count - 1;
}

static void
load_history_location(char *location,
                      int location_size,
                      char *env_var_key,
                      bool is_default)
{
	char resolved_path[BUFLEN];
	char temp_path[BUFLEN];

	char *env_var_value = getenv(env_var_key);
	if (env_var_value == NULL) {
		perror("Error while getting home path");
		exit(EXIT_FAILURE);
	}

	strncpy(temp_path, env_var_value, BUFLEN - 1);
	if (is_default) {
		strcat(temp_path, HISTFILE_DEFAULT_PATH);
	}

	char *res_ptr = realpath(temp_path, resolved_path);
	if (res_ptr == NULL && !is_default) {
		perror("Error on resolving path for histfile");
		exit(EXIT_FAILURE);
	}

	strncpy(location, resolved_path, location_size);
}

static void
update_histfile_if_needed(history_data_t *history)
{
	char env_histfile[FNAMESIZE] = { END_STRING };
	load_history_location(
	        env_histfile, FNAMESIZE, HISTFILE_ENV_VAR_NAME, NOT_DEFAULT);

	if (strncmp(history->histfile_location, env_histfile, FNAMESIZE) != 0) {
		strncpy(history->histfile_location, env_histfile, BUFLEN);
		history_destroy(history);
		open_histfile_from_existing_location(history);
		load_entire_history_from_file(history);
	}
}

void
history_init(history_data_t *history)
{
	history->history_vector_size = 0;
	history->history_count = 0;
	history->history_index = 1;

	history->history_vector = NULL;

	load_history_location(
	        history->histfile_location, FNAMESIZE, HOME_ENV_VAR_KEY, DEFAULT);
	open_histfile_from_existing_location(history);
	load_entire_history_from_file(history);

	int res = setenv(HISTFILE_ENV_VAR_NAME,
	                 history->histfile_location,
	                 OVERWRITE_TRUE);
	if (res == GENERIC_ERROR_CODE) {
		perror("Error while setting HISTFILE");
		exit(EXIT_FAILURE);
	}
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

static bool
is_cmd_equal_to_last_cmd(history_data_t *history, char *cmd)
{
	if (history->history_count == 0) {
		return false;
	}

	return strncmp(history->history_vector[history->history_count - 1],
	               cmd,
	               BUFLEN) == 0;
}

static void
grow_history_if_needed(history_data_t *history)
{
	if (history->history_count + 2 < (int) (history->history_vector_size)) {
		return;
	}

	size_t new_size = (history->history_vector_size == 0
	                           ? 1
	                           : history->history_vector_size) *
	                  HISTORY_VECTOR_GROW_FACTOR;
	char **temp = realloc(history->history_vector, new_size * sizeof(char *));
	if (temp == NULL) {
		perror("Error while allocating memory");
		exit(EXIT_FAILURE);
	}

	history->history_vector = temp;
	history->history_vector_size = new_size;
}

static void
write_entry_to_histfile(history_data_t *history, char *cmd)
{
	fprintf(history->history_file, "%s\n", cmd);
	fflush(history->history_file);
}

void
history_add_entry(history_data_t *history, char *new_cmd_buffer)
{
	update_histfile_if_needed(history);
	if (strlen(new_cmd_buffer) > 0 &&
	    !is_cmd_equal_to_last_cmd(history, new_cmd_buffer)) {
		grow_history_if_needed(history);

		char *temp = calloc(BUFLEN, sizeof(char));
		if (temp == NULL) {
			perror("Error while allocating memory");
			exit(EXIT_FAILURE);
		}

		history->history_vector[history->history_count] = temp;
		strncpy(history->history_vector[history->history_count],
		        new_cmd_buffer,
		        BUFLEN - 1);

		history->history_count++;
		history->history_index = history->history_count - 1;
		write_entry_to_histfile(history, new_cmd_buffer);
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
	update_histfile_if_needed(history);
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
	update_histfile_if_needed(history);
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

void
history_destroy(history_data_t *history)
{
	fclose(history->history_file);
	history->history_file = NULL;

	for (int i = 0; i < history->history_count; i++) {
		free(history->history_vector[i]);
	}

	free(history->history_vector);
	history->history_vector = NULL;
	history->history_count = 0;
	history->history_index = 0;
	history->history_vector_size = 0;
}
