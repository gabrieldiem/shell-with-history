#ifndef UTILS_H
#define UTILS_H

#include "defs.h"

char *split_line(char *buf, char splitter);

int block_contains(char *buf, char c);

int printf_debug(char *format, ...);
int fprintf_debug(FILE *file, char *format, ...);

void update_prompt(char prompt[PRMTLEN], char *new_prompt_text);

int parse_exit_code(int raw_exit_code);

#endif  // UTILS_H
