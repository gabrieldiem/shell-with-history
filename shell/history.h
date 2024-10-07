#ifndef HISTORY_H
#define HISTORY_H

#include "defs.h"

#define MAX_HISTORY 400
#define HISTFILE_PATH "~/.fisop_history"
char history_vector[MAX_HISTORY][BUFLEN];
int history_count = 0;
int history_index = -1;

#endif  // HISTORY_H