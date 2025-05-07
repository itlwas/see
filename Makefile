.PHONY: all clean
CC = gcc
CFLAGS = -std=c89 -Wall -Wextra -O3
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
