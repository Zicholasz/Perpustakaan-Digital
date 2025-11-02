#include "../src/library.h"
#include <stdio.h>

int main(void) {
    lib_status_t st;
    library_db_t *db = lib_db_open("../data/library_db", &st);
    if (!db) {
        printf("Gagal membuka DB, kode error: %d\n", st);
        return 1;
    }

    printf("DB dibuka.\nJumlah buku: %zu\n", db->books_count);

    book_t b = {0};
    strncpy(b.isbn, "978-602-12345-6", LIB_MAX_ISBN-1);
    strncpy(b.title, "Pemrograman C", LIB_MAX_TITLE-1);
    strncpy(b.author, "Nico K", LIB_MAX_AUTHOR-1);
    b.year = 2025;
    b.total_stock = 3;
    b.available = 3;

    st = lib_add_book(db, &b);
    printf("Tambah buku: %s\n", (st == LIB_OK ? "berhasil" : "gagal"));

    lib_db_save(db);
    lib_db_close(db);

    printf("Selesai.\n");
    return 0;
}
