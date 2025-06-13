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
#define BUFFER_SIZE (64 * 1024) /* 64KB: disk I/O sweet spot */

static void platform_setup(void) {
#ifdef _WIN32
	if (!SetConsoleOutputCP(CP_UTF8)) {
		fprintf(stderr, "%s: warning: failed to set console output to UTF-8 (error code: %lu)\n",
		        PROG_NAME, (unsigned long)GetLastError());
	}

	/* Binary mode prevents CRLF translation corruption */
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
		exit(EXIT_FAILURE);
	}
#else
	/* Ignore SIGPIPE - handle EPIPE explicitly for robustness */
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL) == -1) {
		fprintf(stderr, "%s: failed to ignore SIGPIPE: %s\n", PROG_NAME, strerror(errno));
		exit(EXIT_FAILURE);
	}
#endif
}

static void usage(void) {
	static const char usage_text[] =
		"Usage: %s [OPTION]... [FILE]...\n"
		"Concatenate FILE(s) to standard output.\n"
		"With no FILE, or when FILE is -, read standard input.\n\n"
		"Options:\n"
		"  -h, --help     display this help\n"
		"  -v, --version  output version information\n";
	fprintf(stdout, usage_text, PROG_NAME);
	exit(EXIT_SUCCESS);
}

static void version(void) {
	printf("%s %s\n", PROG_NAME, VERSION);
	exit(EXIT_SUCCESS);
}

static int copy_stream(FILE *in, const char *stream_name) {
	/* Static buffer avoids stack pressure and malloc overhead */
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
					continue;
				}
				fprintf(stderr, "%s: read error on %s: %s\n", PROG_NAME, stream_name, strerror(errno));
				return 1;
			}
			break; /* Zero read without error/EOF treated as EOF */
		}

		written_total = 0;
		/* Handle partial writes - critical for pipes and slow devices */
		while (written_total < bytes_read) {
			written = fwrite(buffer + written_total, 1, bytes_read - written_total, stdout);
			if (written == 0) {
				if (ferror(stdout)) {
#ifdef EPIPE
					if (errno == EPIPE) {
						/* Broken pipe is normal termination for utilities */
						clearerr(stdout);
						return 0;
					}
#endif
					if (errno == EINTR) {
						clearerr(stdout);
						continue;
					}
					fprintf(stderr, "%s: write error: %s\n", PROG_NAME, strerror(errno));
					return 1;
				} else {
					fprintf(stderr, "%s: write error: unexpected zero write\n", PROG_NAME);
					return 1;
				}
			}
			written_total += written;
		}
	}

	return 0;
}

static int process_path(const char *path) {
	FILE *input_file;
	int status = 0;
	static char file_buf[BUFFER_SIZE];

	if (path == NULL || strcmp(path, "-") == 0) {
		return copy_stream(stdin, "stdin");
	}

	input_file = fopen(path, "rb");
	if (!input_file) {
		fprintf(stderr, "%s: %s: %s\n", PROG_NAME, path, strerror(errno));
		return 1;
	}

	if (setvbuf(input_file, file_buf, _IOFBF, sizeof(file_buf)) != 0) {
		fprintf(stderr, "%s: %s: warning: failed to set full buffering: %s\n",
		        PROG_NAME, path, strerror(errno));
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

int main(int argc, const char *argv[]) {
	int files_processed = 0;
	int i; /* C89 requires declaration at block start */
	int overall_rc = 0;
	int options_ended = 0;
	static char s_stdout_buf[BUFFER_SIZE];

	platform_setup();

	/* Full buffering improves performance for large outputs */
	if (setvbuf(stdout, s_stdout_buf, _IOFBF, sizeof(s_stdout_buf)) != 0) {
		/* Non-critical failure */
		fprintf(stderr, "%s: warning: failed to set full buffering on stdout: %s\n",
		        PROG_NAME, strerror(errno));
	}

	for (i = 1; i < argc; ++i) {
		const char *arg = argv[i];
		int is_filepath = 1;

		if (!options_ended && arg[0] == '-') {
			if (strcmp(arg, "--") == 0) {
				options_ended = 1;
				is_filepath = 0;
			} else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
				usage();
			} else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
				version();
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

	/* Explicit flush with EPIPE/EINTR handling */
	while (fflush(stdout) != 0) {
		if (errno == EPIPE) {
			clearerr(stdout);
			break;
		}
		if (errno == EINTR) {
			clearerr(stdout);
			continue;
		}
		fprintf(stderr, "%s: flush error on stdout: %s\n", PROG_NAME, strerror(errno));
		overall_rc = 1;
		break;
	}

	while (fflush(stderr) != 0) {
		if (errno == EINTR) {
			clearerr(stderr);
			continue;
		}
		/* Cannot report stderr flush errors to stderr */
		overall_rc = 1;
		break;
	}

	return (overall_rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
