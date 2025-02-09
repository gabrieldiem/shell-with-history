#include "utils.h"
#include <stdarg.h>

// splits a string line in two
// according to the splitter character
char *
split_line(char *buf, char splitter)
{
	int i = 0;

	while (buf[i] != splitter && buf[i] != END_STRING)
		i++;

	buf[i++] = END_STRING;

	while (buf[i] == SPACE)
		i++;

	return &buf[i];
}

// looks in a block for the 'c' character
// and returns the index in which it is, or -1
// in other case
int
block_contains(char *buf, char c)
{
	for (size_t i = 0; i < strlen(buf); i++)
		if (buf[i] == c)
			return i;

	return GENERIC_ERROR_CODE;
}

// Printf wrappers for debug purposes so that they don't
// show when shell is compiled in non-interactive way
int
printf_debug(char *format, ...)
{
#ifndef SHELL_NO_INTERACTIVE
	va_list args;
	va_start(args, format);
	int ret = vprintf(format, args);
	va_end(args);

	return ret;
#else
	MARK_UNUSED(format);
	return 0;
#endif
}

int
fprintf_debug(FILE *file, char *format, ...)
{
#ifndef SHELL_NO_INTERACTIVE
	va_list args;
	va_start(args, format);
	int ret = vfprintf(file, format, args);
	va_end(args);

	return ret;
#else
	MARK_UNUSED(file);
	MARK_UNUSED(format);
	return 0;
#endif
}

int
parse_exit_code(int raw_exit_code)
{
	int parsed = raw_exit_code;
	if (WIFEXITED(raw_exit_code)) {
		parsed = WEXITSTATUS(raw_exit_code);
	} else if (WIFSIGNALED(raw_exit_code)) {
		parsed = -WTERMSIG(raw_exit_code);
	} else if (WTERMSIG(raw_exit_code)) {
		parsed = -WSTOPSIG(raw_exit_code);
	}
	return parsed;
}

void
update_prompt(char prompt[PRMTLEN], char *new_prompt_text)
{
	snprintf(prompt, PRMTLEN - 1, "(%s)", new_prompt_text);
}
