#include "exec.h"

#include "parsing.h"
#include "printstatus.h"
#include "altstack.h"

static const char STR_COMBINE_STREAM_2_INTO_STREAM_1[] = "&1";
static const int LEN_STR_COMBINE_STREAM_2_INTO_STREAM_1 = 3;
static const char KEY_VALUE_SEPARATOR = '=';
static const int OVERWRITE_TRUE = 1;

// sets "key" with the key part of "arg"
// and null-terminates it
//
// Example:
//  - KEY=value
//  arg = ['K', 'E', 'Y', '=', 'v', 'a', 'l', 'u', 'e', '\0']
//  key = "KEY"
//

static void
get_environ_key(char *arg, char *key)
{
	int i;
	for (i = 0; arg[i] != '='; i++)
		key[i] = arg[i];

	key[i] = END_STRING;
}

// sets "value" with the value part of "arg"
// and null-terminates it
// "idx" should be the index in "arg" where "=" char
// resides
//
// Example:
//  - KEY=value
//  arg = ['K', 'E', 'Y', '=', 'v', 'a', 'l', 'u', 'e', '\0']
//  value = "value"
//
static void
get_environ_value(char *arg, char *value, int idx)
{
	size_t i, j;
	for (i = (idx + 1), j = 0; i < strlen(arg); i++, j++)
		value[j] = arg[i];

	value[j] = END_STRING;
}

// sets the environment variables received
// in the command line
//
// Hints:
// - use 'block_contains()' to
// 	get the index where the '=' is
// - 'get_environ_*()' can be useful here
static void
set_environ_vars(char **eargv, int eargc)
{
	for (int i = 0; i < eargc; i++) {
		int key_value_separator_index =
		        block_contains(eargv[i], KEY_VALUE_SEPARATOR);
		if (key_value_separator_index == GENERIC_ERROR_CODE) {
			continue;
		}

		int environ_var_key_len = strlen(eargv[i]);
		char *environ_var_key = calloc(environ_var_key_len, sizeof(char));
		if (environ_var_key == NULL) {
			perror("Error while allocating memory");
			continue;
		}

		char *environ_var_value =
		        calloc(environ_var_key_len, sizeof(char));
		if (environ_var_value == NULL) {
			perror("Error while allocating memory");
			free(environ_var_key);
			continue;
		}

		environ_var_key[0] = END_STRING;
		environ_var_value[0] = END_STRING;

		get_environ_key(eargv[i], environ_var_key);
		get_environ_value(eargv[i],
		                  environ_var_value,
		                  key_value_separator_index);

		int res =
		        setenv(environ_var_key, environ_var_value, OVERWRITE_TRUE);
		if (res == GENERIC_ERROR_CODE) {
			perror("Error while setting environment variable");
		}
		free(environ_var_key);
		free(environ_var_value);
	}
}

// opens the file in which the stdin/stdout/stderr
// flow will be redirected, and returns
// the file descriptor
//
// Find out what permissions it needs.
// Does it have to be closed after the execve(2) call?
//
// Hints:
// - if O_CREAT is used, add S_IWUSR and S_IRUSR
// 	to make it a readable normal file
static int
open_redir_fd(char *file, int flags, int perms)
{
	int fd = open(file, flags, perms);

	if (fd == GENERIC_ERROR_CODE) {
		perror("Error: cannot open redirection file");
		_exit(EXIT_FAILURE);
	}

	return fd;
}

static void
run_exec(struct execcmd *exec_cmd, stack_t *signal_alt_stack)
{
	char *execvp_buff[MAXARGS + 1] = { NULL };

	for (int i = 0; i < exec_cmd->argc; i++) {
		execvp_buff[i] = exec_cmd->argv[i];
	}

	set_environ_vars(exec_cmd->eargv, exec_cmd->eargc);
	execvp(execvp_buff[0], execvp_buff);
	free_command((struct cmd *) exec_cmd);
	free_alternative_stack(signal_alt_stack);
	perror("Error on exec");
	_exit(EXIT_FAILURE);
}

/*
 * Returns `true` if the output file redirection is present,
 * returns `false` otherwise.
 */
static bool
should_redirect_output_to_file(struct execcmd *exec_cmd)
{
	return strlen(exec_cmd->out_file) > 0;
}

/*
 * Returns `true` if the input file redirection is present,
 * returns `false` otherwise.
 */
static bool
should_redirect_input_from_file(struct execcmd *exec_cmd)
{
	return strlen(exec_cmd->in_file) > 0;
}

/*
 * Returns `true` if the stream 2 into a file redirection is present,
 * returns `false` otherwise
 */
static bool
should_redirect_stream_2_into_file(struct execcmd *exec_cmd)
{
	return strlen(exec_cmd->err_file) > 0;
}

/*
 * Returns `true` if the stream 2 into stream 1 redirection is present,
 * returns `false` otherwise
 */
static bool
should_combine_stream_2_into_stream_1(struct execcmd *exec_cmd)
{
	return strncmp(exec_cmd->err_file,
	               STR_COMBINE_STREAM_2_INTO_STREAM_1,
	               LEN_STR_COMBINE_STREAM_2_INTO_STREAM_1) == 0;
}

/*
 * Delete the file at `filepath`.
 * If deletion fails, an error is printed.
 */
static void
unlink_file(char *filepath)
{
	int res = unlink(filepath);
	if (res == GENERIC_ERROR_CODE) {
		perror("Error: could not remove the file from the filesystem");
	}
}

/*
 * Opens the file (or creates a new one if it doesn't exist) and writes
 * the STDOUT output on it.
 */
static void
redirect_output_to_file(struct execcmd *exec_cmd)
{
	int fd = open_redir_fd(exec_cmd->out_file,
	                       O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
	                       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	int res = dup2(fd, STDOUT_FILENO);
	if (res == GENERIC_ERROR_CODE) {
		perror("Error: cannot redirect STDOUT output into file");
		res = close(fd);
		if (res == GENERIC_ERROR_CODE) {
			perror("Error while closing file");
		}
		unlink_file(exec_cmd->out_file);
		_exit(EXIT_FAILURE);
	}
}

/*
 * Opens the file and reads the content of it to be used as the STDIN input.
 */
static void
redirect_input_from_file(struct execcmd *exec_cmd)
{
	int fd = open_redir_fd(exec_cmd->in_file,
	                       O_RDONLY | O_CLOEXEC,
	                       S_IRUSR | S_IRGRP | S_IROTH);

	int res = dup2(fd, STDIN_FILENO);
	if (res == GENERIC_ERROR_CODE) {
		perror("Error: cannot redirect STDIN input into file");
		res = close(fd);
		if (res == GENERIC_ERROR_CODE) {
			perror("Error while closing file");
		}
		unlink_file(exec_cmd->out_file);
		_exit(EXIT_FAILURE);
	}
}

/*
 * Opens the file (or creates a new one if it doesn't exist) and writes
 * the STDERR output on it
 */
static void
redirect_stream_2_into_file(struct execcmd *exec_cmd)
{
	int fd = open_redir_fd(exec_cmd->err_file,
	                       O_WRONLY | O_CREAT | O_CLOEXEC,
	                       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	int res = dup2(fd, STDERR_FILENO);
	if (res == GENERIC_ERROR_CODE) {
		perror("Error: cannot redirect STDERR output into file");
		res = close(fd);
		if (res == GENERIC_ERROR_CODE) {
			perror("Error while closing file");
		}
		unlink_file(exec_cmd->out_file);
		_exit(EXIT_FAILURE);
	}
}

/*
 * Close the current stderr FD and create a new stderr FD that points to
 * the stdout file
 */
static void
combine_stream_2_into_stream_1()
{
	int res = dup2(STDOUT_FILENO, STDERR_FILENO);
	if (res == GENERIC_ERROR_CODE) {
		perror("Error: cannot combine STDERR output into STDOUT");
		_exit(EXIT_FAILURE);
	}
}

/*
 * Executes the redirections of the command.
 */
static void
exec_redirections(struct execcmd *redir_cmd)
{
	if (should_redirect_output_to_file(redir_cmd)) {
		redirect_output_to_file(redir_cmd);
	}

	if (should_redirect_input_from_file(redir_cmd)) {
		redirect_input_from_file(redir_cmd);
	}

	if (should_redirect_stream_2_into_file(redir_cmd)) {
		redirect_stream_2_into_file(redir_cmd);
	}

	if (should_combine_stream_2_into_stream_1(redir_cmd)) {
		combine_stream_2_into_stream_1();
	}
}

/* fd_unused and fd_used are both ends of the same pipe.
 */
static void
redirect_stream_to_pipe(int fd_unused, int fd_used, int stream)
{
	close(fd_unused);
	int res = dup2(fd_used, stream);
	close(fd_used);

	if (res == GENERIC_ERROR_CODE) {
		perror("Error: cannot redirect stream for pipe");
		_exit(EXIT_FAILURE);
	}
}


static int
handle_pipe_cmd_flow(struct pipecmd *pipe_cmd, stack_t *signal_alt_stack)
{
	int fds[PIPE_SIZE_VECTOR];
	int res = pipe(fds);
	if (res == GENERIC_ERROR_CODE) {
		perror("Error while creating pipe");
		_exit(EXIT_FAILURE);
	}

	pid_t pid_left = fork();

	if (pid_left == 0) {
		setpgid(USE_PID_OF_THIS_PROCESS,
		        SET_GPID_SAME_AS_PID_OF_THIS_PROCESS);
		redirect_stream_to_pipe(fds[READ_SIDE],
		                        fds[WRITE_SIDE],
		                        STDOUT_FILENO);
		exec_cmd(pipe_cmd->leftcmd, signal_alt_stack);
	}

	struct cmd *parsed_right = parse_line(pipe_cmd->rightcmd->scmd, &status);
	free_command(pipe_cmd->rightcmd);
	pipe_cmd->rightcmd = parsed_right;

	pid_t pid_right = fork();

	if (pid_right == 0) {
		setpgid(USE_PID_OF_THIS_PROCESS,
		        SET_GPID_SAME_AS_PID_OF_THIS_PROCESS);
		redirect_stream_to_pipe(fds[WRITE_SIDE],
		                        fds[READ_SIDE],
		                        STDIN_FILENO);
		exec_cmd(pipe_cmd->rightcmd, signal_alt_stack);
	}

	close(fds[READ_SIDE]);
	close(fds[WRITE_SIDE]);

	int exit_code = EXIT_SUCCESS;

	if (waitpid(pid_left, &status, NO_OPTIONS) != GENERIC_ERROR_CODE) {
		print_status_info(pipe_cmd->leftcmd);
	}

	if (waitpid(pid_right, &status, NO_OPTIONS) != GENERIC_ERROR_CODE) {
		exit_code = status;
		if (pipe_cmd->rightcmd->type != PIPE) {
			print_status_info(pipe_cmd->rightcmd);
		}
	}
	return exit_code;
}

// executes a command - does not return
//
// Hint:
// - check how the 'cmd' structs are defined
// 	in types.h
// - casting could be a good option
void
exec_cmd(struct cmd *cmd, stack_t *signal_alt_stack)
{
	// To be used in the different cases
	struct execcmd *e_cmd;
	struct backcmd *back_cmd;
	struct execcmd *redir_cmd;
	struct pipecmd *pipe_cmd;

	switch (cmd->type) {
	case EXEC:
		// spawns a command
		e_cmd = (struct execcmd *) cmd;
		run_exec(e_cmd, signal_alt_stack);
		break;

	case BACK: {
		// runs a command in background
		back_cmd = (struct backcmd *) cmd;
		struct cmd *bg_cmd = back_cmd->c;
		back_cmd->c = NULL;
		free_command((struct cmd *) back_cmd);
		exec_cmd(bg_cmd, signal_alt_stack);
		break;
	}

	case REDIR: {
		// changes the input/output/stderr flow
		redir_cmd = (struct execcmd *) cmd;
		exec_redirections(redir_cmd);
		run_exec(redir_cmd, signal_alt_stack);
		break;
	}

	case PIPE: {
		// pipes two commands
		pipe_cmd = (struct pipecmd *) cmd;
		int res = handle_pipe_cmd_flow(pipe_cmd, signal_alt_stack);
		// free the memory allocated
		// for the pipe tree structure
		free_command(parsed_pipe);
		free_alternative_stack(signal_alt_stack);
		_exit(parse_exit_code(res));
		break;
	}
	}
}
