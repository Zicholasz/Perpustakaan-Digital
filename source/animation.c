/* animation.c
   Implementation of simple terminal animations for a library-themed demo.
*/

#include "../include/animation.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef DISABLE_NEWLINE_AUTO_RETURN
#define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#endif
#else
#include <unistd.h>
#endif

static int g_vt_enabled = 0;

void animation_init(void) {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
    if (SetConsoleMode(hOut, dwMode)) g_vt_enabled = 1;
#else
    /* Most POSIX terminals support ANSI by default */
    g_vt_enabled = 1;
#endif
}

static void msleep(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
}

/* Public wrapper for a small blocking delay (ms). */
void animation_delay(int ms) {
    if (ms <= 0) return;
    msleep(ms);
}

void animation_typewriter(const char *text, int ms_per_char) {
    if (!text) return;
    for (size_t i = 0; i < strlen(text); ++i) {
        putchar(text[i]);
        fflush(stdout);
        msleep(ms_per_char);
    }
    putchar('\n');
    /* ensure attributes reset after typing */
    printf("\x1b[0m");
    fflush(stdout);
}

void animation_book_opening(int repeats) {
    const char *frame1 = "  ____________\n /           /|\n/___________/ |\n|  __  __  |  |\n| |  ||  | |  |\n|_|__||__|_|  |\n  (Library)   |\n  -----------/\n";
    const char *frame2 = "   ____\n  / ___| ___  _ __ ___\n | |  _ / _ \\| '_ ` _ \\ \n | |_| | (_) | | | | | |\n  \\____|\\___/|_| |_| |_|\n   (Library)\n";

    for (int r = 0; r < repeats; ++r) {
        printf("\x1b[2J\x1b[H");
        printf("%s", frame1);
        fflush(stdout);
        msleep(500);
        printf("\x1b[2J\x1b[H");
        printf("%s", frame2);
        fflush(stdout);
        msleep(500);
    }
}

void animation_bookshelf_scan(int width) {
    if (width < 10) width = 10;
    int pos = 0;
    int dir = 1;
    for (int t = 0; t < width * 2; ++t) {
        printf("\r");
        for (int i = 0; i < width; ++i) {
            /* Using ASCII-friendly glyphs for wide compatibility */
            if (i == pos) printf("\x1b[48;5;238m\x1b[38;5;231m [BOOK] \x1b[0m");
            else printf(" [ bk ] ");
        }
        fflush(stdout);
        msleep(80);
        pos += dir;
        if (pos == width-1 || pos == 0) dir = -dir;
    }
    printf("\n");
}

void animation_loading_bar(int duration_ms) {
    const int steps = 40;
    int step_ms = (duration_ms > 0) ? (duration_ms / steps) : 25;
    printf("[");
    for (int i = 0; i < steps; ++i) {
        printf("#");
        fflush(stdout);
        msleep(step_ms);
    }
    printf("] Done\n");
    /* Reset attributes to avoid leaking colors */
    printf("\x1b[0m");
    fflush(stdout);
}

void animation_confetti(int count) {
    if (count <= 0) count = 20;
    srand((unsigned)time(NULL));
    for (int i = 0; i < count; ++i) {
        int x = rand() % 60;
        int y = rand() % 10;
        int color = 31 + (rand() % 6);
        printf("\x1b[%d;%dH\x1b[%dm*\x1b[0m", y+2, x+2, color);
        fflush(stdout);
        msleep(60);
    }
    /* Reset attributes and move cursor below the confetti area */
    printf("\x1b[0m\x1b[12;1H");
}

int animation_run_demo(void) {
    animation_init();
    printf("\x1b[2J\x1b[H");
    animation_typewriter("Selamat datang di Perpustakaan Digital", 40);
    msleep(300);
    animation_book_opening(2);
    animation_bookshelf_scan(12);
    animation_loading_bar(1500);
    animation_confetti(40);
    printf("\n");
    return 0;
}

void animation_set_bg_color(int color_code) {
    if (color_code <= 0) {
        printf("\x1b[0m"); /* reset all */
        fflush(stdout);
        return;
    }
    /* 48;5;{n} sets background in 256-color mode */
    /* set background, clear the screen and move cursor home so the whole
       console repaints with the new background for subsequent text output */
    printf("\x1b[48;5;%dm\x1b[2J\x1b[H", color_code % 256);
    fflush(stdout);
}
