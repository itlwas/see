.PHONY: build clean

# Toolchain
CC     = gcc
CFLAGS = -std=c89 -D_FILE_OFFSET_BITS=64 -Wall -Wextra -pipe -Os -s

# Files
OUT = see$(if $(filter Windows_NT,$(OS)),.exe,)
SRC = src/see.c

# Targets
build: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(OUT)
