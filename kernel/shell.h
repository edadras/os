#ifndef SHELL_H
#define SHELL_H

void shell_run(void);
void shell_execute(char *line);     /* run one command line */
void shell_banner(void);            /* logo + welcome message */

#endif
