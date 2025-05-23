# see

A minimal, cross-platform file content display utility optimized for 
performance and compatibility.

## Overview

`see` is a command-line utility designed as a high-performance alternative 
to the traditional `cat` command. Built with C89 for maximum compatibility, 
it provides efficient file concatenation and display functionality across 
Windows, Linux, and macOS platforms.

## Key Features

- **High Performance**: Optimized 64KB buffer for efficient disk I/O operations
- **Cross-Platform**: Native support for Windows, Linux, and macOS
- **Binary Safe**: Handles binary files without content modification
- **UTF-8 Ready**: Proper UTF-8 output handling on Windows systems
- **Robust I/O**: Binary mode operations prevent CRLF translation issues
- **Pipe-Safe**: Graceful handling of broken pipes in command pipelines
- **Minimal Footprint**: Single C source file with optimized binary size
- **Standards Compliant**: Written in C89 for broad compiler compatibility

## Installation

### Build from Source

**Requirements:**
- C compiler with C89 standard support
- Make (optional but recommended)

**Using Make:**
```bash
make
```

**Manual Compilation:**
```bash
gcc -std=c89 -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Os -s -o see src/see.c
```

**Windows Compilation:**
```cmd
gcc -std=c89 -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Os -s -o see.exe src/see.c
```

## Usage

```
Usage: see [OPTION]... [FILE]...
Concatenate FILE(s) to standard output.
With no FILE, or when FILE is -, read standard input.

Options:
  -h, --help     display this help
  -v, --version  output version information
```

### Examples

**Display a single file:**
```bash
see filename.txt
```

**Concatenate multiple files:**
```bash
see file1.txt file2.txt file3.txt
```

**Read from standard input:**
```bash
echo "Hello, World!" | see
```

**Use with redirection:**
```bash
see file1.txt file2.txt > combined.txt
```

**Pipeline integration:**
```bash
see logfile.txt | grep "ERROR"
```

**Process binary files:**
```bash
see image.jpg > image_copy.jpg
```

## Technical Details

### Architecture

- **Language**: C89 for maximum compiler compatibility
- **Buffer Size**: 64KB optimized for typical disk I/O performance
- **Platform Handling**: Conditional compilation for Windows/POSIX systems
- **Error Handling**: Comprehensive error checking with descriptive messages
- **Signal Management**: SIGPIPE handling for robust pipeline operations

### Platform-Specific Features

**Windows:**
- UTF-8 console output configuration
- Binary mode I/O stream setup
- Windows-specific error handling

**POSIX (Linux/macOS):**
- SIGPIPE signal management
- Standard POSIX I/O operations
- Unix-style error reporting

### Performance Characteristics

- **Memory Usage**: Static 64KB buffer minimizes heap allocation
- **I/O Efficiency**: Optimized read/write operations with partial write handling
- **Error Recovery**: Interrupt-safe operations with automatic retry
- **Resource Management**: Proper file handle cleanup and error propagation

## Building

The project uses a simple Makefile with the following targets:

```bash
make build    # Build the executable
make clean    # Remove build artifacts
```

## Documentation

For comprehensive technical documentation and implementation details, 
visit the [DeepWiki documentation](https://deepwiki.com/itlwas/see).

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) 
file for complete terms and conditions.