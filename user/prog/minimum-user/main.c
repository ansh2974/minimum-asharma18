#include <stdint.h>

#include "minemu/user_abi.h"

/* msh: the minimum shell, running in USR mode. All console I/O goes
 * through the ioctl syscall on the UART0 file descriptors. */

/* Maximum number of bytes in one command line, not counting the '\n'.
 * The assignment text says 20, but the public tests in tests/hw2 expect
 * 22- and 25-byte lines to run (and a 140-byte line to be discarded). */
#define MSH_LINE_MAX 64
#define MAX_WORDS (MSH_LINE_MAX / 2 + 1)
#define INPUT_CHUNK 32

/* ---- console I/O ---- */

static void write_bytes(const char *s, uint32_t n) {
    while (n > 0) {
        int32_t wrote = ioctl(MINEMU_FD_UART_OUT, MINEMU_IOCTL_UART_WRITE, (void *)s, n);
        if (wrote <= 0) {
            return;
        }
        s += wrote;
        n -= (uint32_t)wrote;
    }
}

static void put_str(const char *s) {
    uint32_t n = 0;
    while (s[n] != '\0') {
        ++n;
    }
    write_bytes(s, n);
}

/* Blocking byte input. uart_read is non-blocking, so poll until at least
 * one byte arrives, keeping any extra bytes for later calls. */
static char get_char(void) {
    static char input[INPUT_CHUNK];
    static int32_t input_len;
    static int32_t input_pos;

    while (input_pos >= input_len) {
        input_pos = 0;
        input_len = ioctl(MINEMU_FD_UART_IN, MINEMU_IOCTL_UART_READ, (void *)input,
                          (uint32_t)sizeof(input));
        if (input_len < 0) {
            input_len = 0;
        }
    }
    return input[input_pos++];
}

/* ---- line editing and parsing (same behavior as the HW1 kernel shell) ---- */

/* Read one line terminated by '\n' into buf (NUL-terminated).
 * Returns the line length, or -1 if the line was longer than MSH_LINE_MAX.
 *
 * len counts every byte currently on the line, even past the buffer
 * capacity, so backspacing an overlong line back under the limit still
 * leaves the correct bytes in buf. */
static int32_t read_line(char *buf) {
    uint32_t len = 0;

    for (;;) {
        char c = get_char();

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

/* ---- commands ---- */

static void cmd_echo(uint32_t argc, char **argv) {
    for (uint32_t i = 1; i < argc; ++i) {
        if (i > 1) {
            put_str(" ");
        }
        put_str(argv[i]);
    }
    put_str("\n");
}

void minemu_user_main(void) {
    char line[MSH_LINE_MAX + 1];
    char *words[MAX_WORDS];

    for (;;) {
        put_str("msh> ");

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
            put_str("command not found: ");
            put_str(words[0]);
            put_str("\n");
        }
    }
}
