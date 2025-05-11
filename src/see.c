/*
 * see - Minimal, cross-platform file content display utility.
 * High performance sequential file reader with binary data support.
 * Optimized for speed, minimal size, and broad compatibility.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#ifndef EPIPE
#define EPIPE 32 /* Broken pipe error code */
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

#define PROG_NAME "see"
#define VERSION   "v1.0"
#define BUFFER_SIZE (64 * 1024) /* 64KB for optimal I/O performance */

/* Configure platform-specific I/O settings */
static void platform_setup(void) {
#ifdef _WIN32
	/* Set UTF-8 console output and binary mode for stdin/stdout */
	SetConsoleOutputCP(CP_UTF8);

	int fd = _fileno(stdin);
	if (fd != -1) {
		if (_setmode(fd, _O_BINARY) == -1) {
			fprintf(stderr, "%s: stdin: failed to set binary mode: %s\n", PROG_NAME, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	fd = _fileno(stdout);
	if (fd != -1) {
		if (_setmode(fd, _O_BINARY) == -1) {
			fprintf(stderr, "%s: stdout: failed to set binary mode: %s\n", PROG_NAME, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
#else
	/* Ignore SIGPIPE to handle broken pipe gracefully */
	signal(SIGPIPE, SIG_IGN);
#endif
}

/* Display usage information */
static void usage(void) {
    static const char *usage_text =
        "Usage: %s [OPTION]... [FILE]...\n"
        "Concatenate FILE(s) to standard output.\n"
        "With no FILE, or when FILE is -, read standard input.\n\n"
        "Options:\n"
        "  %-2s, %-15s %s\n"
        "  %-2s, %-15s %s\n";
    fprintf(stdout, usage_text,
        PROG_NAME,
        "-h", "--help",    "display this help",
        "-v", "--version", "output version information"
    );
    exit(EXIT_SUCCESS);
}

/* Display version information */
static void version(void) {
	printf("%s %s\n", PROG_NAME, VERSION);
}

/* Copy data from input stream to stdout
 * Returns 0 on success, 1 on error
 */
static int copy_stream(FILE *in, const char *stream_name) {
	/* Static buffer for improved cache locality and reduced allocations */
	static unsigned char buffer[BUFFER_SIZE];
	size_t bytes_read;
	while ((bytes_read = fread(buffer, 1, sizeof(buffer), in)) > 0) {
		size_t written_total = 0;
		while (written_total < bytes_read) {
			size_t written = fwrite(buffer + written_total, 1, bytes_read - written_total, stdout);
			if (written == 0) {
				/* Handle broken pipe gracefully */
				if (errno != EPIPE) {
					fprintf(stderr, "%s: write error: %s\n", PROG_NAME, strerror(errno));
					return 1;
				}
				return 0;
			}
			written_total += written;
		}
	}

	if (ferror(in)) {
		fprintf(stderr, "%s: read error on %s: %s\n", PROG_NAME, stream_name, strerror(errno));
		return 1;
	}
	return 0;
}

/* Process a file or stdin
 * Returns 0 on success, 1 on error
 */
static int process_path(const char *path) {
	FILE *input_file;
	int status = 0;

	if (path == NULL || strcmp(path, "-") == 0) {
		return copy_stream(stdin, "stdin");
	}

	input_file = fopen(path, "rb");
	if (!input_file) {
		fprintf(stderr, "%s: %s: %s\n", PROG_NAME, path, strerror(errno));
		return 1;
	}

	if (copy_stream(input_file, path) != 0) {
		status = 1;
	}

	if (fclose(input_file) != 0) {
		fprintf(stderr, "%s: %s: close error: %s\n", PROG_NAME, path, strerror(errno));
		status = 1;
	}
	return status;
}

int main(int argc, char *argv[]) {
	int i;
	int overall_rc = 0;
	static unsigned char outbuf[BUFFER_SIZE];

	platform_setup();

	if (setvbuf(stdout, (char*)outbuf, _IOFBF, sizeof(outbuf)) != 0) {
		/* Non-critical: proceed with default buffering */
	}

	/* Simple argument parsing: -h/--help and -v/--version take precedence */
	for (i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			usage();
			return EXIT_SUCCESS;
		}
		if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
			version();
			return EXIT_SUCCESS;
		}
	}

	if (argc == 1) {
		/* No arguments: process stdin */
		overall_rc |= process_path(NULL);
	} else {
		/* Process each file argument */
		for (i = 1; i < argc; ++i) {
			overall_rc |= process_path(argv[i]);
		}
	}

	/* Flush stdout and check for errors (ignore EPIPE) */
	if (fflush(stdout) != 0) {
		if (errno != EPIPE) {
			fprintf(stderr, "%s: flush error on stdout: %s\n", PROG_NAME, strerror(errno));
			overall_rc = 1;
		}
	}

	/* Flush stderr (silently handle errors) */
	if (fflush(stderr) != 0) {
		overall_rc = 1;
	}

	return (overall_rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
