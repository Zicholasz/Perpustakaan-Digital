#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "library.h"
#include "view.h"

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

/* UI lifecycle */
int ui_init(void) {
    init_particles();
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
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                    DETAIL BUKU (Demo UI)                         ║\n");
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    if (b) {
        printf("║ ISBN        : %-47s ║\n", b->isbn);
        printf("║ Judul       : %-47s ║\n", b->title);
        printf("║ Penulis     : %-47s ║\n", b->author);
        printf("║ Tahun Terbit: %-47d ║\n", b->year);
        printf("║ Total Stok  : %-47d ║\n", b->total_stock);
        printf("║ Tersedia    : %-47d ║\n", b->available);
        printf("║                                                              ║\n");
        if (b->available < b->total_stock) {
            printf("║ Status      : %d dari %d buku sedang dipinjam                ║\n",
                   b->total_stock - b->available, b->total_stock);
        } else {
            printf("║ Status      : Semua buku tersedia                           ║\n");
        }
        if (b->notes[0] != '\0') {
            printf("║                                                              ║\n");
            printf("║ Catatan     : %-47s ║\n", b->notes);
        }
    } else {
        printf("║                                                              ║\n");
        printf("║              Buku tidak ditemukan                            ║\n");
        printf("║                                                              ║\n");
    }
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
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
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                      DAFTAR BUKU (Demo UI)                       ║\n");
    printf("╠════╤═══════════════╤════════════════════╤═══════════╤══════════╣\n");
    printf("║ No │     ISBN      │       Judul        │  Penulis  │   Stok   ║\n");
    printf("╠════╪═══════════════╪════════════════════╪═══════════╪══════════╣\n");
    printf("║ %s1 │ 9781234567897 │ Demo Book 1       │ Author 1  │    5     ║\n",
           selected_local_index == 0 ? ">" : " ");
    printf("║ %s2 │ 9781234567898 │ Demo Book 2       │ Author 2  │    3     ║\n",
           selected_local_index == 1 ? ">" : " ");
    printf("║ %s3 │ 9781234567899 │ Demo Book 3       │ Author 3  │    7     ║\n",
           selected_local_index == 2 ? ">" : " ");
    printf("╚════╧═══════════════╧════════════════════╧═══════════╧══════════╝\n");
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