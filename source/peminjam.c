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
#include <time.h>

#include "library.h"
#include "ui.h"

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

/* Tampilkan daftar buku untuk peminjam */
static void tampilkan_daftar_buku(library_db_t *db) {
    printf("\n=== DAFTAR BUKU ===\n");
    printf("%-15s %-30s %-20s %-5s\n", "ISBN", "Judul", "Pengarang", "Stok");
    printf("------------------------------------------------------------------------\n");
    
    for (size_t i = 0; i < db->books_count; i++) {
        const book_t *b = &db->books[i];
        printf("%-15s %-30s %-20s %-5d\n",
               b->isbn, b->title, b->author, b->available);
    }
}

/* Tampilkan pinjaman yang sedang aktif untuk peminjam */
static void tampilkan_pinjaman_aktif(library_db_t *db, const char *borrower_id) {
    loan_t *loans[64];
    size_t found = lib_find_loans_by_borrower(db, borrower_id, loans, 64);
    if (found == 0) {
        printf("Tidak ada pinjaman aktif.\n");
        return;
    }

    printf("\n=== PINJAMAN AKTIF ===\n");
    printf("%-10s %-15s %-30s %-12s %-12s %s\n",
           "ID", "ISBN", "Judul", "Tgl Pinjam", "Jatuh Tempo", "Status");
    printf("--------------------------------------------------------------------------------\n");
    
    lib_date_t today = lib_date_from_time_t(time(NULL));
    for (size_t i = 0; i < found; i++) {
        const loan_t *l = loans[i];
        const book_t *b = lib_find_book_by_isbn(db, l->isbn);
        
        char date_borrow[16], date_due[16];
        snprintf(date_borrow, sizeof(date_borrow), "%04d-%02d-%02d",
                l->date_borrow.year, l->date_borrow.month, l->date_borrow.day);
        snprintf(date_due, sizeof(date_due), "%04d-%02d-%02d",
                l->date_due.year, l->date_due.month, l->date_due.day);
        
        printf("%-10s %-15s %-30s %-12s %-12s ",
               l->loan_id,
               l->isbn,
               b ? b->title : "???",
               date_borrow,
               date_due);
               
        if (l->is_lost) printf("HILANG\n");
        else if (l->is_returned) printf("Kembali\n");
        else {
            int late = lib_date_days_between(l->date_due, today);
            if (late > 0) {
                printf("TERLAMBAT %d hari\n", late);
            } else {
                printf("Dipinjam\n");
            }
        }
    }
}

void menu_peminjam(library_db_t *db) {
    if (!db) {
        printf("[!] Database belum diinisialisasi.\n");
        return;
    }
    
    printf("=== REGISTRASI/LOGIN PEMINJAM ===\n");
    char nim[32];
    printf("Masukkan NIM Anda: ");
    if (!read_line_local(nim, sizeof(nim))) return;
    
    if (!lib_validate_nim_format(nim)) {
        printf("[!] Format NIM tidak valid.\n");
        return;
    }

    borrower_t *current = lib_get_or_create_borrower_by_nim(db, nim, true);
    if (!current) {
        printf("[!] Gagal memproses data peminjam.\n");
        return;
    }

    if (current->name[0] == '\0') {
        printf("\nSilakan lengkapi data Anda:\n");
        printf("Nama lengkap\t: ");
        read_line_local(current->name, LIB_MAX_NAME);
        printf("No. Telepon\t: ");
        read_line_local(current->phone, sizeof(current->phone));
        printf("Email\t: ");
        read_line_local(current->email, sizeof(current->email));
        lib_db_save(db);
        printf("Data berhasil disimpan.\n");
    }

    int running = 1;
    while (running) {
        printf("\n==== MENU PEMINJAM ====\n");
        printf("Selamat datang, %s (NIM: %s)\n\n", 
               current->name[0] ? current->name : current->nim, 
               current->nim);
        printf("1. Lihat daftar buku\n");
        printf("2. Cari buku berdasarkan judul\n");
        printf("3. Pinjam buku\n");
        printf("4. Lihat pinjaman aktif\n");
        printf("5. Kembalikan buku\n");
        printf("6. Lapor buku hilang\n");
        printf("0. Logout / Kembali ke menu utama\n");
        printf("Pilihan anda: ");
        
        int opt = read_int_choice_local();
        char input[256];
        
        switch (opt) {
            case 1:
                tampilkan_daftar_buku(db);
                break;

            case 2: {
                printf("\nMasukkan judul atau kata kunci\t: ");
                if (!read_line_local(input, sizeof(input))) break;

                const book_t *results[64];
                size_t found = lib_search_books_by_title(db, input, results, 64);
                if (found > 0) {
                    printf("\nDitemukan %zu buku:\n", found);
                    for (size_t i = 0; i < found; i++) {
                        printf("\n=== Buku #%zu ===\n", i+1);
                        lib_print_book(results[i], stdout);
                    }
                } else {
                    printf("Tidak ada buku yang cocok.\n");
                }
                break;
            }
                
            case 3: {
                printf("\nMasukkan ISBN buku yang akan dipinjam: ");
                if (!read_line_local(input, sizeof(input))) break;
                
                const book_t *b = lib_find_book_by_isbn(db, input);
                if (!b) {
                    printf("[!] Buku tidak ditemukan.\n");
                    break;
                }
                
                if (b->available == 0) {
                    printf("[!] Buku sedang tidak tersedia untuk dipinjam.\n");
                    break;
                }

                printf("\nDetail buku yang akan dipinjam:\n");
                lib_print_book(b, stdout);
                
                printf("\nKonfirmasi peminjaman? (y/n): ");
                if (!read_line_local(input, sizeof(input)) || 
                    (input[0] != 'y' && input[0] != 'Y')) break;
                
                lib_date_t today = lib_date_from_time_t(time(NULL));
                lib_date_t due_date = today;
                due_date.day += 7; // 7 hari peminjaman
                
                char loan_id[32];
                lib_status_t st = lib_checkout_book(db, b->isbn, current, today, due_date, loan_id);
                if (st == LIB_OK) {
                    printf("Peminjaman berhasil!\n");
                    printf("ID Pinjam: %s\n", loan_id);
                    printf("Tanggal kembali: %04d-%02d-%02d\n", 
                           due_date.year, due_date.month, due_date.day);
                    lib_db_save(db);
                } else {
                    printf("[!] Gagal meminjam buku (kode: %d)\n", (int)st);
                }
                break;
            }
                
            case 4:
                tampilkan_pinjaman_aktif(db, current->id);
                break;

            case 5: {
                tampilkan_pinjaman_aktif(db, current->id);
                printf("\nMasukkan ID Pinjaman yang akan dikembalikan: ");
                if (!read_line_local(input, sizeof(input))) break;

                lib_date_t today = lib_date_from_time_t(time(NULL));
                unsigned long fine = 0;
                lib_status_t st = lib_return_book(db, input, today, &fine);
                if (st == LIB_OK) {
                    printf("Buku berhasil dikembalikan!\n");
                    if (fine > 0) {
                        printf("[!] Denda keterlambatan: Rp%lu\n", fine);
                    }
                    lib_db_save(db);
                } else {
                    printf("[!] Gagal mengembalikan buku (kode: %d)\n", (int)st);
                }
                break;
            }

            case 6: {
                tampilkan_pinjaman_aktif(db, current->id);
                /* For option 6, we don't need to prompt for Enter after showing history */
                break;
            }

            case 0:
                printf("Logout berhasil. Sampai jumpa %s!\n", 
                       current->name[0] ? current->name : current->nim);
                running = 0;
                break;
            
            default:
                printf("[!] Pilihan tidak valid.\n");
                break;
        }

        if (running && opt != 1) {
            printf("\nTekan Enter untuk melanjutkan...");
            while (getchar() != '\n');
        }
    }
}
