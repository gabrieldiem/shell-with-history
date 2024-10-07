#include <unistd.h>

#include "defs.h"
#include "readline.h"

static char buffer[BUFLEN];
static char ansi_sequence_buff[ANSI_SEQUENCE_BUFF_SIZE] = { END_STRING };

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
handle_inline_character_deletion(int *buffer_index)
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
		buffer[*buffer_index] = *char_read;
		(*buffer_index)++;
		echo(char_read);
	}
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
			handle_inline_character_deletion(&i);
			*just_handled_arrow_action = false;
			read(STDIN_FILENO, &char_read, 1 * sizeof(char));
			break;

		default:
			load_buffer_and_echo(just_handled_arrow_action,
			                     buffer,
			                     &i,
			                     &char_read);
			*just_handled_arrow_action = false;
			read(STDIN_FILENO, &char_read, 1 * sizeof(char));
			break;
		}
	}

	// if the user press ctrl+D
	// just exit normally
	if (is_character_eof(char_read))
		return NULL;

	return buffer;
}
