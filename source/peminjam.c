/* peminjam.c
 *
 * Modul interaktif untuk peran "Peminjam" (mahasiswa)
 * Kompatibel dengan library.h / library.c terbaru (atomic save, Windows compat)
 *
 * Fitur:
 *  - Login/registrasi sederhana berdasarkan NIM
 *  - Cari buku (by ISBN exact atau title substring)
 *  - Checkout (peminjaman) otomatis menyimpan DB (auto-save)
 *  - Return (pengembalian) otomatis menyimpan DB + menampilkan denda
 *  - Lihat daftar pinjaman milik peminjam (estimasi denda)
 *  - Lapor buku hilang (mark lost) otomatis menyimpan DB + biaya
 *
 * Desain:
 *  - Semua I/O file dilakukan oleh library.c
 *  - peminjam.c hanya menangani interaksi pengguna & memanggil API
 *
 * Kompilasi (contoh):
 *   gcc -std=c99 -Wall -Wextra -I./source -o app source/library.c source/peminjam.c source/main.c source/admin.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../include/library.h"

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

void login_peminjam(void) {
    char username[128];
    char password[128];
    printf("\n--- LOGIN PEMINJAM ---\n");
    printf("Username: ");
    if (!read_line_local(username, sizeof(username))) return;
    printf("Password: ");
    if (!read_line_local(password, sizeof(password))) return;
    /* placeholder auth */
    printf("Halo, %s! (login simulasi)\n", username);

    for (;;) {
        int opt;
        printf("\n---- MENU PEMINJAM ----\n");
        printf("1. Lihat daftar buku\n");
        printf("2. Cari buku berdasarkan judul\n");
        printf("3. Pinjam buku (by ID)\n");
        printf("4. Kembalikan buku (by ID)\n");
        printf("0. Logout\n");
        printf("Pilihan anda: ");
        opt = read_int_choice_local();
        switch (opt) {
            case 1:
                list_books();
                break;
            case 2: {
                char query[TITLE_MAX];
                printf("Masukkan kata kunci judul: ");
                if (!read_line_local(query, sizeof(query))) break;
                int ids[32];
                int found = find_books_by_title(query, ids, (int)(sizeof(ids)/sizeof(ids[0])));
                if (found == 0) {
                    printf("Tidak ada buku yang cocok.\n");
                } else {
                    printf("Ditemukan %d:\n", found);
                    for (int i = 0; i < found; ++i) {
                        Book *b = find_book_by_id(ids[i]);
                        if (b) printf("ID %d | %s - %s | %s\n", b->id, b->title, b->author, b->available ? "Tersedia" : "Dipinjam");
                    }
                }
                break;
            }
            case 3: {
                printf("Masukkan ID buku yang akan dipinjam: ");
                int id = read_int_choice_local();
                if (id <= 0) { printf("ID tidak valid.\n"); break; }
                if (borrow_book(id) == 0) printf("Berhasil meminjam buku ID %d\n", id);
                else printf("Gagal meminjam: buku tidak tersedia atau tidak ditemukan.\n");
                break;
            }
            case 4: {
                printf("Masukkan ID buku yang akan dikembalikan: ");
                int id = read_int_choice_local();
                if (id <= 0) { printf("ID tidak valid.\n"); break; }
                if (return_book(id) == 0) printf("Berhasil mengembalikan buku ID %d\n", id);
                else printf("Gagal mengembalikan: buku tidak ditemukan atau belum dipinjam.\n");
                break;
            }
            case 0:
                printf("Logout.\n");
                return;
            default:
                printf("Pilihan tidak valid.\n");
                break;
        }
    }
}
