#include "defs.h"
#include "types.h"
#include "readline.h"
#include "runcmd.h"
#include "altstack.h"
#include "history.h"

static struct termios ORIGINAL_TERMINAL_SETTINGS;
char prompt[PRMTLEN] = { END_STRING };

/*
 * Converts an integer to its string representation
 * and writes it to a file descriptor.
 */
static inline void
write_number(int fd, int number)
{
	char number_buff[NUMBER_BUFF_LEN];
	int i = sizeof(number_buff) - 1;
	number_buff[i] = END_STRING;

	if (number == 0) {
		i--;
		number_buff[i] = ZERO_CHAR;
	} else {
		unsigned int abs_number = number < 0 ? (unsigned int) (-number)
		                                     : (unsigned int) (number);

		while (abs_number > 0) {
			i--;
			number_buff[i] = (abs_number % 10) + ZERO_CHAR;
			abs_number = abs_number / 10;
		}

		if (number < 0) {
			i--;
			number_buff[i] = MINUS_SIGN_CHAR;
		}
	}
	write(fd, &number_buff[i], sizeof(number_buff) - i - 1);
}

/*
 * Custom handler for SIGCHLD signal. Waits only for child processes from same
 * process group, non-blocking.
 */
static void
sigchild_handler(int /*signum*/, siginfo_t *signal_info, void * /*ucontext_t*/)
{
	int child_status = 0;
	int child_pid = 0;

	if (signal_info != NULL) {
		child_pid = (int) signal_info->si_pid;
	}

	int res = (int) waitpid(WAIT_FOR_A_PROCESS_THAT_SHARES_GPID_WITH_THIS_PROCESS,
	                        &child_status,
	                        WNOHANG);
	if (res != GENERIC_ERROR_CODE) {
#ifndef SHELL_NO_INTERACTIVE
		const char *action;

		if (WIFEXITED(child_status)) {
			action = "exited";
			child_status = WEXITSTATUS(child_status);
		} else if (WIFSIGNALED(child_status)) {
			action = "killed";
			child_status = -WTERMSIG(child_status);
		} else if (WTERMSIG(child_status)) {
			action = "stopped";
			child_status = -WSTOPSIG(child_status);
		} else {
			action = "exited";
		}

		char intro[] = "Program with PID [";
		char close_bracket[] = "] ";
		char status_with_comma[] = ", status: ";
		write(STDOUT_FILENO, COLOR_BLUE, strlen(COLOR_BLUE));
		write(STDOUT_FILENO, intro, strlen(intro));
		write_number(STDOUT_FILENO, child_pid);
		write(STDOUT_FILENO, close_bracket, strlen(close_bracket));
		write(STDOUT_FILENO, action, strlen(action));
		write(STDOUT_FILENO, status_with_comma, strlen(status_with_comma));
		write_number(STDOUT_FILENO, child_status);
		write(STDOUT_FILENO, COLOR_RESET, strlen(COLOR_RESET));
		write(STDOUT_FILENO, "\n ", 2);
#else
		MARK_UNUSED(child_pid);
#endif
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
run_shell_in_canonical_mode(stack_t *signal_alt_stack, history_data_t *history)
{
	char *cmd;

	while ((cmd = read_line(prompt)) != NULL)
		if (run_cmd(cmd, prompt, signal_alt_stack, history) == EXIT_SHELL)
			return;
}

static void
run_shell_in_non_canonical_mode(stack_t *signal_alt_stack, history_data_t *history)
{
	char *cmd;
	bool just_handled_arrow_action = false;

	while ((cmd = read_line_non_canonical(
	                prompt, &just_handled_arrow_action, history)) != NULL) {
		if (run_cmd(cmd, prompt, signal_alt_stack, history) == EXIT_SHELL)
			return;
	}
}

// runs a shell command
static void
run_shell(stack_t *signal_alt_stack, int run_mode, history_data_t *history)
{
	if (run_mode == SHELL_CANONICAL_MODE) {
		run_shell_in_canonical_mode(signal_alt_stack, history);
	} else /* if (run_mode == SHELL_NON_CANONICAL_MODE) */ {
		run_shell_in_non_canonical_mode(signal_alt_stack, history);
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

	history_data_t history;
	history_init(&history);

	int run_mode = SHELL_CANONICAL_MODE;
	init_shell(&signal_alt_stack, &run_mode);

	run_shell(&signal_alt_stack, run_mode, &history);

	free_alternative_stack(&signal_alt_stack);
	return 0;
}
