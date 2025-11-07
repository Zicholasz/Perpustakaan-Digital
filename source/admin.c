#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "library.h"
#include "ui.h"

const char *usernamekey = "user123";
const char *passwordkey = "pass123";

/* Helper untuk membaca input */
static char *read_line_local(char *buf, size_t size) {
    if (!buf || size == 0) return NULL;
    if (fgets(buf, (int)size, stdin) == NULL) return NULL;
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    return buf;
}

/* Helper untuk membaca pilihan angka */
static int read_int_choice_local(void) {
    char tmp[64];
    if (!read_line_local(tmp, sizeof(tmp))) return -1;
    char *p = tmp;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '\0') return -1;
    return atoi(p);
}

/* Login admin */
int login_admin(void) {
    char username[32], password[32];
    admin_user_t admin = {0};
    
    printf("=== LOGIN ADMIN ===\n");
    printf("Username\t: Kakak Admin");
    if (!read_line_local(username, sizeof(username))) return 0;
    
    printf("Password\t: 1234");
    if (!read_line_local(password, sizeof(password))) return 0;
    
    if (lib_verify_admin(username, password, &admin) == LIB_OK) {
        printf("Login berhasil sebagai: %s\n", admin.username);
        return 1;
    }
    return 0;
}

/* Menu admin */
void menu_admin(library_db_t *db) {
    if (!db) {
        printf("[!] Database belum diinisialisasi.\n");
        return;
    }

    int running = 1;
    while (running) {
        printf("\n==== ADMIN MENU ====\n");
        printf("1. Lihat daftar buku\n");
        printf("2. Tambah buku\n");
        printf("3. Hapus buku (by ISBN)\n");
        printf("4. Update stok buku\n");
        printf("0. Kembali ke menu utama\n");
        printf("Pilihan anda: ");

        int opt = read_int_choice_local();
        char buf[256];

        switch (opt) {
            case 1: {
                printf("\n=== DAFTAR BUKU ===\n");
                printf("%-15s %-30s %-20s %-5s %-5s\n", 
                       "ISBN", "Judul", "Penulis", "Total", "Sedia");
                printf("------------------------------------------------------------------------\n");
                for (size_t i = 0; i < db->books_count; i++) {
                    const book_t *b = &db->books[i];
                    printf("%-15s %-30s %-20s %-5d %-5d\n",
                           b->isbn, b->title, b->author, 
                           b->total_stock, b->available);
                }
                break;
            }
            case 2: {
                book_t new_book = {0};
                printf("ISBN\t: ");
                if (!read_line_local(buf, sizeof(buf))) break;
                strncpy(new_book.isbn, buf, LIB_MAX_ISBN-1);
                
                printf("Judul\t: ");
                if (!read_line_local(buf, sizeof(buf))) break;
                strncpy(new_book.title, buf, LIB_MAX_TITLE-1);
                
                printf("Pengarang\t: ");
                if (!read_line_local(buf, sizeof(buf))) break;
                strncpy(new_book.author, buf, LIB_MAX_AUTHOR-1);
                
                printf("Tahun terbit\t: ");
                if (!read_line_local(buf, sizeof(buf))) break;
                new_book.year = atoi(buf);
                
                printf("Jumlah stok\t: ");
                if (!read_line_local(buf, sizeof(buf))) break;
                new_book.total_stock = atoi(buf);
                new_book.available = new_book.total_stock;
                
                printf("Catatan (opsional)\t: ");
                if (!read_line_local(buf, sizeof(buf))) break;
                strncpy(new_book.notes, buf, LIB_MAX_NOTES-1);

                lib_status_t st = lib_add_book(db, &new_book);
                if (st == LIB_OK) {
                    printf("Buku berhasil ditambahkan!\n");
                    lib_db_save(db);
                } else {
                    printf("[!] Gagal menambah buku (kode: %d)\n", (int)st);
                }
                break;
            }
            case 3: {
                printf("Masukkan ISBN buku yang ingin dihapus\t: ");
                if (!read_line_local(buf, sizeof(buf))) break;

                lib_status_t st = lib_remove_book(db, buf);
                if (st == LIB_OK) {
                    printf("Buku berhasil dihapus.\n");
                    lib_db_save(db);
                } else {
                    printf("[!] Gagal menghapus buku (kode: %d)\n", (int)st);
                }
                break;
            }
            case 4: {
                printf("Masukkan ISBN buku\t: ");
                if (!read_line_local(buf, sizeof(buf))) break;

                const book_t *b = lib_find_book_by_isbn(db, buf);
                if (!b) {
                    printf("[!] Buku tidak ditemukan.\n");
                    break;
                }

                printf("\nBuku: %s\n", b->title);
                printf("Stok saat ini: %d (tersedia: %d)\n", 
                       b->total_stock, b->available);
                printf("Perubahan stok (+/-): ");
                if (!read_line_local(buf, sizeof(buf))) break;
                int delta = atoi(buf);

                lib_status_t st = lib_update_book_stock(db, b->isbn, delta);
                if (st == LIB_OK) {
                    printf("Stok berhasil diupdate.\n");
                    lib_db_save(db);
                } else {
                    printf("[!] Gagal mengupdate stok (kode: %d)\n", (int)st);
                }
                break;
            }
            case 5: {
                printf("Masukkan judul buku untuk dicari\t: ");
                if (!read_line_local(buf, sizeof(buf))) break;

                const book_t *results[64];
                size_t found = lib_search_books_by_title(db, buf, results, 64);
                if (found > 0) {
                    printf("\nDitemukan %zu buku:\n", found);
                    for (size_t i = 0; i < found; i++) {
                        printf("\n=== Buku #%zu ===\n", i+1);
                        lib_print_book(results[i], stdout);
                    }
                } else {
                    printf("[!] Tidak ditemukan buku yang cocok.\n");
                }
                break;
            }
            case 6: {
                printf("\n=== HISTORY PEMINJAMAN ===\n");
                printf("%-10s %-15s %-20s %-12s %-12s %-12s %s\n",
                       "ID", "ISBN", "Peminjam", "Tgl Pinjam", "Jatuh Tempo", "Kembali", "Status");
                printf("--------------------------------------------------------------------------------\n");
                
                for (size_t i = 0; i < db->loans_count; i++) {
                    const loan_t *l = &db->loans[i];
                    const book_t *b = lib_find_book_by_isbn(db, l->isbn);
                    const borrower_t *br = lib_find_borrower_by_id(db, l->borrower_id);

                    char date_borrow[16], date_due[16], date_return[16];
                    snprintf(date_borrow, sizeof(date_borrow), "%04d-%02d-%02d", 
                            l->date_borrow.year, l->date_borrow.month, l->date_borrow.day);
                    snprintf(date_due, sizeof(date_due), "%04d-%02d-%02d",
                            l->date_due.year, l->date_due.month, l->date_due.day);
                    if (l->is_returned) {
                        snprintf(date_return, sizeof(date_return), "%04d-%02d-%02d",
                                l->date_returned.year, l->date_returned.month, l->date_returned.day);
                    } else {
                        strcpy(date_return, "-");
                    }

                    printf("%-10s %-15s %-20s %-12s %-12s %-12s ", 
                           l->loan_id,
                           l->isbn,
                           br ? br->name : "???",
                           date_borrow,
                           date_due,
                           date_return);

                    if (l->is_lost) printf("HILANG\n");
                    else if (l->is_returned) printf("Kembali\n");
                    else printf("Dipinjam\n");
                }
                break;
            }
            case 0:
                running = 0;
                break;
            default:
                printf("[!] Pilihan tidak valid.\n");
                break;
        }

        if (running) {
            printf("\nTekan Enter untuk melanjutkan...");
            while (getchar() != '\n');
        }
    }
    return;
}
