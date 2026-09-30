#include <stdint.h>

#include "minimum/console.h"
#include "minimum/msh.h"
#include "minimum/uart.h"

#define MAX_WORDS (MSH_LINE_MAX / 2 + 1)

/* Read one line terminated by '\n' into buf (NUL-terminated).
 * Returns the line length, or -1 if the line was longer than MSH_LINE_MAX.
 *
 * len counts every byte currently on the line, even past the buffer
 * capacity, so backspacing an overlong line back under the limit still
 * leaves the correct bytes in buf. */
static int32_t read_line(char *buf) {
    uint32_t len = 0;

    for (;;) {
        char c = uart_getc();

        if (c == '\n') {
            if (len > MSH_LINE_MAX) {
                return -1;
            }
            buf[len] = '\0';
            return (int32_t)len;
        }

        if (c == 0x08 || c == 0x7f) {
            /* backspace; ignored on an empty line */
            if (len > 0) {
                --len;
            }
            continue;
        }

        if (len < MSH_LINE_MAX) {
            buf[len] = c;
        }
        if (len < UINT32_MAX) {
            ++len;
        }
    }
}

/* Split line in place on ASCII spaces. Repeated, leading and trailing
 * spaces are skipped. Returns the number of words. */
static uint32_t split_words(char *line, char **words) {
    uint32_t count = 0;
    char *p = line;

    while (*p != '\0') {
        while (*p == ' ') {
            ++p;
        }
        if (*p == '\0') {
            break;
        }
        words[count++] = p;
        while (*p != '\0' && *p != ' ') {
            ++p;
        }
        if (*p == ' ') {
            *p = '\0';
            ++p;
        }
    }
    return count;
}

static int str_equal(const char *a, const char *b) {
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

static void cmd_echo(uint32_t argc, char **argv) {
    for (uint32_t i = 1; i < argc; ++i) {
        if (i > 1) {
            kprintf(" ");
        }
        kprintf("%s", argv[i]);
    }
    kprintf("\n");
}

void msh_run(void) {
    char line[MSH_LINE_MAX + 1];
    char *words[MAX_WORDS];

    for (;;) {
        kprintf("msh> ");

        if (read_line(line) < 0) {
            /* overlong line: discard it */
            continue;
        }

        uint32_t count = split_words(line, words);
        if (count == 0) {
            /* empty or all-space line */
            continue;
        }

        if (str_equal(words[0], "echo")) {
            cmd_echo(count, words);
        } else {
            kprintf("command not found: %s\n", words[0]);
        }
    }
}
