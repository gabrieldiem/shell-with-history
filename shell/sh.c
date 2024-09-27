#include "defs.h"
#include "types.h"
#include "readline.h"
#include "runcmd.h"
#include "altstack.h"

static struct termios ORIGINAL_TERMINAL_SETTINGS;
char prompt[PRMTLEN] = { END_STRING };

/*
 * Custom handler for SIGCHLD signal. Waits only for child processes from same
 * process group, non-blocking.
 */
static void
sigchild_handler(int /*signum*/, siginfo_t *signal_info, void * /*ucontext_t*/)
{
	int status = 0;
	int child_pid = (int) signal_info->si_pid;
	int child_gpid = (int) getpgid(child_pid);
	bool is_background_task = child_pid != child_gpid;

	if (is_background_task) {
		int res = (int) waitpid(child_pid, &status, WNOHANG);
		if (res != GENERIC_ERROR_CODE) {
			print_status_info_from_pid(child_pid, status);
		}
	}
}

/*
 * Initializes the signal handler for SIGCHLD to custom handler.
 */
static void
initialize_sigaction_for_sigchild(stack_t *signal_alt_stack)
{
	struct sigaction signal_action;

	signal_action.sa_flags = SA_SIGINFO | SA_RESTART;
	signal_action.sa_sigaction = sigchild_handler;
	sigemptyset(&signal_action.sa_mask);

	int res = sigaction(SIGCHLD, &signal_action, NULL);
	if (res == GENERIC_ERROR_CODE) {
		perror("Error while setting up sigaction");
		free_alternative_stack(signal_alt_stack);
		exit(EXIT_FAILURE);
	}

	install_alternative_stack(signal_alt_stack);
}

static void
run_shell_in_canonical_mode(stack_t *signal_alt_stack)
{
	char *cmd;

	while ((cmd = read_line(prompt)) != NULL)
		if (run_cmd(cmd, prompt, signal_alt_stack) == EXIT_SHELL)
			return;
}

static void
run_shell_in_non_canonical_mode(stack_t *signal_alt_stack)
{
	char *cmd;
	int cmd_index = 0;
	bool just_handled_arrow = false;

	while ((cmd = read_line_non_canonical(prompt, &just_handled_arrow)) !=
	       NULL) {
		if (run_cmd(cmd, prompt, signal_alt_stack) == EXIT_SHELL)
			return;
	}
}

// runs a shell command
static void
run_shell(stack_t *signal_alt_stack, int run_mode)
{
	if (run_mode == SHELL_CANONICAL_MODE) {
		run_shell_in_canonical_mode(signal_alt_stack);
	} else /* if (run_mode == SHELL_NON_CANONICAL_MODE) */ {
		run_shell_in_non_canonical_mode(signal_alt_stack);
	}
}

static void
restore_original_terminal_settings()
{
	int res = tcsetattr(STDIN_FILENO, TCSAFLUSH, &ORIGINAL_TERMINAL_SETTINGS);
	if (res == GENERIC_ERROR_CODE) {
		perror("Error while restoring ORIGINAL_TERMINAL_SETTINGS");
	}
}

static int
enable_non_canonical_mode()
{
#ifndef SHELL_NO_INTERACTIVE
	if (isatty(1)) {
		int res = tcgetattr(STDIN_FILENO, &ORIGINAL_TERMINAL_SETTINGS);
		if (res == GENERIC_ERROR_CODE) {
			perror("Error while retrieving terminal settings");
			return FAILED;
		}

		res = atexit(restore_original_terminal_settings);
		if (res != SUCCESS) {
			perror("Error while setting cleanup function to "
			       "restore "
			       "terminal settings");
			return FAILED;
		}

		struct termios new_settings = ORIGINAL_TERMINAL_SETTINGS;
		new_settings.c_lflag &= ~(ECHO | ICANON);
		new_settings.c_cc[VMIN] = 1;
		new_settings.c_cc[VTIME] = 0;

		res = tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_settings);
		if (res == GENERIC_ERROR_CODE) {
			perror("Error while setting terminal non canonical "
			       "mode");
			return FAILED;
		}
	}
	return SUCCESS;
#else
	return FAILED;
#endif
}

// initializes the shell
// with the "HOME" directory
static void
init_shell(stack_t *signal_alt_stack, int *run_mode)
{
	char buf[BUFLEN] = { END_STRING };
	char *home = getenv(HOME_ENV_VAR_KEY);

	if (chdir(home) == GENERIC_ERROR_CODE) {
		snprintf(buf, sizeof buf, "cannot cd to %s ", home);
		perror(buf);
	} else {
		update_prompt(prompt, home);
	}

	setpgid(USE_PID_OF_THIS_PROCESS, SET_GPID_SAME_AS_PID_OF_THIS_PROCESS);
	initialize_sigaction_for_sigchild(signal_alt_stack);

	int non_canonical_mode_status = enable_non_canonical_mode();
	if (non_canonical_mode_status == SUCCESS) {
		*run_mode = SHELL_NON_CANONICAL_MODE;
	} else {
		*run_mode = SHELL_CANONICAL_MODE;
	}
}


int
main(void)
{
	stack_t signal_alt_stack;
	init_alternative_stack(&signal_alt_stack);

	int run_mode = SHELL_CANONICAL_MODE;
	init_shell(&signal_alt_stack, &run_mode);

	run_shell(&signal_alt_stack, run_mode);

	free_alternative_stack(&signal_alt_stack);
	return 0;
}
