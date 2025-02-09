#ifndef READLINE_H
#define READLINE_H

#include "history.h"

char *read_line(const char *prompt);

char *read_line_non_canonical(const char *prompt,
                              bool *just_handled_arrow,
                              history_data_t *history);

#endif  // READLINE_H
