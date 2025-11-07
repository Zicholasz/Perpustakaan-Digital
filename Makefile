# Makefile cross-platform (Linux/macOS and Windows MinGW/MSYS)
CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -Iinclude

SRC_DIR = source
SRCS = $(SRC_DIR)/library.c $(SRC_DIR)/peminjam.c $(SRC_DIR)/main.c $(SRC_DIR)/admin.c
OBJS = $(SRCS:.c=.o)

DATA_DIR = data

# detect windows
ifeq ($(OS),Windows_NT)
    EXE = .exe
    RM = del /Q
else
    EXE =
    RM = rm -f
endif

TARGET = source/main$(EXE)

.PHONY: all clean run create-admin test

all: $(TARGET)

# ensure data dir exists
$(DATA_DIR):
	@mkdir -p $(DATA_DIR) || if exist $(DATA_DIR) (exit 0) || exit 0

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

# Build a small helper that creates an admin account without touching the main build.
# This compiles `source/create_admin.c` with the library implementation and produces
# `source/create_admin.exe` which can be run to add an admin interactively.
create-admin:
	@echo "Building create-admin helper..."
	$(CC) $(CFLAGS) -o source/create_admin$(EXE) source/create_admin.c source/library.c
	@echo "Built source/create_admin$(EXE). Run it to create an admin."

clean:
	-$(RM) $(TARGET) $(OBJS) source/create_admin$(EXE) source/main$(EXE)

# convenience: build for windows from unix (mingw cross) if CROSS_CC provided
windows:
	@if [ -z "$(CROSS_CC)" ]; then echo "Set CROSS_CC (e.g. x86_64-w64-mingw32-gcc)"; exit 1; fi
	$(CROSS_CC) $(CFLAGS) -o app.exe $(SRCS)

run: $(TARGET)
	./$(TARGET)

# Run automated tests (Windows): uses PowerShell script build-and-test.ps1
.PHONY: test smoke
test: $(TARGET)
	@powershell -NoProfile -ExecutionPolicy Bypass -File build-and-test.ps1

# smoke: quick build + run that immediately exits
smoke: $(TARGET)
	@echo 3 | ./$(TARGET)

# Build and run the small fines test program
.PHONY: test-fines
test-fines:
	@echo "Building fines test..."
	$(CC) $(CFLAGS) -o tests/test_fines$(EXE) tests/test_fines.c source/library.c
	@echo "Running fines test..."
	@./tests/test_fines$(EXE)

# test: run program with scripted input (cross-platform). On Windows we use PowerShell
# to pipe the file; on Unix-like systems we use input redirection.
test: $(TARGET)
ifeq ($(OS),Windows_NT)
	@powershell -NoProfile -Command "Get-Content run_input.txt | .\$(TARGET)"
else
	@./$(TARGET) < run_input.txt
endif