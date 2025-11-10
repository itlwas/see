/*
 * see - Minimal, cross-platform file content display utility.
 * High-performance sequential file reader with binary data support.
 * Optimized for speed, minimal size, and broad compatibility (C89).
 */

#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L /* For sigaction() on POSIX */
#endif
#endif

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h> /* SetConsoleOutputCP */
#include <io.h>      /* _setmode, _fileno */
#include <fcntl.h>   /* _O_BINARY */
#endif

#define PROG_NAME   "see"
#define VERSION     "v1.0"
#define BUFFER_SIZE (64 * 1024) /* 64KB: good disk I/O sweet spot */

/* Forward declarations */
static void platform_setup(void);
static void usage(void);
static void version(void);
static int  flush_stream(FILE *stream, const char *stream_name, int treat_broken_pipe_as_success);
static int  copy_stream(FILE *input_stream, const char *input_name);
static int  process_path(const char *file_path);

/* Platform-specific initialization. Exits on unrecoverable failures. */
static void platform_setup(void) {
#ifdef _WIN32
    /* Best-effort UTF-8 console; failure is non-fatal. */
    if (!SetConsoleOutputCP(CP_UTF8)) {
        fprintf(stderr,
                "%s: warning: failed to set console output to UTF-8 (error code: %lu)\n",
                PROG_NAME, (unsigned long)GetLastError());
    }

    /* Binary mode prevents CRLF translation corruption. */
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        int err = errno;
        fprintf(stderr, "%s: stdin: failed to set binary mode: %s\n",
                PROG_NAME, strerror(err));
        exit(EXIT_FAILURE);
    }
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        int err = errno;
        fprintf(stderr, "%s: stdout: failed to set binary mode: %s\n",
                PROG_NAME, strerror(err));
        exit(EXIT_FAILURE);
    }
    if (_setmode(_fileno(stderr), _O_BINARY) == -1) {
        int err = errno;
        fprintf(stderr, "%s: stderr: failed to set binary mode: %s\n",
                PROG_NAME, strerror(err));
        exit(EXIT_FAILURE);
    }
#else
    /* Ignore SIGPIPE - handle EPIPE explicitly for robustness. */
    struct sigaction sa;
    int err;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        err = errno;
        fprintf(stderr, "%s: failed to ignore SIGPIPE: %s\n",
                PROG_NAME, strerror(err));
        exit(EXIT_FAILURE);
    }
#endif
}

/* Print usage and exit successfully. */
static void usage(void) {
    static const char usage_text[] =
        "Usage: %s [OPTION]... [FILE]...\n"
        "Concatenate FILE(s) to standard output.\n"
        "With no FILE, or when FILE is -, read standard input.\n\n"
        "Options:\n"
        "  -h, --help     display this help\n"
        "  -v, --version  output version information\n";
    printf(usage_text, PROG_NAME);
    (void)flush_stream(stdout, "stdout", 1);
    exit(EXIT_SUCCESS);
}

/* Print version and exit successfully. */
static void version(void) {
    printf("%s %s\n", PROG_NAME, VERSION);
    (void)flush_stream(stdout, "stdout", 1);
    exit(EXIT_SUCCESS);
}

/* Robust fflush with EINTR and optional EPIPE handling.
 * Returns 0 on success (including EPIPE handled as non-error), 1 on error.
 * If 'stream_name' is NULL, suppresses error messages (used for stderr). */
static int flush_stream(FILE *stream, const char *stream_name, int treat_broken_pipe_as_success) {
    int err;

    for (;;) {
        if (fflush(stream) == 0) {
            return 0;
        }

        err = errno;
        if (err == EINTR) {
            clearerr(stream);
            continue;
        }

        if (treat_broken_pipe_as_success) {
#ifdef EPIPE
            if (err == EPIPE) {
                clearerr(stream);
                return 0; /* Treat broken pipe as normal termination. */
            }
#endif
        }

        if (stream_name != NULL) {
            fprintf(stderr, "%s: flush error on %s: %s\n",
                    PROG_NAME, stream_name, strerror(err));
        }
        return 1;
    }
}

/* Copy all data from 'input_stream' to stdout. Returns 0 on success, 1 on error.
 * 'input_name' is used for diagnostics. */
static int copy_stream(FILE *input_stream, const char *input_name) {
    /* Static buffer avoids stack pressure and malloc overhead. */
    static unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    size_t total_bytes_written;
    size_t bytes_written;

    for (;;) {
        bytes_read = fread(buffer, 1, sizeof(buffer), input_stream);
        if (bytes_read == 0) {
            if (feof(input_stream)) {
                break;
            }
            if (ferror(input_stream)) {
                int err = errno;
                if (err == EINTR) {
                    clearerr(input_stream);
                    continue;
                }
                fprintf(stderr, "%s: read error on %s: %s\n",
                        PROG_NAME, input_name, strerror(err));
                return 1;
            }
            /* Zero read without error/EOF treated as EOF. */
            break;
        }

        /* Handle partial writes - critical for pipes and slow devices. */
        total_bytes_written = 0;
        while (total_bytes_written < bytes_read) {
            bytes_written = fwrite(buffer + total_bytes_written, 1,
                                   bytes_read - total_bytes_written, stdout);
            if (bytes_written == 0) {
                if (ferror(stdout)) {
                    int err = errno;
#ifdef EPIPE
                    if (err == EPIPE) {
                        /* Broken pipe is normal termination for utilities. */
                        clearerr(stdout);
                        return 0;
                    }
#endif
                    if (err == EINTR) {
                        clearerr(stdout);
                        continue;
                    }
                    fprintf(stderr, "%s: write error on stdout: %s\n",
                            PROG_NAME, strerror(err));
                    return 1;
                } else {
                    fprintf(stderr, "%s: write error on stdout: unexpected zero write\n",
                            PROG_NAME);
                    return 1;
                }
            } else {
                total_bytes_written += bytes_written;
            }
        }
    }

    return 0;
}

/* Process a path or stdin ("-" or NULL). Returns 0 on success, 1 on error. */
static int process_path(const char *file_path) {
    FILE *input_file;
    int status = 0;
    static char file_buf[BUFFER_SIZE];

    if (file_path == NULL || strcmp(file_path, "-") == 0) {
        return copy_stream(stdin, "stdin");
    }

    input_file = fopen(file_path, "rb");
    if (!input_file) {
        int err = errno;
        fprintf(stderr, "%s: %s: %s\n", PROG_NAME, file_path, strerror(err));
        return 1;
    }

    if (setvbuf(input_file, file_buf, _IOFBF, sizeof(file_buf)) != 0) {
        int err = errno;
        fprintf(stderr, "%s: %s: warning: failed to set full buffering: %s\n",
                PROG_NAME, file_path, strerror(err));
    }

    if (copy_stream(input_file, file_path) != 0) {
        status = 1;
    }

    if (fclose(input_file) != 0) {
        int err = errno;
        fprintf(stderr, "%s: %s: close error: %s\n",
                PROG_NAME, file_path, strerror(err));
        status = 1;
    }

    return status;
}

int main(int argc, char *argv[]) {
    int files_processed = 0;
    int i; /* C89 requires declaration at block start. */
    int overall_rc = 0;
    int options_ended = 0;
    static char stdout_buf[BUFFER_SIZE];

    platform_setup();

    /* Full buffering improves performance for large outputs. */
    if (setvbuf(stdout, stdout_buf, _IOFBF, sizeof(stdout_buf)) != 0) {
        int err = errno; /* Non-critical failure. */
        fprintf(stderr, "%s: warning: failed to set full buffering on stdout: %s\n",
                PROG_NAME, strerror(err));
    }

    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        int is_filepath = 1;

        if (!options_ended && arg[0] == '-') {
            if (strcmp(arg, "--") == 0) {
                options_ended = 1;
                is_filepath = 0;
            } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
                usage();   /* no return */
            } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
                version(); /* no return */
            }
        }

        if (is_filepath) {
            overall_rc |= process_path(arg);
            files_processed = 1;
        }
    }

    if (!files_processed) {
        overall_rc |= process_path(NULL);
    }

    /* Explicit flush with EINTR and optional EPIPE handling. */
    overall_rc |= flush_stream(stdout, "stdout", 1);
    overall_rc |= flush_stream(stderr, NULL, 0); /* Cannot report to stderr. */

    return (overall_rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
