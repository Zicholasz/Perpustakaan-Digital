/* library.h
 * Header untuk Sistem Perpustakaan (modular C)
 * - Pastikan file disimpan UTF-8 tanpa BOM
 * - Include guard dan urutan deklarasi dibuat benar agar tidak timbul error "identifier is undefined"
 *
 * Standard: ISO C99
 */

#ifndef PERPUSTAKAAN_LIBRARY_H
#define PERPUSTAKAAN_LIBRARY_H

/* --- Standard includes --- */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>     /* size_t */
#include <stdint.h>
#include <stdbool.h>
#include <time.h>       /* time_t */
#include <string.h>

/* -------------------------
   Konfigurasi / Constants
   ------------------------- */

/* Maksimum panjang string (aman untuk buffer) */
#define LIB_MAX_TITLE       128
#define LIB_MAX_AUTHOR      64
#define LIB_MAX_ISBN        32
#define LIB_MAX_NAME        64
#define LIB_MAX_NOTES       256

/* File default untuk penyimpanan database (bisa diubah pada runtime).
   Use a `data/` folder by default so files are grouped and permission issues
   are less likely when running from the project root. */
#define LIB_DEFAULT_DB_FILE "data/library_db"

/* Batas jumlah entri default (dapat diresize dinamis) */
#define LIB_DEFAULT_CAPACITY 256

/* Maksimal jenis buku (default dan hard limit) */
#define LIB_MAX_BOOK_TYPES 1024U
#define LIB_MAX_BOOK_TYPES_HARD_LIMIT 65536U

/* Kebijakan denda: nilai default (bisa diubah) */
#define LIB_DEFAULT_FINE_PER_DAY 1000 /* contoh: Rupiah per hari */

/* Format tanggal (internal) -- gunakan fungsi pembantu untuk parsing/format */
#define LIB_DATE_FORMAT "%Y-%m-%d" /* ISO-like: 2025-10-31 */

/* Return codes */
typedef enum {
    LIB_OK = 0,
    LIB_ERR_IO = -1,
    LIB_ERR_NOT_FOUND = -2,
    LIB_ERR_EXISTS = -3,
    LIB_ERR_NO_STOCK = -4,
    LIB_ERR_INVALID_ARG = -5,
    LIB_ERR_MEMORY = -6,
    LIB_ERR_AUTH = -7,
    LIB_ERR_MAX_TYPES = -8
} lib_status_t;

/* -------------------------
   Tipe tanggal sederhana
   ------------------------- */
typedef struct {
    int year;
    int month; /* 1-12 */
    int day;   /* 1-31 */
} lib_date_t;

/* Utility: convert time_t <-> lib_date_t */
lib_date_t lib_date_from_time_t(time_t t);
time_t lib_time_t_from_date(lib_date_t d);
int lib_date_days_between(lib_date_t a, lib_date_t b); /* b - a in days */

/* -------------------------
   Entitas data
   ------------------------- */

typedef enum {
    BOOK_AVAILABLE = 0,
    BOOK_BORROWED,
    BOOK_LOST,
    BOOK_RESERVED
} book_status_t;

typedef struct {
    char isbn[LIB_MAX_ISBN];
    char title[LIB_MAX_TITLE];
    char author[LIB_MAX_AUTHOR];
    int year;           /* tahun terbit */
    int total_stock;    /* total kepunyaan perpustakaan */
    int available;      /* stok tersedia */
    char notes[LIB_MAX_NOTES];
} book_t;

/* Borrower: tambahan field 'nim' untuk mahasiswa */
typedef struct {
    char id[32];          /* unik internal (autogen) */
    char nim[32];         /* NIM mahasiswa (kosong jika bukan) */
    char name[LIB_MAX_NAME];
    char phone[32];
    char email[64];
} borrower_t;

typedef struct {
    char loan_id[32];     /* unik */
    char isbn[LIB_MAX_ISBN];
    char borrower_id[32];
    lib_date_t date_borrow;
    lib_date_t date_due;
    lib_date_t date_returned;
    bool is_returned;
    bool is_lost;
    long fine_paid;
} loan_t;

/* -------------------------
   Database container
   ------------------------- */

typedef struct {
    book_t *books;
    size_t books_count;
    size_t books_capacity;

    borrower_t *borrowers;
    size_t borrowers_count;
    size_t borrowers_capacity;

    loan_t *loans;
    size_t loans_count;
    size_t loans_capacity;

    long fine_per_day; /* policy */

    size_t max_book_types; /* runtime limit */

    char *db_file_path;
} library_db_t;

/* -------------------------
   API: DB management
   ------------------------- */

library_db_t *lib_db_open(const char *path, lib_status_t *err);
lib_status_t lib_db_save(library_db_t *db);
lib_status_t lib_db_close(library_db_t *db);

/* set/get max book types */
lib_status_t lib_set_max_book_types(library_db_t *db, size_t max);
size_t lib_get_max_book_types(const library_db_t *db);

/* -------------------------
   Book API
   ------------------------- */

lib_status_t lib_add_book(library_db_t *db, const book_t *book);
lib_status_t lib_remove_book(library_db_t *db, const char *isbn);
const book_t *lib_find_book_by_isbn(const library_db_t *db, const char *isbn);
size_t lib_search_books_by_title(const library_db_t *db,
                                 const char *title_substr,
                                 const book_t **out,
                                 size_t out_capacity);
lib_status_t lib_update_book_stock(library_db_t *db, const char *isbn, int delta);

/* -------------------------
   Borrower API (NIM support)
   ------------------------- */

lib_status_t lib_add_borrower(library_db_t *db, const borrower_t *b);
const borrower_t *lib_find_borrower_by_id(const library_db_t *db, const char *id);
const borrower_t *lib_find_borrower_by_nim(const library_db_t *db, const char *nim);
/* get or create: returns pointer to internal borrower (modifiable if not const) */
borrower_t *lib_get_or_create_borrower_by_nim(library_db_t *db, const char *nim, bool create_if_missing);
bool lib_validate_nim_format(const char *nim);

/* -------------------------
   Loan API
   ------------------------- */

lib_status_t lib_checkout_book(library_db_t *db,
                               const char *isbn,
                               const borrower_t *borrower,
                               lib_date_t date_borrow,
                               lib_date_t date_due,
                               char out_loan_id[32]);
lib_status_t lib_return_book(library_db_t *db,
                             const char *loan_id,
                             lib_date_t date_return,
                             unsigned long *out_fine);
lib_status_t lib_mark_book_lost(library_db_t *db,
                                const char *loan_id,
                                unsigned long *out_cost);
size_t lib_find_loans_by_borrower(const library_db_t *db,
                                  const char *borrower_id_or_name,
                                  loan_t **out, size_t out_capacity);

/* -------------------------
   Fine helper
   ------------------------- */
unsigned long lib_calculate_fine(const library_db_t *db,
                                 lib_date_t due_date,
                                 lib_date_t return_date);

/* -------------------------
   Admin / Auth helpers
   ------------------------- */

typedef struct {
    char username[32];
    char password_hash[64];
    bool is_super;
} admin_user_t;

lib_status_t lib_add_admin(const char *username, const char *password_plain);
lib_status_t lib_verify_admin(const char *username, const char *password_plain, admin_user_t *out);

/* -------------------------
   Persistence helpers
   ------------------------- */
lib_status_t lib_db_export_csv(const library_db_t *db, const char *path);
lib_status_t lib_db_import_csv(library_db_t *db, const char *path);

/* Compatibility / convenience initialiser used by older main.c */
lib_status_t lib_db_init(library_db_t *db);

/* Mutable find and delete aliases (backwards compatibility) */
book_t *lib_find_book_by_isbn_mutable(library_db_t *db, const char *isbn);
lib_status_t lib_delete_book(library_db_t *db, const char *isbn);

/* -------------------------
   Utility / debug
   ------------------------- */
void lib_print_book(const book_t *b, FILE *fp);
void lib_print_borrower(const borrower_t *br, FILE *fp);
void lib_print_loan(const loan_t *l, FILE *fp);

/* convenience alloc/free */
book_t *lib_book_create_empty(void);
void lib_book_free(book_t *b);
borrower_t *lib_borrower_create_empty(void);
void lib_borrower_free(borrower_t *b);
loan_t *lib_loan_create_empty(void);
void lib_loan_free(loan_t *l);

/* library core */
void library_init(void);

#endif /* PERPUSTAKAAN_LIBRARY_H */
