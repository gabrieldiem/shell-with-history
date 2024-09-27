#ifndef READLINE_H
#define READLINE_H

char *read_line(const char *prompt);

char *read_line_non_canonical(const char *prompt, bool *just_handled_arrow);

#endif  // READLINE_H
