# Makefile cross-platform (Linux/macOS and Windows MinGW/MSYS)
CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -I./source

SRC_DIR = source
SRCS = $(SRC_DIR)/library.c $(SRC_DIR)/peminjam.c $(SRC_DIR)/main.c $(SRC_DIR)/admin.c
OBJS = $(SRCS:.c=.o)

# detect windows
ifeq ($(OS),Windows_NT)
    EXE = .exe
    RM = del /Q
else
    EXE =
    RM = rm -f
endif

TARGET = app$(EXE)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

clean:
	-$(RM) $(TARGET) $(OBJS)

# convenience: build for windows from unix (mingw cross) if CROSS_CC provided
windows:
	@if [ -z "$(CROSS_CC)" ]; then echo "Set CROSS_CC (e.g. x86_64-w64-mingw32-gcc)"; exit 1; fi
	$(CROSS_CC) $(CFLAGS) -o app.exe $(SRCS)

run: $(TARGET)
	./$(TARGET)
