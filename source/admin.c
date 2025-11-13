#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../include/library.h"
#include "../include/ui.h"
#include "../include/animation.h"
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

/* trim leading and trailing whitespace in-place */
static void trim_spaces(char *s) {
    if (!s) return;
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len-1])) s[--len] = '\0';
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
    printf("Username\t: ");
    if (!read_line_local(username, sizeof(username))) return 0;
    
    printf("Password\t: ");
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

    static int first_enter = 1;
    if (first_enter) {
        animation_init();
        animation_typewriter("Selamat datang, Admin. Memuat menu...", 30);
        first_enter = 0;
    }

    int running = 1;
    while (running) {
        printf("\n==== ADMIN MENU ====\n");
        printf("1. Lihat daftar buku\n");
        printf("2. Tambah buku\n");
        printf("3. Hapus buku (by ISBN)\n");
        printf("4. Update stok buku\n");
    printf("5. Cari buku (judul)\n");
    printf("6. Lihat history peminjaman\n");
    printf("7. Tandai pinjaman terlambat sebagai HILANG (by Loan ID)\n");
    printf("8. Pengaturan (ubah denda / kebijakan penggantian)\n");
        printf("0. Kembali ke menu utama\n");
        printf("Pilihan anda: ");

        int opt = read_int_choice_local();
        char buf[256];

        switch (opt) {
            case 1: {
                animation_typewriter("[Admin] Memuat daftar buku...", 25);
                animation_delay(300);
                printf("\n=== DAFTAR BUKU ===\n");
                /* small shelf scan animation before listing */
                animation_bookshelf_scan((int)(db->books_count > 12 ? 12 : db->books_count));
                printf("\n%-15s | %-28s | %-18s | %-10s | %-6s | %s\n",
                       "ISBN", "Judul", "Penulis", "Harga", "Total", "Tersedia");
                printf("%-15s-+-%-28s-+-%-18s-+-%-10s-+-%-6s-+-%s\n",
                       "===============", 
                       "============================", 
                       "==================",
                       "==========",
                       "======",
                       "=========");
                for (size_t i = 0; i < db->books_count; i++) {
                    const book_t *b = &db->books[i];
                    printf("%-15s | %-28s | %-18s | Rp%8.2f | %-6d | %d\n",
                           b->isbn, 
                           (strlen(b->title) > 28) ? "..." : b->title, 
                           (strlen(b->author) > 18) ? "..." : b->author,
                           b->price, b->total_stock, b->available);
                }
                printf("\n");
                break;
            }
            case 2: {
                animation_typewriter("[Admin] Tambah buku baru...", 25);
                animation_delay(300);
                book_t new_book = {0};
                printf("\n--- Masukkan Data Buku ---\n\n");
                printf("ISBN                  : ");
                if (!read_line_local(buf, sizeof(buf))) break;
                trim_spaces(buf);
                strncpy(new_book.isbn, buf, LIB_MAX_ISBN-1);
                /* Early duplicate check: if ISBN exists, offer update instead */
                {
                    const book_t *existing = lib_find_book_by_isbn(db, new_book.isbn);
                    if (existing) {
                        printf("\n[!] Buku dengan ISBN %s sudah ada: %s (penulis: %s)\n", existing->isbn, existing->title, existing->author);
                        printf("Ingin memperbarui stok/harga buku ini? [Y/N] : ");
                        if (!read_line_local(buf, sizeof(buf))) break;
                        if (buf[0] == 'y' || buf[0] == 'Y') {
                            printf("Masukkan jumlah stok tambahan (negatif untuk kurangi) : ");
                            if (!read_line_local(buf, sizeof(buf))) break;
                            int delta = atoi(buf);
                            lib_status_t s2 = lib_update_book_stock(db, existing->isbn, delta);
                            if (s2 == LIB_OK) { printf("Stok diperbarui.\n"); lib_db_save(db); animation_loading_bar(300); }
                            else printf("[!] Gagal memperbarui stok (kode: %d)\n", (int)s2);
                            printf("Ingin mengubah harga? [Y/N] : ");
                            if (!read_line_local(buf, sizeof(buf))) break;
                            if (buf[0] == 'y' || buf[0] == 'Y') {
                                printf("Masukkan harga baru      : "); if (!read_line_local(buf, sizeof(buf))) break;
                                book_t *bm = lib_find_book_by_isbn_mutable(db, existing->isbn);
                                if (bm) { bm->price = atof(buf); lib_db_save(db); printf("Harga diubah.\n"); }
                            }
                        }
                        break; /* done with add flow */
                    }
                }
                
                printf("Judul                 : ");
                if (!read_line_local(buf, sizeof(buf))) break;
                strncpy(new_book.title, buf, LIB_MAX_TITLE-1);
                
                printf("Pengarang             : ");
                if (!read_line_local(buf, sizeof(buf))) break;
                strncpy(new_book.author, buf, LIB_MAX_AUTHOR-1);
                
                printf("Tahun Terbit          : ");
                if (!read_line_local(buf, sizeof(buf))) break;
                new_book.year = atoi(buf);
                
                printf("Jumlah Stok           : ");
                if (!read_line_local(buf, sizeof(buf))) break;
                new_book.total_stock = atoi(buf);
                new_book.available = new_book.total_stock;
                printf("Harga Buku (cth: 15000): ");
                if (!read_line_local(buf, sizeof(buf))) break;
                new_book.price = atof(buf);
                
                printf("Catatan (opsional)    : ");
                if (!read_line_local(buf, sizeof(buf))) break;
                strncpy(new_book.notes, buf, LIB_MAX_NOTES-1);

                lib_status_t st = lib_add_book(db, &new_book);
                if (st == LIB_OK) {
                    printf("Buku berhasil ditambahkan!\n");
                    lib_db_save(db);
                    animation_loading_bar(400);
                } else {
                    printf("[!] Gagal menambah buku (kode: %d)\n", (int)st);
                }
                break;
            }
            case 3: {
                animation_typewriter("[Admin] Hapus buku dari sistem...", 25);
                animation_delay(300);
                printf("Masukkan ISBN buku yang ingin dihapus\t: ");
                if (!read_line_local(buf, sizeof(buf))) break;
                trim_spaces(buf);

                const book_t *b = lib_find_book_by_isbn(db, buf);
                if (!b) {
                    printf("[!] Buku tidak ditemukan.\n");
                    break;
                }
                /* Show preview / demo of the book to be deleted */
                printf("\n=== Preview Buku (akan dihapus) ===\n");
                lib_print_book(b, stdout);
                printf("Apakah Anda yakin ingin menghapus buku ini? (y/N): ");
                if (!read_line_local(buf, sizeof(buf))) break;
                if (buf[0] == 'y' || buf[0] == 'Y') {
                    lib_status_t st = lib_remove_book(db, b->isbn);
                    if (st == LIB_OK) {
                        printf("Buku berhasil dihapus.\n");
                        lib_db_save(db);
                        animation_loading_bar(300);
                    } else {
                        printf("[!] Gagal menghapus buku (kode: %d)\n", (int)st);
                    }
                } else {
                    printf("Pembatalan: buku tidak dihapus.\n");
                }
                break;
            }
            case 4: {
                animation_typewriter("[Admin] Update stok buku...", 25);
                animation_delay(300);
                printf("Masukkan ISBN buku\t: ");
                if (!read_line_local(buf, sizeof(buf))) break;
                trim_spaces(buf);

                const book_t *b = lib_find_book_by_isbn(db, buf);
                if (!b) {
                    printf("[!] Buku tidak ditemukan.\n");
                    break;
                }

                printf("\n=== Update Stok Buku ===\n");
                printf("Judul Buku   : %s\n", b->title);
                printf("Total stok   : %d\n", b->total_stock);
                printf("Tersedia     : %d\n", b->available);
                printf("Dipinjam     : %d\n", b->total_stock - b->available);
                printf("\n1. Tambah stok\n");
                printf("2. Kurangi stok\n");
                printf("0. Batal\n");
                printf("\nPilihan: ");
                if (!read_line_local(buf, sizeof(buf))) break;
                int choice = atoi(buf);
                
                int delta = 0;
                lib_status_t st = LIB_OK;
                if (choice == 1 || choice == 2) {
                    printf("\nJumlah stok yang akan %s: ", choice == 1 ? "ditambahkan" : "dikurangi");
                    if (!read_line_local(buf, sizeof(buf))) break;
                    int amount = atoi(buf);
                    if (amount <= 0) {
                        printf("[!] Jumlah harus lebih dari 0\n");
                        break;
                    }
                    delta = choice == 1 ? amount : -amount;
                    
                    if (choice == 2 && (-delta > b->available)) {
                        printf("[!] Stok tersedia tidak cukup untuk dikurangi\n");
                        break;
                    }
                    
                    printf("\nKonfirmasi perubahan stok? [Y/N]: ");
                    if (!read_line_local(buf, sizeof(buf)) || 
                        (buf[0] != 'y' && buf[0] != 'Y' && buf[0] != '\0')) break;
                        
                    st = lib_update_book_stock(db, b->isbn, delta);
                    if (st == LIB_OK) {
                        printf("Stok berhasil diupdate.\n");
                        lib_db_save(db);
                        animation_loading_bar(300);
                    } else {
                        printf("[!] Gagal mengupdate stok (kode: %d)\n", (int)st);
                    }
                    /* Ask admin if they want to update price */
                    printf("\nUbah harga buku sekarang? [Y/N]: ");
                    if (!read_line_local(buf, sizeof(buf))) break;
                    if (buf[0] == 'y' || buf[0] == 'Y') {
                        printf("Masukkan harga baru (mis. 15000.00): ");
                        if (!read_line_local(buf, sizeof(buf))) break;
                        book_t *bm = lib_find_book_by_isbn_mutable(db, b->isbn);
                        if (bm) {
                            bm->price = atof(buf);
                            printf("Harga berhasil diubah.\n");
                            lib_db_save(db);
                        } else {
                            printf("[!] Gagal menemukan buku untuk mengubah harga.\n");
                        }
                    }
                } else if (choice != 0) {
                    printf("[!] Pilihan tidak valid.\n");
                }
                /* saves/messages already handled inside the choice branches above; avoid duplicate actions */
                break;
            }
            case 5: {
                animation_typewriter("[Admin] Cari buku berdasarkan judul...", 25);
                animation_delay(300);
                printf("Masukkan judul buku untuk dicari\t: ");
                if (!read_line_local(buf, sizeof(buf))) break;
                trim_spaces(buf);

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
                animation_typewriter("[Admin] Memuat history peminjaman...", 25);
                animation_delay(300);
                printf("\n=== HISTORY PEMINJAMAN ===\n\n");
                printf("%-10s | %-13s | %-18s | %-12s | %-12s | %-12s | %s\n",
                       "ID", "ISBN", "Peminjam", "Tgl Pinjam", "Jatuh Tempo", "Kembali", "Status");
                printf("%-10s-+-%-13s-+-%-18s-+-%-12s-+-%-12s-+-%-12s-+-%s\n",
                       "==========", "=============", "==================", "============", "============", "============", "=========");
                
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
            case 7: {
                animation_typewriter("[Admin] Cek pinjaman terlambat...", 25);
                animation_delay(300);
                /* List overdue loans and allow marking one as lost */
                printf("\n=== PINJAMAN TERLAMBAT ===");
                lib_date_t today = lib_date_from_time_t(time(NULL));
                int found = 0;
                for (size_t i = 0; i < db->loans_count; ++i) {
                    const loan_t *l = &db->loans[i];
                    if (!l->is_returned) {
                        int days = lib_date_days_between(l->date_due, today);
                        if (days > 0) {
                            const borrower_t *br = lib_find_borrower_by_id(db, l->borrower_id);
                            printf("Loan ID: %s | ISBN: %s | Borrower: %s | Due: %04d-%02d-%02d | Days late: %d\n",
                                   l->loan_id, l->isbn, br ? br->name : "(unknown)",
                                   l->date_due.year, l->date_due.month, l->date_due.day, days);
                            found++;
                        }
                    }
                }
                if (found == 0) {
                    printf("Tidak ada pinjaman terlambat saat ini.\n");
                    break;
                }

                printf("Masukkan Loan ID untuk ditandai HILANG (atau kosong untuk batal): ");
                if (!read_line_local(buf, sizeof(buf))) break;
                if (buf[0] == '\0') break;
                unsigned long cost = 0;
                lib_status_t st = lib_mark_book_lost(db, buf, &cost);
                if (st == LIB_OK) {
                    printf("Pinjaman %s ditandai HILANG. Biaya penggantian: %lu\n", buf, cost);
                    lib_db_save(db);
                } else {
                    printf("Gagal menandai hilang (kode: %d).\n", (int)st);
                }
                break;
            }
            case 8: {
                animation_typewriter("[Admin] Pengaturan & kebijakan...", 25);
                animation_delay(300);
                /* Settings submenu: view/change fine_per_day and replacement_cost_days */
                printf("\n=== PENGATURAN ADMIN ===");
                long cur_fine = lib_get_fine_per_day(db);
                unsigned long cur_days = lib_get_replacement_cost_days(db);
                printf("Denda per hari saat ini: %ld\n", cur_fine);
                printf("Replacement cost days (fallback) saat ini: %lu\n", cur_days);
                printf("Masukkan nilai baru untuk denda per hari (kosong untuk tidak mengubah): ");
                if (!read_line_local(buf, sizeof(buf))) break;
                if (buf[0] != '\0') {
                    long nf = atol(buf);
                    if (nf >= 0) {
                        lib_set_fine_per_day(db, nf);
                        printf("Denda per hari diubah menjadi: %ld\n", nf);
                    } else {
                        printf("Nilai tidak valid, tidak diubah.\n");
                    }
                }
                printf("Masukkan nilai baru untuk replacement_cost_days (kosong untuk tidak mengubah): ");
                if (!read_line_local(buf, sizeof(buf))) break;
                if (buf[0] != '\0') {
                    unsigned long nd = strtoul(buf, NULL, 10);
                    if (nd > 0) {
                        lib_set_replacement_cost_days(db, nd);
                        printf("Replacement days diubah menjadi: %lu\n", nd);
                    } else {
                        printf("Nilai tidak valid, tidak diubah.\n");
                    }
                }
                /* Persist settings */
                lib_db_save(db);
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
