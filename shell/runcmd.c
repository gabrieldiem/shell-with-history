#include "runcmd.h"

int status = 0;
struct cmd *parsed_pipe;

// runs the command in 'cmd'
int
run_cmd(char *cmd, char *prompt, stack_t *signal_alt_stack)
{
	pid_t _pid;
	struct cmd *parsed;
	// printf("[%s]\n", cmd);

	// if the "enter" key is pressed
	// just print the prompt again
	if (cmd[0] == END_STRING)
		return 0;

	// "history" built-in call
	if (history(cmd, &status))
		return 0;

	// "cd" built-in call
	if (cd(cmd, prompt, &status))
		return 0;

	// "exit" built-in call
	if (exit_shell(cmd, &status))
		return EXIT_SHELL;

	// "pwd" built-in call
	if (pwd(cmd, &status))
		return 0;

	// parses the command line
	parsed = parse_line(cmd, &status);

	// forks and run the command
	if ((_pid = fork()) == 0) {
		// keep a reference
		// to the parsed pipe cmd
		// so it can be freed later
		if (parsed->type == PIPE)
			parsed_pipe = parsed;


		if (parsed->type != BACK) {
			setpgid(USE_PID_OF_THIS_PROCESS,
			        SET_GPID_SAME_AS_PID_OF_THIS_PROCESS);
		}

		exec_cmd(parsed, signal_alt_stack);
	}

	// stores the pid of the process
	parsed->pid = _pid;

	// background process special treatment
	if (parsed->type == BACK) {
		print_back_info(parsed);
		free_command(parsed);
		return 0;
	}

	// waits for the process to finish
	waitpid(_pid, &status, NO_OPTIONS);
	int exit_code = parse_exit_code(status);

	print_status_info(parsed);

	free_command(parsed);

	status = exit_code;
	return 0;
}
