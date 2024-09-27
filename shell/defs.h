#ifndef DEFS_H
#define DEFS_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <termios.h>

#ifndef SHELL_NO_COLORS
// color scape strings
#define COLOR_BLUE "\x1b[34m"
#define COLOR_RED "\x1b[31m"
#define COLOR_RESET "\x1b[0m"
#else
#define COLOR_BLUE ""
#define COLOR_RED ""
#define COLOR_RESET ""
#endif

#ifdef SHELL_NO_INTERACTIVE
#define MARK_UNUSED(parameter) (void) (parameter)
#else
#define MARK_UNUSED(parameter)
#endif

#define END_STRING '\0'
#define END_LINE '\n'
#define SPACE ' '
#define BEGIN_ANSI_SEQUENCE_CHARACTER '\x1b'
#define BACKSPACE 127

#define BUFLEN 1024
#define PRMTLEN 1024
#define MAXARGS 20
#define ARGSIZE 1024
#define FNAMESIZE 1024

#define HOME_ENV_VAR_KEY "HOME"
#define GENERIC_ERROR_CODE -1
#define SUCCESS 0
#define FAILED -1
#define MAGIC_VAR_BUFF_LEN 20
#define ANSI_SEQUENCE_BUFF_SIZE 4

#define BEGIN_ANSI_FUNCTION_CHARACTER '['
#define ANSI_FUNCTION_CURSOR_UP_CHARACTER 'A'
#define ANSI_FUNCTION_CURSOR_DOWN_CHARACTER 'B'
#define ANSI_CODE_MOVE_CURSOR_TO_LINE_START "\r"
#define ANSI_CODE_CLEAN_LINE "\33[2K"

#define USE_PID_OF_THIS_PROCESS 0
#define SET_GPID_SAME_AS_PID_OF_THIS_PROCESS 0
#define NO_OPTIONS 0

#define SHELL_CANONICAL_MODE 0
#define SHELL_NON_CANONICAL_MODE 1

// command representation after parsed
#define EXEC 1
#define BACK 2
#define REDIR 3
#define PIPE 4

// fd numbers for pipes
#define READ_SIDE 0
#define WRITE_SIDE 1
#define PIPE_SIZE_VECTOR 2

#define EXIT_SHELL 1

/* Macros for min/max.  */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#endif  // DEFS_H
