/*
 * see - Minimal, cross-platform file content display utility.
 * High performance sequential file reader with binary data support.
 * Optimized for speed, minimal size, and broad compatibility (C89).
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L /* For sigaction */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h> /* SetConsoleOutputCP */
#include <io.h>      /* _setmode, _fileno */
#include <fcntl.h>   /* _O_BINARY */
#endif

#define PROG_NAME "see"
#define VERSION   "v1.0"
#define BUFFER_SIZE (64 * 1024) /* 64KB: common optimal size for disk I/O */

/* Configure platform-specific settings for console and I/O streams. */
static void platform_setup(void) {
#ifdef _WIN32
	/* Ensure UTF-8 output on Windows console. */
	if (!SetConsoleOutputCP(CP_UTF8)) {
		/* Non-fatal, but warn the user about potential display issues. */
		fprintf(stderr, "%s: warning: failed to set console output to UTF-8 (error code: %lu)\n",
		        PROG_NAME, (unsigned long)GetLastError());
	}

	/* Set stdin/stdout to binary mode to prevent CRLF translation. */
	if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
		fprintf(stderr, "%s: stdin: failed to set binary mode: %s\n", PROG_NAME, strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
		fprintf(stderr, "%s: stdout: failed to set binary mode: %s\n", PROG_NAME, strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (_setmode(_fileno(stderr), _O_BINARY) == -1) {
		fprintf(stderr, "%s: stderr: failed to set binary mode: %s\n", PROG_NAME, strerror(errno));
		exit(EXIT_FAILURE); /* Critical for consistent error reporting if redirected */
	}
#else
	/* On POSIX, ignore SIGPIPE. Write errors (like EPIPE) are handled explicitly. */
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL) == -1) {
		fprintf(stderr, "%s: failed to ignore SIGPIPE: %s\n", PROG_NAME, strerror(errno));
		exit(EXIT_FAILURE);
	}
#endif
}

/* Display usage information and exit successfully. */
static void usage(void) {
    static const char *usage_text =
        "Usage: %s [OPTION]... [FILE]...\n"
        "Concatenate FILE(s) to standard output.\n"
        "With no FILE, or when FILE is -, read standard input.\n\n"
        "Options:\n"
        "  -h, --help     display this help\n"
        "  -v, --version  output version information\n";
    fprintf(stdout, usage_text, PROG_NAME);
    exit(EXIT_SUCCESS);
}

/* Display version information and exit successfully. */
static void version(void) {
	printf("%s %s\n", PROG_NAME, VERSION);
	exit(EXIT_SUCCESS);
}

/*
 * Copy data from input stream to stdout using a fixed-size buffer.
 * Handles write errors, including EPIPE (broken pipe) gracefully.
 * Returns 0 on success, 1 on read/write error (excluding EPIPE).
 */
static int copy_stream(FILE *in, const char *stream_name) {
	/* Static buffer minimizes stack usage and allocation overhead. */
	static unsigned char buffer[BUFFER_SIZE];
	size_t bytes_read;
	size_t written_total;
	size_t written;

	while (1) {
		bytes_read = fread(buffer, 1, sizeof(buffer), in);
		if (bytes_read == 0) {
			if (feof(in)) {
				break;
			}
			if (ferror(in)) {
				if (errno == EINTR) {
					clearerr(in);
					continue; /* Retry interrupted read */
				}
				fprintf(stderr, "%s: read error on %s: %s\n", PROG_NAME, stream_name, strerror(errno));
				return 1;
			}
			break; /* Treat unexpected zero read without error/EOF as EOF */
		}

		written_total = 0;
		/* Loop handles potential partial writes. */
		while (written_total < bytes_read) {
			written = fwrite(buffer + written_total, 1, bytes_read - written_total, stdout);
			if (written == 0) { /* fwrite returns 0 on error or EOF. Check ferror and errno. */
				if (ferror(stdout)) {
#ifdef EPIPE
				if (errno == EPIPE) {
					/* Broken pipe: reader closed connection. Not an error for 'see'. */
					clearerr(stdout);
					return 0;
				}
#endif
					if (errno == EINTR) {
						clearerr(stdout);
						continue; /* Retry interrupted write */
					}
					fprintf(stderr, "%s: write error: %s\n", PROG_NAME, strerror(errno));
					return 1;
				} else {
					/* fwrite returned 0 but ferror is not set (e.g. EOF on stdout). */
					fprintf(stderr, "%s: write error: unexpected zero write\n", PROG_NAME);
					return 1;
				}
			}
			written_total += written;
		}
	}

	return 0; /* Success */
}

/*
 * Open and process a single file path, or stdin if path is NULL or "-".
 * Returns 0 on success, 1 on error (file open, read, write, close).
 */
static int process_path(const char *path) {
	FILE *input_file;
	int status = 0; /* Assume success initially */

	if (path == NULL || strcmp(path, "-") == 0) {
		return copy_stream(stdin, "stdin");
	}

	input_file = fopen(path, "rb");
	if (!input_file) {
		fprintf(stderr, "%s: %s: %s\n", PROG_NAME, path, strerror(errno));
		return 1; /* Indicate failure */
	}

	if (copy_stream(input_file, path) != 0) {
		status = 1; /* Record failure */
	}

	if (fclose(input_file) != 0) {
		fprintf(stderr, "%s: %s: close error: %s\n", PROG_NAME, path, strerror(errno));
		status = 1; /* Record failure */
	}

	return status;
}

int main(int argc, char *argv[]) {
	int files_processed = 0;
	int i;                   /* C89 requires loop counter declaration at block start */
	int overall_rc = 0;
	int options_ended = 0;
	static char s_stdout_buf[BUFFER_SIZE];

	platform_setup();

	if (setvbuf(stdout, s_stdout_buf, _IOFBF, sizeof(s_stdout_buf)) != 0) {
		/* Non-critical: proceed with default buffering. */
	}

	for (i = 1; i < argc; ++i) {
		if (!options_ended) {
			if (strcmp(argv[i], "--") == 0) {
				options_ended = 1;
				continue;
			}
			if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
				usage(); /* Exits */
			} else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
				version(); /* Exits */
			}
		}
		overall_rc |= process_path(argv[i]);
		files_processed = 1;
	}

	if (!files_processed) {
		overall_rc |= process_path(NULL); /* NULL indicates stdin */
	}

	while (fflush(stdout) != 0) {
		/* EPIPE is not an error for 'see'; stdout considered closed. */
		if (errno == EPIPE) {
			clearerr(stdout); /* Clear error state. */
			break;
		}
		if (errno == EINTR) {
			clearerr(stdout); /* Clear error state. */
			continue; /* Retry interrupted flush */
		}
		fprintf(stderr, "%s: flush error on stdout: %s\n", PROG_NAME, strerror(errno));
		overall_rc = 1;
		break;
	}

	while (fflush(stderr) != 0) {
		/* Potentially log this, but usually not fatal. */
		if (errno == EINTR) {
			clearerr(stderr); /* Clear error state. */
			continue; /* Retry interrupted flush */
		}
		/* For other errors (e.g. EPIPE), cannot print error for failing stderr flush. */
		overall_rc = 1; /* Consider if stderr flush failure warrants exit(1) */
		break;
	}

	return (overall_rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
