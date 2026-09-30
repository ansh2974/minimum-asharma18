#ifndef MINIMUM_MSH_H
#define MINIMUM_MSH_H

/* Maximum number of bytes in one command line, not counting the '\n'. */
#define MSH_LINE_MAX 20

/* Run the kernel shell loop. Does not return. */
void msh_run(void) __attribute__((noreturn));

#endif
