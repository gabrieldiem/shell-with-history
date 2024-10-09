#include "parsing.h"

static const char ENVIRON_VAR_KEY = '$';
static const char EXIT_STATUS_MAGIC_VAR[] = "?";

// parses an argument of the command stream input
static char *
get_token(char *buf, int idx)
{
	char *tok;
	int i;

	tok = (char *) calloc(ARGSIZE, sizeof(char));
	i = 0;

	while (buf[idx] != SPACE && buf[idx] != END_STRING) {
		tok[i] = buf[idx];
		i++;
		idx++;
	}

	return tok;
}

// parses and changes stdin/out/err if needed
static bool
parse_redir_flow(struct execcmd *c, char *arg)
{
	int inIdx, outIdx;

	// flow redirection for output
	if ((outIdx = block_contains(arg, '>')) >= 0) {
		switch (outIdx) {
		// stdout redir
		case 0: {
			strcpy(c->out_file, arg + 1);
			break;
		}
		// stderr redir
		case 1: {
			strcpy(c->err_file, &arg[outIdx + 1]);
			break;
		}
		}

		free(arg);
		c->type = REDIR;

		return true;
	}

	// flow redirection for input
	if ((inIdx = block_contains(arg, '<')) >= 0) {
		// stdin redir
		strcpy(c->in_file, arg + 1);

		c->type = REDIR;
		free(arg);

		return true;
	}

	return false;
}

// parses and sets a pair KEY=VALUE
// environment variable
static bool
parse_environ_var(struct execcmd *c, char *arg)
{
	// sets environment variables apart from the
	// ones defined in the global variable "environ"
	if (block_contains(arg, '=') > 0) {
		// checks if the KEY part of the pair
		// does not contain a '-' char which means
		// that it is not a environ var, but also
		// an argument of the program to be executed
		// (For example:
		// 	./prog -arg=value
		// 	./prog --arg=value
		// )
		if (block_contains(arg, '-') < 0) {
			c->eargv[c->eargc++] = arg;
			return true;
		}
	}

	return false;
}

/*
 * Returns true if `arg` is an environment variable
 */
static bool
is_environ_var(char *arg, int arg_len)
{
	if (arg_len == 0) {
		return false;
	}

	if (arg[0] == ENVIRON_VAR_KEY) {
		return true;
	}

	return false;
}

/*
 * Returns true if `var_name` matches with a magic variable
 */
static bool
is_magic_variable(char *var_name, int var_len)
{
	int max_len = MAX(var_len, (int) strlen(EXIT_STATUS_MAGIC_VAR));
	return strncmp(var_name, EXIT_STATUS_MAGIC_VAR, max_len + 1) == 0;
}

/*
 * Fills `magic_var_buff` with the matching magic variable content.
 * If the magic variable is `EXIT_STATUS_MAGIC_VAR` the content is the last built-in
 * exit status code.
 * At last, `environ_var_content` points to `magic_var_buff`
 */
static void
fill_with_magic_variable(char **environ_var_content,
                         char magic_var_buff[MAGIC_VAR_BUFF_LEN],
                         int *status)
{
	snprintf(magic_var_buff, MAGIC_VAR_BUFF_LEN - 1, "%d", *status);
	*environ_var_content = magic_var_buff;
}

// this function will be called for every token, and it should
// expand environment variables. In other words, if the token
// happens to start with '$', the correct substitution with the
// environment value should be performed. Otherwise the same
// token is returned. If the variable does not exist, an empty string should be
// returned within the token
//
// Hints:
// - check if the first byte of the argument contains the '$'
// - expand it and copy the value in 'arg'
// - remember to check the size of variable's value
//		It could be greater than the current size of 'arg'
//		If that's the case, you should realloc 'arg' to the new size.
static char *
expand_environ_var(char *arg, bool *was_expanded, int *status)
{
	int arg_len = strlen(arg);
	if (!is_environ_var(arg, arg_len)) {
		return arg;
	}

	// Ignore the ENVIRON_VAR_KEY
	char *environ_var_name = arg + 1;

	char *environ_var_content = NULL;
	char magic_var_buff[MAGIC_VAR_BUFF_LEN] = { END_STRING };

	if (is_magic_variable(environ_var_name, arg_len - 1)) {
		fill_with_magic_variable(&environ_var_content,
		                         magic_var_buff,
		                         status);
	} else {
		environ_var_content = getenv(environ_var_name);
	}
	*was_expanded = true;

	if (environ_var_content == NULL) {
		arg[0] = END_STRING;
		return arg;
	}

	int environ_var_len = strlen(environ_var_content);

	// environ_var_content fits inside arg
	if (environ_var_len > 0 && environ_var_len < arg_len) {
		strncpy(arg, environ_var_content, arg_len * sizeof(char) + 1);
		return arg;

		// environ_var_content does not fit inside arg, thus needing reallocation
	} else if (environ_var_len > 0 && environ_var_len >= arg_len) {
		char *temp_buff =
		        realloc(arg, environ_var_len * sizeof(char) + 2);
		if (temp_buff == NULL) {
			perror("Error while allocating memory");
			exit(EXIT_FAILURE);
		}

		arg = temp_buff;
		strncpy(arg,
		        environ_var_content,
		        environ_var_len * sizeof(char) + 1);
		return arg;
	}

	// environ_var_content is empty
	arg[0] = END_STRING;
	return arg;
}

// parses one single command having into account:
// - the arguments passed to the program
// - stdin/stdout/stderr flow changes
// - environment variables (expand and set)
static struct cmd *
parse_exec(char *buf_cmd, int *status)
{
	struct execcmd *c;
	char *tok;
	int idx = 0, argc = 0;
	bool was_expanded = false;
	bool should_index_tok = true;

	c = (struct execcmd *) exec_cmd_create(buf_cmd);

	while (buf_cmd[idx] != END_STRING) {
		was_expanded = false;
		tok = get_token(buf_cmd, idx);
		idx = idx + strlen(tok);

		if (buf_cmd[idx] != END_STRING)
			idx++;

		if (parse_redir_flow(c, tok))
			continue;

		if (parse_environ_var(c, tok))
			continue;

		tok = expand_environ_var(tok, &was_expanded, status);

		should_index_tok =
		        !was_expanded || (was_expanded && strlen(tok) > 0);
		if (should_index_tok) {
			c->argv[argc++] = tok;
		} else {
			free(tok);
			tok = NULL;
		}
	}

	c->argv[argc] = (char *) NULL;
	c->argc = argc;

	return (struct cmd *) c;
}

// parses a command knowing that it contains the '&' char
static struct cmd *
parse_back(char *buf_cmd, int *status)
{
	int i = 0;
	struct cmd *e;

	while (buf_cmd[i] != '&')
		i++;

	buf_cmd[i] = END_STRING;

	e = parse_exec(buf_cmd, status);

	return back_cmd_create(e);
}

// parses a command and checks if it contains the '&'
// (background process) character
static struct cmd *
parse_cmd(char *buf_cmd, int *status)
{
	if (strlen(buf_cmd) == 0)
		return NULL;

	int idx;

	// checks if the background symbol is after
	// a redir symbol, in which case
	// it does not have to run in in the 'back'
	if ((idx = block_contains(buf_cmd, '&')) >= 0 && buf_cmd[idx - 1] != '>')
		return parse_back(buf_cmd, status);

	return parse_exec(buf_cmd, status);
}

// parses the command line
// looking for the pipe character '|'
struct cmd *
parse_line(char *buf, int *status)
{
	struct cmd *r, *l;

	char *right = split_line(buf, '|');

	l = parse_cmd(buf, status);
	r = parse_cmd(right, status);

	return pipe_cmd_create(l, r);
}
