#include "altstack.h"

/*
 * Frees the alternative stack memory.
 */
void
free_alternative_stack(stack_t *alternative_stack)
{
	if (alternative_stack->ss_sp != NULL) {
		free(alternative_stack->ss_sp);
		alternative_stack->ss_sp = NULL;
	}
}

/*
 * Initializes the alternative stack with heap memory.
 */
void
init_alternative_stack(stack_t *alternative_stack)
{
	void *alt_stack_ptr = malloc(SIGSTKSZ);

	if (alt_stack_ptr == NULL) {
		perror("Error while allocating memory for signal alternative "
		       "stack");
		exit(EXIT_FAILURE);
	}

	alternative_stack->ss_sp = alt_stack_ptr;
	alternative_stack->ss_size = SIGSTKSZ;
	alternative_stack->ss_flags = SS_ONSTACK;
}

/*
 * Installs the alternative stack.
 */
void
install_alternative_stack(stack_t *alternative_stack)
{
	if (sigaltstack(alternative_stack, NULL) == GENERIC_ERROR_CODE) {
		perror("Error while installing alternative stack");
		free_alternative_stack(alternative_stack);
		exit(EXIT_FAILURE);
	};
}
