/*
 * see - A minimal, cross-platform 'cat' implementation.
 * Aims for high performance, small binary size, and wide compatibility.
 * Reads files sequentially and writes their content to standard output.
 * Handles binary data correctly.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN /* Exclude rarely-used stuff from Windows headers */
#include <windows.h>
#include <io.h>   /* For _setmode, _fileno */
#include <fcntl.h> /* For _O_BINARY */
#endif

#define PROG_NAME "see"     /* Program name for messages */
#define VERSION   "v1.0"   /* Program version, updated to reflect C89 changes */
#define BUFFER_SIZE (64 * 1024) /* I/O buffer size (64KB) for efficient reads/writes */

/* Platform-specific setup */
static void platform_setup(void) {
#ifdef _WIN32
	/* Attempt to set console output to UTF-8 on Windows. */
	/* This aids in displaying non-ASCII characters if the console supports it. */
	SetConsoleOutputCP(CP_UTF8);

	/* Ensure stdin/stdout are in binary mode on Windows. */
	/* This prevents CR/LF translation and other text-mode transformations. */
	if (_fileno(stdin) != -1) {
		_setmode(_fileno(stdin), _O_BINARY);
	}
	if (_fileno(stdout) != -1) {
		_setmode(_fileno(stdout), _O_BINARY);
	}
#else
	/* For non-Windows systems, rely on system/terminal locale settings. */
	/* No specific setup needed here for standard behavior. */
	(void)0; /* Explicitly a no-op for clarity. */
#endif
}

/* Display usage information to stderr. */
static void usage(void) {
	fprintf(stderr,
		"Usage: %s [OPTION]... [FILE]...\n"
		"Concatenate FILE(s) to standard output.\n"
		"With no FILE, or when FILE is -, read standard input.\n\n"
		"  -h, --help     display this help and exit\n"
		"  -v, --version  output version information and exit\n",
		PROG_NAME);
}

/* Display version information to stdout. */
static void version(void) {
	printf("%s %s\n", PROG_NAME, VERSION);
}

/*
 * Copy data from input stream 'in' to standard output.
 * 'stream_name' is used for error messages (e.g., filename or "stdin").
 * Returns 0 on success, 1 on error.
 */
static int copy_stream(FILE *in, const char *stream_name) {
	/* Static buffer: avoids repeated stack allocation, improves cache locality. */
	/* Sized by BUFFER_SIZE for potentially large, efficient I/O operations. */
	static unsigned char buffer[BUFFER_SIZE];
	size_t bytes_read;
	size_t bytes_written;
	size_t chunk_written;

	while ((bytes_read = fread(buffer, 1, sizeof(buffer), in)) > 0) {
		chunk_written = 0;
		while (chunk_written < bytes_read) {
			bytes_written = fwrite(buffer + chunk_written, 1, bytes_read - chunk_written, stdout);
			if (bytes_written == 0) {
				/* fwrite returns 0 if count was > 0 and an error occurred before any items were written. */
				/* If count was 0, it also returns 0, but that's not an error. */
				/* Here, (bytes_read - chunk_written) is always > 0. */
				fprintf(stderr, "%s: write error: %s\n", PROG_NAME, strerror(errno));
				return 1;
			}
			chunk_written += bytes_written;
		}
	}

	if (ferror(in)) {
		fprintf(stderr, "%s: read error on %s: %s\n", PROG_NAME, stream_name, strerror(errno));
		return 1;
	}
	/* Output errors on stdout are checked by fflush(stdout) in main. */
	return 0;
}

/*
 * Process a single file path or standard input.
 * 'path' is the file path, or NULL or "-" for stdin.
 * Returns 0 on success, 1 on error.
 */
static int process_path(const char *path) {
	FILE *input_file;
	int status = 0;

	if (path == NULL || strcmp(path, "-") == 0) {
		return copy_stream(stdin, "stdin");
	}

	input_file = fopen(path, "rb"); /* Open in binary read mode ("b" is critical on Windows). */
	if (!input_file) {
		fprintf(stderr, "%s: %s: %s\n", PROG_NAME, path, strerror(errno));
		return 1;
	}

	if (copy_stream(input_file, path) != 0) {
		status = 1; /* Error occurred during copy_stream. */
		/* Continue to fclose the file. */
	}

	if (fclose(input_file) != 0) {
		fprintf(stderr, "%s: %s: close error: %s\n", PROG_NAME, path, strerror(errno));
		status = 1; /* Mark error; overrides previous success if any. */
	}
	return status;
}

int main(int argc, char *argv[]) {
	int i;
	int overall_rc = 0; /* Overall return code: 0 for success, 1 for any failure. */

	platform_setup();

	/*
	 * Argument parsing:
	 * Special options -h/--help and -v/--version take precedence.
	 * If found anywhere, they are actioned, and the program exits.
	 * This simple approach avoids complex option parsing for a minimal tool.
	 */
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
		/* No arguments: process standard input. */
		overall_rc |= process_path(NULL);
	} else {
		/* Process each argument as a file. argv[0] is the program name. */
		/* If -h/-v were present, we would have exited already. */
		for (i = 1; i < argc; ++i) {
			overall_rc |= process_path(argv[i]);
		}
	}

	/*
	 * Ensure all buffered output to stdout is written and check for errors.
	 * EPIPE (broken pipe) is common (e.g., `see file | head`) and typically
	 * not considered an error for `see` itself, so it's ignored.
	 */
	if (fflush(stdout) != 0) {
		if (errno != EPIPE) {
			fprintf(stderr, "%s: flush error on stdout: %s\n", PROG_NAME, strerror(errno));
			overall_rc = 1; /* Indicate failure. */
		}
	}

	/*
	 * Attempt to flush stderr as well, though errors are less common
	 * and harder to report if stderr itself is broken.
	 * If an error occurs here and stdout was fine, still report failure.
	 */
	if (fflush(stderr) != 0) {
		/* Silently ignore stderr flush errors to avoid recursive error reporting. */
		/* However, ensure the main return code reflects an issue if not already set. */
		if (overall_rc == 0) {
			overall_rc = 1;
		}
	}

	return (overall_rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
