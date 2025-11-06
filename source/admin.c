#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../include/library.h"

/* minimal safe getline-like helper */
static char *read_line_local(char *buf, size_t size) {
    if (!buf || size == 0) return NULL;
    if (fgets(buf, (int)size, stdin) == NULL) return NULL;
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    return buf;
}

static int read_int_choice_local(void) {
    char tmp[64];
    if (!read_line_local(tmp, sizeof(tmp))) return -1;
    char *p = tmp;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '\0') return -1;
    return atoi(p);
}

void admin_menu(void) {
    int opt;
    char buf[256];
    for (;;) {
        printf("\n==== ADMIN MENU ====\n");
        printf("1. Lihat daftar buku\n");
        printf("2. Tambah buku\n");
        printf("3. Hapus buku (by ID)\n");
        printf("0. Kembali ke menu utama\n");
        printf("Pilihan anda: ");
        opt = read_int_choice_local();
        switch (opt) {
            case 1:
                list_books();
                break;
            case 2:
                printf("Judul: ");
                if (!read_line_local(buf, sizeof(buf))) break;
                {
                    char author[AUTHOR_MAX];
                    printf("Pengarang: ");
                    if (!read_line_local(author, sizeof(author))) break;
                    int id = add_book(buf, author);
                    if (id > 0) printf("Buku ditambahkan dengan ID %d\n", id);
                    else printf("Gagal menambah buku.\n");
                }
                break;
            case 3:
                printf("Masukkan ID buku yang ingin dihapus: ");
                opt = read_int_choice_local();
                if (opt <= 0) { printf("ID tidak valid.\n"); break; }
                if (remove_book(opt) == 0) printf("Buku (ID %d) dihapus.\n", opt);
                else printf("Buku dengan ID %d tidak ditemukan.\n", opt);
                break;
            case 0:
                return;
            default:
                printf("Pilihan tidak valid.\n");
                break;
        }
    }
}
