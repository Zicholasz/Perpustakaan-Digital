/* animation.h
   Cross-platform terminal animations for "Perpustakaan Digital" demo.
   Adds a modern, library-themed animation module without changing existing program files.
*/
#ifndef ANIMATION_H
#define ANIMATION_H

#include <stdio.h>

/* Initialize terminal for ANSI sequences (Windows enables VT processing). */
void animation_init(void);

/* Simple animations (blocking) - call from a demo or before your UI shows): */
void animation_typewriter(const char *text, int ms_per_char);
void animation_book_opening(int repeats);
void animation_bookshelf_scan(int width);
void animation_loading_bar(int duration_ms);
void animation_confetti(int count);
/* Set background color using 256-color ANSI code (0 = reset). */
void animation_set_bg_color(int color_code);

/* Small blocking delay in milliseconds (cross-platform helper). */
void animation_delay(int ms);

/* Demo runner that cycles a few animations */
int animation_run_demo(void);

#endif /* ANIMATION_H */
