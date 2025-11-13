#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/library.h"
#include "../include/view.h"
#include "../include/animation.h"
#include "../include/ui.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

// Forward declarations
static void init_particles(void);

// Particle struct and global state
#define PARTICLE_COUNT 100

typedef struct {
    int x, y;
    int dx, dy;
    char c;
    int active;
} particle_t;

static particle_t s_particles[PARTICLE_COUNT];

/* Rendering templates (choose unicode or ASCII at runtime) */
static int view_use_ascii = 0;
static const char *T_top;
static const char *T_title;
static const char *T_sep;
static const char *T_bottom;

static const char *L_top;
static const char *L_title;
static const char *L_header1;
static const char *L_header2;
static const char *L_header3;
static const char *L_row;
static const char *L_bottom;

static void detect_console_encoding(void) {
    /* Prefer environment override */
    if (getenv("FORCE_ASCII") != NULL) { view_use_ascii = 1; return; }
#if defined(_WIN32) || defined(_WIN64)
    UINT cp = GetConsoleOutputCP();
    if (cp != 65001) view_use_ascii = 1; else view_use_ascii = 0;
#else
    const char *lang = getenv("LANG");
    if (!lang) { view_use_ascii = 0; return; }
    if (strstr(lang, "UTF-8") || strstr(lang, "utf8")) view_use_ascii = 0; else view_use_ascii = 1;
#endif
}

static void init_templates(void) {
    if (!T_top) {
        /* Unicode templates */
        static const char *u_top = "╔════════════════════════════════════════════════════════════════════════╗\n";
        static const char *u_title = "║                        DETAIL BUKU (Demo UI)                           ║\n";
        static const char *u_sep = "╠════════════════════════════════════════════════════════════════════════╣\n";
        static const char *u_bottom = "╚════════════════════════════════════════════════════════════════════════╝\n";

        static const char *ul_top = "╔════════════════════════════════════════════════════════════════════════╗\n";
        static const char *ul_title = "║                         DAFTAR BUKU (Demo UI)                          ║\n";
        static const char *ul_header1 = "╠════╤═══════════════╤══════════════════════╤═══════════════╤══════════╣\n";
        static const char *ul_header2 = "║ No │     ISBN      │        Judul         │   Penulis     │   Stok   ║\n";
        static const char *ul_header3 = "╠════╪═══════════════╪══════════════════════╪═══════════════╪══════════╣\n";
        static const char *ul_row = "║ %s1 │ 9781234567897 │ Demo Book 1         │ Author 1      │    5     ║\n";
        static const char *ul_bottom = "╚════╧═══════════════╧══════════════════════╧═══════════════╧══════════╝\n";

        /* ASCII templates */
        static const char *a_top = "+====================================================================+\n";
        static const char *a_title = "|                       DETAIL BUKU (Demo UI)                        |\n";
        static const char *a_sep = "+--------------------------------------------------------------------+\n";
        static const char *a_bottom = "+====================================================================+\n";

        static const char *al_top = "+====================================================================+\n";
        static const char *al_title = "|                        DAFTAR BUKU (Demo UI)                       |\n";
        static const char *al_header1 = "+----+---------------+----------------------+---------------+----------+\n";
        static const char *al_header2 = "| No |     ISBN      |         Judul        |    Penulis    |   Stok   |\n";
        static const char *al_header3 = "+----+---------------+----------------------+---------------+----------+\n";
        static const char *al_row = "| %s1 | 9781234567897 | Demo Book 1          | Author 1      |    5     |\n";
        static const char *al_bottom = "+----+---------------+----------------------+---------------+----------+\n";

        if (view_use_ascii) {
            T_top = a_top; T_title = a_title; T_sep = a_sep; T_bottom = a_bottom;
            L_top = al_top; L_title = al_title; L_header1 = al_header1; L_header2 = al_header2; L_header3 = al_header3; L_row = al_row; L_bottom = al_bottom;
        } else {
            T_top = u_top; T_title = u_title; T_sep = u_sep; T_bottom = u_bottom;
            L_top = ul_top; L_title = ul_title; L_header1 = ul_header1; L_header2 = ul_header2; L_header3 = ul_header3; L_row = ul_row; L_bottom = ul_bottom;
        }
    }
}

/* UI lifecycle */
int ui_init(void) {
    detect_console_encoding();
    init_templates();
    init_particles();
    /* Initialize animation (enables VT mode on Windows). First prefer ENV override, otherwise load saved theme. */
    animation_init();
    const char *bg = getenv("LIB_BG");
    if (bg && bg[0] != '\0') {
        int code = atoi(bg);
        if (code > 0) animation_set_bg_color(code);
    } else {
        int saved = ui_load_theme();
        if (saved > 0) animation_set_bg_color(saved);
    }
    return 0;
}

void ui_shutdown(void) {
}

/* Internal: particle system (minimal stub) */
static void init_particles(void) {
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        s_particles[i].active = 0;
    }
}

/* Persistence helpers */
int ui_save_db(const char *filename) {
    // Stub implementation
    (void)filename;
    return 0;
}

int ui_load_db(const char *filename) {
    // Stub implementation
    (void)filename;
    return 0;
}

/* Demo/interactive mainloop */
void ui_demo_mainloop(void) {
    // Stub implementation
}

/* Demo helper: add book to in-memory store */
void ui_add_demo_book(const char *isbn, const char *title, const char *author, 
                     int year, int total_stock, int available) {
    // Stub implementation
    (void)isbn; (void)title; (void)author;
    (void)year; (void)total_stock; (void)available;
}

/* Book-detail display */
void ui_show_book_detail(const book_t *b) {
    /* Use selected templates for top/title/separator */
    printf("%s", T_top);
    printf("%s", T_title);
    printf("%s", T_sep);
    if (b) {
        if (view_use_ascii) {
            printf("| ISBN         : %-60s |\n", b->isbn);
            printf("| Judul        : %-60s |\n", b->title);
            printf("| Penulis      : %-60s |\n", b->author);
            printf("| Tahun Terbit : %-60d |\n", b->year);
            printf("| Total Stok   : %-60d |\n", b->total_stock);
            printf("| Tersedia     : %-60d |\n", b->available);
            printf("|                                                                          |\n");
            if (b->available < b->total_stock)
                printf("| Status       : %d dari %d buku sedang dipinjam                            |\n", b->total_stock - b->available, b->total_stock);
            else
                printf("| Status       : Semua buku tersedia                                       |\n");
            if (b->notes[0] != '\0') {
                printf("|                                                                          |\n");
                printf("| Catatan      : %-60s |\n", b->notes);
            }
        } else {
            printf("║ ISBN         : %-60s ║\n", b->isbn);
            printf("║ Judul        : %-60s ║\n", b->title);
            printf("║ Penulis      : %-60s ║\n", b->author);
            printf("║ Tahun Terbit : %-60d ║\n", b->year);
            printf("║ Total Stok   : %-60d ║\n", b->total_stock);
            printf("║ Tersedia     : %-60d ║\n", b->available);
            printf("║                                                                          ║\n");
            if (b->available < b->total_stock)
                printf("║ Status       : %d dari %d buku sedang dipinjam                            ║\n", b->total_stock - b->available, b->total_stock);
            else
                printf("║ Status       : Semua buku tersedia                                       ║\n");
            if (b->notes[0] != '\0') {
                printf("║                                                                          ║\n");
                printf("║ Catatan      : %-60s ║\n", b->notes);
            }
        }
    } else {
        if (view_use_ascii) {
            printf("|                                                              |\n");
            printf("|              Buku tidak ditemukan                            |\n");
            printf("|                                                              |\n");
        } else {
            printf("║                                                              ║\n");
            printf("║              Buku tidak ditemukan                            ║\n");
            printf("║                                                              ║\n");
        }
    }
    printf("%s", T_bottom);
}

/* Input with autocomplete */
int ui_input_autocomplete(char *buffer, size_t bufsize) {
    // Basic implementation without autocomplete
    if (!buffer || bufsize == 0) return -1;
    if (fgets(buffer, (int)(bufsize), stdin) == NULL) return -1;
    buffer[strcspn(buffer, "\n")] = 0;
    return 0;
}

/* Book list render */
void ui_render_book_list(int top, int left, int width, int height,
                        const char *query, int page, int selected_local_index) {
    printf("%s", L_top);
    printf("%s", L_title);
    printf("%s", L_header1);
    printf("%s", L_header2);
    printf("%s", L_header3);
    printf(L_row, selected_local_index == 0 ? ">" : " ");
    printf(L_row, selected_local_index == 1 ? ">" : " ");
    printf(L_row, selected_local_index == 2 ? ">" : " ");
    printf("%s", L_bottom);
    printf("\nQuery: %s | Page: %d/%d | Selected: %d\n", 
           query ? query : "-", page, 3, selected_local_index + 1);
}

/* Book actions */
int ui_borrow_book_by_index(int global_index) {
    // Stub implementation
    (void)global_index;
    return -1;
}

int ui_return_book_by_index(int global_index) {
    // Stub implementation
    (void)global_index;
    return -1;
}

/* History display */
void ui_show_history_timeline(void) {
    // Stub implementation
    printf("[UI] History timeline would be shown here.\n");
}