#include "builtin.h"
#include <limits.h>
#include "utils.h"

static const int EXECUTED = (int) true, NOT_EXECUTED = (int) false;

static const char EXIT_CMD_STR[] = "exit";
static const int LEN_EXIT_CMD_STR = 5;

static const char CD_CMD_STR[] = "cd";
static const int LEN_CD_CMD_STR = 3;

static const char PWD_CMD_STR[] = "pwd";
static const int LEN_PWD_CMD_STR = 4;

// returns true if the 'exit' call
// should be performed
//
// (It must not be called from here)
int
exit_shell(char *cmd, int *status)
{
	if (strncmp(cmd, EXIT_CMD_STR, LEN_EXIT_CMD_STR) != 0) {
		return NOT_EXECUTED;
	}

	*status = EXIT_SUCCESS;
	return EXECUTED;
}

/*
 * Returns true if the `cmd` string starts with `CD_CMD_STR`. Returns false otherwise
 */
static bool
is_cd_command(char *cmd)
{
	int cmd_len = strlen(cmd);
	int max_iter = MIN(LEN_CD_CMD_STR - 1, cmd_len);
	int i = 0;
	for (i = 0; i < max_iter; i++) {
		if (cmd[i] != CD_CMD_STR[i]) {
			return false;
		}
	}

	if (i <= cmd_len) {
		return cmd[i] == END_STRING || cmd[i] == SPACE;
	}

	return false;
}

/*
 * Returns true if the string `path` has at least a character that is not a
 * space character. Returns false otherwise
 */
static bool
is_non_empty(char *path)
{
	int max_iter = MIN(PATH_MAX, strlen(path));

	for (int i = 0; i < max_iter; i++) {
		if (path[i] != SPACE) {
			return true;
		}
	}

	return false;
}

// returns true if "chdir" was performed
//  this means that if 'cmd' contains:
// 	1. $ cd directory (change to 'directory')
// 	2. $ cd (change to $HOME)
//  it has to be executed and then return true
//
//  Remember to update the 'prompt' with the
//  	new directory.
//
// Examples:
//  1. cmd = ['c','d', ' ', '/', 'b', 'i', 'n', '\0']
//  2. cmd = ['c','d', '\0']
int
cd(char *cmd, char *prompt, int *status)
{
	if (!is_cd_command(cmd)) {
		return NOT_EXECUTED;
	}

	char dest_path[PATH_MAX] = { END_STRING };
	char *temp_path = strchr(cmd, SPACE);

	if (temp_path != NULL && is_non_empty(temp_path)) {
		strncpy(dest_path, temp_path + 1, PATH_MAX - 1);

	} else {
		char *home_path = getenv(HOME_ENV_VAR_KEY);
		if (home_path == NULL) {
			perror("Error while retrieving home path");
			*status = EXIT_FAILURE;
			return EXECUTED;
		}
		strncpy(dest_path, home_path, PATH_MAX - 1);
	}

	char full_rest_path[PATH_MAX] = { END_STRING };
	char *res = realpath(dest_path, full_rest_path);
	if (res == NULL) {
		perror("Error while retrieving realpath");
		*status = EXIT_FAILURE;
		return EXECUTED;
	}

	int _res = chdir(full_rest_path);
	if (_res == GENERIC_ERROR_CODE) {
		perror("Error while changing directory");
		*status = EXIT_FAILURE;
		return EXECUTED;
	}

	update_prompt(prompt, full_rest_path);
	*status = EXIT_SUCCESS;

	return EXECUTED;
}

// returns true if 'pwd' was invoked
// in the command line
//
// (It has to be executed here and then
// 	return true)
int
pwd(char *cmd, int *status)
{
	if (strncmp(cmd, PWD_CMD_STR, LEN_PWD_CMD_STR) != 0) {
		return NOT_EXECUTED;
	}

	char cwd_path[PATH_MAX] = { END_STRING };

	char *res = getcwd(cwd_path, PATH_MAX);
	if (res == NULL) {
		perror("Error while executing pwd");
		*status = EXIT_FAILURE;
		return EXECUTED;
	}

	printf("%s\n", cwd_path);
	*status = EXIT_SUCCESS;

	return EXECUTED;
}

// returns true if `history` was invoked
// in the command line
//
// (It has to be executed here and then
// 	return true)
int
history(char *cmd, int *status)
{
	// Your code here
	MARK_UNUSED_ALWAYS(cmd);
	MARK_UNUSED_ALWAYS(status);
	return NOT_EXECUTED;
}
