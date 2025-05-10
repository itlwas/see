.PHONY: build clean
CC = gcc
CFLAGS = -std=c89 -D_FILE_OFFSET_BITS=64 -Wall -Wextra -O3 -s
OUT = see$(if $(filter Windows_NT,$(OS)),.exe,)
SRC = src/see.c
build: $(OUT)
$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<
clean:
	rm -f $(OUT)
