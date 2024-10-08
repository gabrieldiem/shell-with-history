#include <unistd.h>

#include "defs.h"
#include "readline.h"

static char buffer[BUFLEN];
static char ansi_sequence_buff[ANSI_SEQUENCE_BUFF_SIZE] = { END_STRING };
static const char EVENT_DESIGNATOR_LAST_CMD_STR[] = "!!";
static const char EVENT_DESIGNATOR_LAST_CMD_LEN = 2;

static const char EVENT_DESIGNATOR_LAST_NTH_CMD_STR[] = "!-";
static const char EVENT_DESIGNATOR_LAST_NTH_CMD_LEN = 2;

static const int TAB_TO_SPACE_EQUIVALENCE = 4;

static void
echo_eval_symbol()
{
	fprintf(stdout, "%s", "$ ");
	fflush(stdout);
}

static void
echo_prompt(const char *prompt)
{
	fprintf(stdout, "%s %s %s\n", COLOR_RED, prompt, COLOR_RESET);
	echo_eval_symbol();
	fflush(stdout);
}

// reads a line from the standard input
// and prints the prompt
char *
read_line(const char *prompt)
{
	int i = 0, c = 0;

#ifndef SHELL_NO_INTERACTIVE
	if (isatty(1)) {
		echo_prompt(prompt);
	}
#else
	MARK_UNUSED(prompt);
#endif

	memset(buffer, 0, BUFLEN);

	c = getchar();

	while (c != END_LINE && c != EOF) {
		buffer[i++] = c;
		c = getchar();
	}

	// if the user press ctrl+D
	// just exit normally
	if (c == EOF)
		return NULL;

	buffer[i] = END_STRING;

	return buffer;
}

static void
echo_buffer_with_prompt()
{
	printf(ANSI_CODE_CLEAN_LINE /* + */
	               ANSI_CODE_MOVE_CURSOR_TO_LINE_START);
	echo_eval_symbol();
	printf("%s", buffer);
	fflush(stdout);
}

static void
handle_history_switch(char *buffer,
                      int *buffer_index,
                      char *ansi_sequence_buff,
                      char char_read,
                      bool *just_handled_arrow,
                      history_data_t *history)
{
	memset(ansi_sequence_buff, END_STRING, ANSI_SEQUENCE_BUFF_SIZE);
	ansi_sequence_buff[0] = char_read;
	if (read(STDIN_FILENO, &ansi_sequence_buff[1], 2) == 2) {
		memset(buffer, END_STRING, BUFLEN);
		*buffer_index = 0;

		if (ansi_sequence_buff[1] == BEGIN_ANSI_FUNCTION_CHARACTER) {
			switch (ansi_sequence_buff[2]) {
			case ANSI_FUNCTION_CURSOR_UP_CHARACTER:
				history_move_backwards(history,
				                       just_handled_arrow,
				                       buffer,
				                       buffer_index,
				                       &echo_buffer_with_prompt);
				*just_handled_arrow = true;
				break;

			case ANSI_FUNCTION_CURSOR_DOWN_CHARACTER:
				history_move_forwards(history,
				                      just_handled_arrow,
				                      buffer,
				                      buffer_index,
				                      &echo_buffer_with_prompt);
				break;
			}
		}
	}
}

static void
handle_end_line_read(int *buffer_index, bool *should_stop, history_data_t *history)
{
	if ((*buffer_index) + 1 < BUFLEN) {
		buffer[(*buffer_index) + 1] = END_STRING;
	}
	history_add_entry(history, buffer);
	fflush(stdout);
	*should_stop = true;
}

static void
delete_one_buffered_character(int *buffer_index)
{
	if (*buffer_index > 0) {
		(*buffer_index)--;
		buffer[*buffer_index] = END_STRING;
		printf(ANSI_CODE_MOVE_CURSOR_ONE_CHARACTER_TO_LEFT);  // Move 1 to left, for cleaning
		printf("%c",
		       SPACE);  // Overwrite with 1 space so it appears as cleared
		printf(ANSI_CODE_MOVE_CURSOR_ONE_CHARACTER_TO_LEFT);  // Move 1 to left again, for positioning
		fflush(stdout);
	}
}

static void
echo(char *char_read)
{
	write(STDOUT_FILENO, char_read, 1 * sizeof(char));
	fflush(stdout);
}

static void
load_buffer_and_echo(bool *just_handled_arrow,
                     char *buffer,
                     int *buffer_index,
                     char *char_read)
{
	if (!(*just_handled_arrow)) {
		if (*char_read == TAB) {
			char space_char = SPACE;
			for (int i = 0; i < TAB_TO_SPACE_EQUIVALENCE; i++) {
				buffer[*buffer_index] = space_char;
				(*buffer_index)++;
				echo(&space_char);
			}
		} else {
			buffer[*buffer_index] = *char_read;
			(*buffer_index)++;
			echo(char_read);
		}
	}
}

static void
echo_buffer_from_position(char *buffer, int buffer_index, int start_position)
{
	for (int i = start_position; i <= buffer_index; i++) {
		write(STDOUT_FILENO, &buffer[i], 1 * sizeof(char));
	}
	fflush(stdout);
}

static bool
check_and_replace_last_cmd_designator(char *buffer,
                                      int *buffer_index,
                                      history_data_t *history,
                                      char *char_read)
{
	bool was_replaced = false;
	char *token = strstr(buffer, EVENT_DESIGNATOR_LAST_CMD_STR);

	while (token != NULL && strlen(token) != 0) {
		for (int i = 0; i < EVENT_DESIGNATOR_LAST_CMD_LEN; i++) {
			delete_one_buffered_character(buffer_index);
		}

		int original_buff_index = *buffer_index;
		history_append_last_cmd(history, buffer, buffer_index, BUFLEN);
		echo_buffer_from_position(buffer,
		                          *buffer_index,
		                          original_buff_index);

		token += EVENT_DESIGNATOR_LAST_CMD_LEN * sizeof(char);
		token = strstr(token, EVENT_DESIGNATOR_LAST_CMD_STR);
		was_replaced = true;
	}

	if (was_replaced) {
		*char_read = SPACE;
	}
	return was_replaced;
}

static bool
check_and_replace_last_nth_cmd_designator(char *buffer,
                                          int *buffer_index,
                                          history_data_t *history,
                                          char *char_read)
{
	bool was_replaced = false;
	char *token = strstr(buffer, EVENT_DESIGNATOR_LAST_NTH_CMD_STR);
	char cmd_number_str[SMALL_BUFLEN];

	while (token != NULL && strlen(token) != 0) {
		token += EVENT_DESIGNATOR_LAST_CMD_LEN * sizeof(char);

		char *token_end = strchr(token, SPACE);
		if (token_end == NULL) {
			token_end = strchr(token, END_STRING);
		}

		if (token_end == NULL) {
			break;
		}

		int token_len_diff = strlen(token) - strlen(token_end);
		int i = 0;
		for (i = 0; i < MIN(token_len_diff, SMALL_BUFLEN); i++) {
			cmd_number_str[i] = token[i];
		}

		if (i < SMALL_BUFLEN) {
			cmd_number_str[i] = END_LINE;
		}

		int cmd_number = atoi(cmd_number_str);

		for (int i = 0;
		     i < (token_len_diff + EVENT_DESIGNATOR_LAST_NTH_CMD_LEN);
		     i++) {
			delete_one_buffered_character(buffer_index);
		}

		int original_buff_index = *buffer_index;
		history_append_last_nth_cmd(
		        history, cmd_number, buffer, buffer_index, BUFLEN);
		echo_buffer_from_position(buffer,
		                          *buffer_index,
		                          original_buff_index);

		token = strstr(token, EVENT_DESIGNATOR_LAST_CMD_STR);
		was_replaced = true;
	}

	if (was_replaced) {
		*char_read = SPACE;
	}
	return was_replaced;
}

static bool
check_for_event_designators_and_replace(char *buffer,
                                        int *buffer_index,
                                        history_data_t *history,
                                        char *char_read)
{
	bool was_replaced_last = false;
	bool was_replaced_last_nth = false;

	if (history_is_empty(history)) {
		return false;
	}

	was_replaced_last = check_and_replace_last_cmd_designator(
	        buffer, buffer_index, history, char_read);
	was_replaced_last_nth = check_and_replace_last_nth_cmd_designator(
	        buffer, buffer_index, history, char_read);

	return was_replaced_last || was_replaced_last_nth;
}

static bool
is_character_eof(char char_read)
{
	return (unsigned int) char_read == EOT_ASCII_CODE;
}

char *
read_line_non_canonical(const char *prompt,
                        bool *just_handled_arrow_action,
                        history_data_t *history)
{
	int i = 0;
	char char_read = 0;
	bool should_stop = false;
	bool was_replaced = false;

	echo_prompt(prompt);

	memset(buffer, END_STRING, BUFLEN);
	memset(ansi_sequence_buff, END_STRING, ANSI_SEQUENCE_BUFF_SIZE);

	read(STDIN_FILENO, &char_read, 1 * sizeof(char));

	while (!is_character_eof(char_read) && !should_stop) {
		switch (char_read) {
		case BEGIN_ANSI_SEQUENCE_CHARACTER:
			handle_history_switch(buffer,
			                      &i,
			                      ansi_sequence_buff,
			                      char_read,
			                      just_handled_arrow_action,
			                      history);
			read(STDIN_FILENO, &char_read, 1 * sizeof(char));
			break;
		case END_LINE:
			handle_end_line_read(&i, &should_stop, history);
			echo(&char_read);
			*just_handled_arrow_action = false;
			break;

		case BACKSPACE:
			delete_one_buffered_character(&i);
			*just_handled_arrow_action = false;
			read(STDIN_FILENO, &char_read, 1 * sizeof(char));
			break;

		case SPACE:
		case TAB:
			was_replaced = check_for_event_designators_and_replace(
			        buffer, &i, history, &char_read);
			*just_handled_arrow_action = false;

			if (was_replaced) {
				read(STDIN_FILENO, &char_read, 1 * sizeof(char));
			}
			/* fall through */

		default:
			if (was_replaced) {
				break;
			}
			load_buffer_and_echo(just_handled_arrow_action,
			                     buffer,
			                     &i,
			                     &char_read);
			*just_handled_arrow_action = false;
			read(STDIN_FILENO, &char_read, 1 * sizeof(char));
			break;
		}
		was_replaced = false;
	}

	// if the user press ctrl+D
	// just exit normally
	if (is_character_eof(char_read))
		return NULL;

	return buffer;
}
