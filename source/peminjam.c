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

#include "../include/library.h"
#include "../include/ui.h"
#include "../include/animation.h"

/* Provide portable case-insensitive compare if platform lacks strcasecmp */
#if !defined(__APPLE__) && !defined(__unix__) && (defined(_WIN32) || defined(_WIN64))
/* On some Windows toolchains strcasecmp/_stricmp may not be available; provide small fallback */
static int portable_strcasecmp(const char *a, const char *b) {
    if (!a || !b) return (a == b) ? 0 : (a ? 1 : -1);
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}
#ifndef strcasecmp
#define strcasecmp portable_strcasecmp
#endif
#endif


static char *read_line_local(char *buf, size_t size) {
    if (!buf || size == 0) return NULL;
    if (fgets(buf, (int)size, stdin) == NULL) return NULL;
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    return buf;
}

/* trim leading and trailing whitespace in-place (local copy) */
static void trim_spaces(char *s) {
    if (!s) return;
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len-1])) s[--len] = '\0';
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
    printf("\n%-15s | %-28s | %-18s | %-10s | %-6s | %s\n",
           "ISBN", "Judul", "Pengarang", "Harga", "Total", "Tersedia");
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
}

/* Tampilkan pinjaman yang sedang aktif untuk peminjam */
static void tampilkan_pinjaman_aktif(library_db_t *db, const char *borrower_id) {
    loan_t *loans[64];
    size_t found = lib_find_loans_by_borrower(db, borrower_id, loans, 64);
    if (found == 0) {
        printf("Tidak ada pinjaman aktif.\n");
        return;
    }

    printf("\n=== PINJAMAN AKTIF ===\n\n");
    printf("%-10s | %-13s | %-28s | %-12s | %-12s | %s\n",
           "ID", "ISBN", "Judul", "Tgl Pinjam", "Jatuh Tempo", "Status");
    printf("%-10s-+-%-13s-+-%-28s-+-%-12s-+-%-12s-+-%s\n",
           "==========", "=============", "============================", "============", "============", "==========");
    
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
    
    printf("\n=== REGISTRASI/LOGIN PEMINJAM ===\n\n");
    char nim[32];
    printf("NIM Anda              : ");
    if (!read_line_local(nim, sizeof(nim))) return;
    trim_spaces(nim);
    
    if (!lib_validate_nim_format(nim)) {
        printf("[!] Format NIM tidak valid.\n");
        return;
    }

    borrower_t *current = lib_get_or_create_borrower_by_nim(db, nim, true);
    if (!current) {
        printf("[!] Gagal memproses data peminjam.\n");
        return;
    }

    /* initialize animation once and show welcome */
    animation_init();
    animation_typewriter("Selamat datang di Perpustakaan Digital", 30);
    /* Short pause so the welcome animation is visible before menu content prints */
    animation_delay(450);

    if (current->name[0] == '\0') {
        printf("\n--- Lengkapi Data Anda ---\n\n");
        printf("Nama Lengkap          : ");
        read_line_local(current->name, LIB_MAX_NAME);
        printf("No. Telepon           : ");
        read_line_local(current->phone, sizeof(current->phone));
        printf("Email                 : ");
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
                system("cls");
                animation_typewriter("[Peminjam] Memuat daftar buku...", 25);
                animation_delay(300);
                animation_bookshelf_scan((int)(db->books_count > 12 ? 12 : db->books_count));
                tampilkan_daftar_buku(db);
                break;

            case 2: {
                system("cls");
                animation_typewriter("[Peminjam] Cari buku...", 25);
                animation_delay(300);
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
                system("cls");
                animation_typewriter("[Peminjam] Pinjam buku...", 25);
                animation_delay(300);
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
                
                printf("\nKonfirmasi peminjaman? [Y/N]: ");
                if (!read_line_local(input, sizeof(input)) || 
                    (input[0] != 'y' && input[0] != 'Y' && input[0] != '\0')) break;
                
                printf("\nMasukkan tanggal peminjaman (YYYY-MM-DD): ");
                char date_str[11];
                if (!read_line_local(date_str, sizeof(date_str))) break;
                
                lib_date_t borrow_date = {0};
                if (sscanf(date_str, "%d-%d-%d", &borrow_date.year, &borrow_date.month, &borrow_date.day) != 3) {
                    printf("[!] Format tanggal tidak valid. Gunakan format YYYY-MM-DD\n");
                    break;
                }
                
                lib_date_t today = lib_date_from_time_t(time(NULL));
                lib_date_t due_date = borrow_date;
                due_date.day += 7; // 7 hari peminjaman
                
                char loan_id[32];
                lib_status_t st = lib_checkout_book(db, b->isbn, current, borrow_date, due_date, loan_id);
                if (st == LIB_OK) {
                    printf("Peminjaman berhasil!\n");
                    printf("ID Pinjam: %s\n", loan_id);
                    printf("Tanggal kembali: %04d-%02d-%02d\n", 
                           due_date.year, due_date.month, due_date.day);
                    lib_db_save(db);
                    animation_loading_bar(400);
                } else {
                    printf("[!] Gagal meminjam buku (kode: %d)\n", (int)st);
                }
                break;
            }
                
            case 4:
                system("cls");
                animation_typewriter("[Peminjam] Memuat pinjaman aktif Anda...", 25);
                animation_delay(300);
                tampilkan_pinjaman_aktif(db, current->id);
                break;

            case 5: {
                system("cls");
                animation_typewriter("[Peminjam] Kembalikan buku...", 25);
                animation_delay(300);
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
                        /* Require exact payment matching the fine */
                        while (1) {
                            printf("Masukkan jumlah yang dibayar (harus Rp%lu) atau ketik 'batal' untuk membatalkan: ", fine);
                            char paybuf[64];
                            if (!read_line_local(paybuf, sizeof(paybuf))) break;
                            trim_spaces(paybuf);
                            if (strcasecmp(paybuf, "batal") == 0) {
                                printf("Pembayaran dibatalkan oleh peminjam.\n");
                                break;
                            }
                            unsigned long paid = strtoul(paybuf, NULL, 10);
                            if (paid == fine) {
                                lib_set_loan_payment(db, input, (long)paid);
                                printf("Pembayaran Rp%lu dicatat. Terima kasih.\n", paid);
                                break;
                            } else {
                                printf("Jumlah tidak sesuai. Harus tepat Rp%lu. Coba lagi.\n", fine);
                            }
                        }
                    }
                    lib_db_save(db);
                    animation_loading_bar(300);
                } else {
                    printf("[!] Gagal mengembalikan buku (kode: %d)\n", (int)st);
                }
                break;
            }

            case 6: {
                system("cls");
                animation_typewriter("[Peminjam] Lapor buku hilang...", 25);
                animation_delay(300);
                tampilkan_pinjaman_aktif(db, current->id);
                printf("\nMasukkan ID Pinjaman yang akan dilaporkan HILANG (atau kosong untuk batal): ");
                if (!read_line_local(input, sizeof(input))) break;
                if (input[0] == '\0') break;
                /* Find the loan in DB first and validate status before marking lost */
                loan_t *found_ln = NULL;
                for (size_t ii = 0; ii < db->loans_count; ++ii) {
                    if (strcmp(db->loans[ii].loan_id, input) == 0) { found_ln = &db->loans[ii]; break; }
                }
                if (!found_ln) {
                    printf("[!] Loan ID '%s' tidak ditemukan.\n", input);
                    break;
                }
                if (found_ln->is_lost) {
                    printf("[!] Pinjaman %s sudah ditandai HILANG sebelumnya.\n", input);
                    break;
                }
                if (found_ln->is_returned) {
                    printf("[!] Pinjaman %s sudah dikembalikan; tidak dapat ditandai HILANG.\n", input);
                    break;
                }

                unsigned long cost = 0;
                lib_status_t st2 = lib_mark_book_lost(db, input, &cost);
                if (st2 == LIB_OK) {
                    printf("Pinjaman %s berhasil ditandai HILANG. Biaya penggantian: Rp%lu\n", input, cost);
                    /* Require exact payment for replacement cost */
                    while (1) {
                        printf("Masukkan jumlah yang dibayarkan untuk penggantian (harus Rp%lu) atau ketik 'batal' untuk membatalkan: ", cost);
                        char paybuf2[64];
                        if (!read_line_local(paybuf2, sizeof(paybuf2))) break;
                        trim_spaces(paybuf2);
                        if (strcasecmp(paybuf2, "batal") == 0) {
                            printf("Pembayaran penggantian dibatalkan oleh peminjam.\n");
                            break;
                        }
                        unsigned long paid2 = strtoul(paybuf2, NULL, 10);
                        if (paid2 == cost) {
                            lib_set_loan_payment(db, input, (long)paid2);
                            printf("Pembayaran penggantian Rp%lu dicatat. Terima kasih.\n", paid2);
                            break;
                        } else {
                            printf("Jumlah tidak sesuai. Harus tepat Rp%lu. Coba lagi.\n", cost);
                        }
                    }
                    lib_db_save(db);
                    animation_loading_bar(400);
                } else {
                    printf("Gagal menandai hilang (kode: %d).\n", (int)st2);
                }
                break;
            }

            case 0:
                system("cls");
                printf("Logout berhasil. Sampai jumpa %s!\n", 
                       current->name[0] ? current->name : current->nim);
                running = 0;
                break;
            
            default:
                printf("[!] Pilihan tidak valid.\n");
                press_enter();
                system("cls");
                break;
        }

        if (running && opt != 1) {
            printf("\nTekan Enter untuk melanjutkan...");
            while (getchar() != '\n');
        }
    }
}
