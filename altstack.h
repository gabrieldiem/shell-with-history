#ifndef ALTSTACK_H
#define ALTSTACK_H

#include "defs.h"

void init_alternative_stack(stack_t *alternative_stack);

void install_alternative_stack(stack_t *alternative_stack);

void free_alternative_stack(stack_t *alternative_stack);

#endif  // ALTSTACK_H
