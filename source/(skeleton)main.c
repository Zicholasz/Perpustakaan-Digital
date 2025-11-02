#include <stdio.h>
#include "library.h"

int main(void) {
    lib_status_t st;
    library_db_t *db = lib_db_open("library_db", &st);
    if (!db) { fprintf(stderr, "Gagal buka db\n"); return 1; }

    /* contoh main menu */
    int choice = 0;
    while (1) {
        printf("1. Admin\n2. Peminjam\n3. Simpan & Keluar\n> ");
        if (scanf("%d", &choice)!=1) break;
        if (choice == 1) {
            /* panggil admin menu di admin.c (implementasikan ada fungsi admin_menu(db)) */
            /* admin_menu(db); */
        } else if (choice == 2) {
            /* panggil peminjam menu */
        } else break;
    }

    lib_db_save(db);
    lib_db_close(db);
    return 0;
}
