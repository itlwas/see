.PHONY: all clean
CC = gcc
CFLAGS = -std=c89 -D_FILE_OFFSET_BITS=64 -Wall -Wextra -O3 -s
ifeq ($(OS),Windows_NT)
	TARGET=see.exe
else
	TARGET=see
endif
SRC = src/see.c
all: $(TARGET)
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<
clean:
	rm -f $(TARGET)
