#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "library.h"
#include "view.h"
#include "ui.h"

// Initialize book types list
void init_jenis_list() {
    // We're using a more dynamic system now through library.h
}

// Ensure sample data exists with richer data
void ensure_sample_data() {
    library_db_t *db;
    lib_status_t err;
    db = lib_db_open(NULL, &err);
    if (!db || err != LIB_OK) return;

    // Check if we already have data
    if (db->books_count == 0) {
        // Sample books
        book_t sample_books[] = {
            {
                .isbn = "9780141988511",
                .title = "21 Lessons for the 21st Century",
                .author = "Yuval Noah Harari",
                .total_stock = 3,
                .available = 3,
                .year = 2018
            },
            {
                .isbn = "9780545010221",
                .title = "Harry Potter and the Deathly Hallows",
                .author = "J.K. Rowling",
                .total_stock = 5,
                .available = 5,
                .year = 2007
            },
            {
                .isbn = "9780262033848",
                .title = "Introduction to Algorithms",
                .author = "Thomas H. Cormen",
                .total_stock = 4,
                .available = 4,
                .year = 2009
            },
            {
                .isbn = "9780134685991",
                .title = "Clean Architecture",
                .author = "Robert C. Martin",
                .total_stock = 2,
                .available = 2,
                .year = 2017
            }
        };

        // Add all sample books
        for (size_t i = 0; i < sizeof(sample_books) / sizeof(sample_books[0]); i++) {
            lib_add_book(db, &sample_books[i]);
        }
    }

    // Check admin account
    admin_user_t admin_test = {0};
    if (lib_verify_admin("admin", "admin123", &admin_test) != LIB_OK) {
        // Create default admin account if it doesn't exist
        lib_add_admin("admin", "admin123");
    }

    // Always save changes
    lib_db_save(db);
    lib_db_close(db);
}

// Display a header with title
void header_tampilan(const char *title) {
    printf("\n================ %s ================\n", title);
}

// Wait for user to press enter
void press_enter() {
    printf("Press Enter to continue...");
    while (getchar() != '\n');
}

// Helper untuk membaca input dengan aman
char *ui_read_line(char *buf, size_t size) {
    if (!buf || size == 0) return NULL;
    if (fgets(buf, (int)size, stdin) == NULL) return NULL;
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
        buf[--len] = '\0';
    }
    return buf;
}

// Login function for borrower
int login_peminjam() {
    char nim[32];
    printf("=== LOGIN PEMINJAM ===\n");
    printf("Masukkan NIM (atau 'q' untuk kembali)\t: ");
    if (!ui_read_line(nim, sizeof(nim))) return 0;
    if (strcmp(nim, "q") == 0 || strcmp(nim, "Q") == 0) return 0;

    if (lib_validate_nim_format(nim)) {
        printf("Login berhasil dengan NIM: %s\n", nim);
        return 1;
    } else {
        printf("[!] Format NIM tidak valid.\n");
        return 0;
    }
}

