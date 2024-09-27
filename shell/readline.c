#include <unistd.h>

#include "defs.h"
#include "readline.h"
#include "history.h"

static char buffer[BUFLEN];
static char ansi_sequence_buff[ANSI_SEQUENCE_BUFF_SIZE] = { END_STRING };

// reads a line from the standard input
// and prints the prompt
char *
read_line(const char *prompt)
{
	int i = 0, c = 0;

#ifndef SHELL_NO_INTERACTIVE
	if (isatty(1)) {
		fprintf(stdout, "%s %s %s\n", COLOR_RED, prompt, COLOR_RESET);
		fprintf(stdout, "%s", "$ ");
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
                      bool *should_stop,
                      const char *prompt)
{
	memset(ansi_sequence_buff, END_STRING, ANSI_SEQUENCE_BUFF_SIZE);
	ansi_sequence_buff[0] = char_read;
	if (read(STDIN_FILENO, &ansi_sequence_buff[1], 2) == 2) {
		memset(buffer, END_STRING, BUFLEN);
		memcpy(buffer,
		       ansi_sequence_buff,
		       (ANSI_SEQUENCE_BUFF_SIZE - 1) * sizeof(char));
		*buffer_index = 3;
		buffer[*buffer_index] = END_STRING;

		if (buffer[1] == BEGIN_ANSI_FUNCTION_CHARACTER) {
			switch (buffer[2]) {
			case ANSI_FUNCTION_CURSOR_UP_CHARACTER:
				if (history_index > 1) {
					history_index--;
				}
				strcpy(buffer, history_vector[history_index - 1]);
				*buffer_index =
				        strlen(history_vector[history_index - 1]);
				echo_buffer_with_prompt();
				break;
			case ANSI_FUNCTION_CURSOR_DOWN_CHARACTER:
				if (history_index < history_count - 1) {
					history_index++;
					strcpy(buffer,
					       history_vector[history_index]);
					*buffer_index = strlen(
					        history_vector[history_index]);
					echo_buffer_with_prompt();
				} else {
					buffer[0] = END_STRING;
					*buffer_index = 0;
				}
				break;
			}
		}
	}
}

static void
handle_end_line_read(bool *just_handled_arrow, int *buffer_index, bool *should_stop)
{
	if (!(*just_handled_arrow)) {
		if ((*buffer_index) + 1 < BUFLEN) {
			buffer[(*buffer_index) + 1] = END_STRING;
		}
		if (history_count < MAX_HISTORY) {
			strcpy(history_vector[history_count], buffer);
			history_count++;
			history_index = history_count;
		}
	}
	*should_stop = true;
}

static void
handle_inline_character_deletion(int *buffer_index)
{
	if (*buffer_index > 0) {
		(*buffer_index)--;
		printf("\b \b");
		fflush(stdout);
	}
}

static void
echo(char *char_read)
{
	write(STDOUT_FILENO, char_read, 1);
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


char *
read_line_non_canonical(const char *prompt, bool *just_handled_arrow)
{
	int i = 0;
	char char_read = 0;
	bool should_stop = false;

	echo_prompt(prompt);

	memset(buffer, END_STRING, BUFLEN);
	memset(ansi_sequence_buff, END_STRING, ANSI_SEQUENCE_BUFF_SIZE);

	read(STDIN_FILENO, &char_read, 1 * sizeof(char));

	while (char_read != EOF && !should_stop) {
		if (char_read == BEGIN_ANSI_SEQUENCE_CHARACTER) {
			handle_history_switch(buffer,
			                      &i,
			                      ansi_sequence_buff,
			                      char_read,
			                      &should_stop,
			                      prompt);
			*just_handled_arrow = true;
			read(STDIN_FILENO, &char_read, 1 * sizeof(char));
		} else if (char_read == END_LINE) {
			echo(&char_read);
			handle_end_line_read(just_handled_arrow, &i, &should_stop);
			*just_handled_arrow = false;
		} else if (char_read == BACKSPACE && false) {
			handle_inline_character_deletion(&i);
			*just_handled_arrow = false;
		} else {
			load_buffer_and_echo(
			        just_handled_arrow, buffer, &i, &char_read);
			*just_handled_arrow = false;
			read(STDIN_FILENO, &char_read, 1 * sizeof(char));
		}
	}


	// if the user press ctrl+D
	// just exit normally
	if (char_read == EOF)
		return NULL;

	return buffer;
}
