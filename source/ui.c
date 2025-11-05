#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "library.h"
#include "view.h"


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
static char *read_line(char *buf, size_t size) {
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
    printf("Masukkan NIM (atau 'q' untuk kembali): ");
    if (!read_line(nim, sizeof(nim))) return 0;
    if (strcmp(nim, "q") == 0 || strcmp(nim, "Q") == 0) return 0;

    if (lib_validate_nim_format(nim)) {
        printf("Login berhasil dengan NIM: %s\n", nim);
        return 1;
    } else {
        printf("[!] Format NIM tidak valid.\n");
        return 0;
    }
}

// Menampilkan daftar buku dengan format yang lebih baik
static void tampilkan_daftar_buku(const library_db_t *db) {
    if (!db || db->books_count == 0) {
        printf("Belum ada data buku.\n");
        return;
    }

    printf("\n=== DAFTAR BUKU ===\n");
    printf("%-4s %-15s %-30s %-20s %-8s %-8s\n", 
           "No", "ISBN", "Judul", "Penulis", "Total", "Tersedia");
    printf("--------------------------------------------------------------------------------\n");
    
    for (size_t i = 0; i < db->books_count; ++i) {
        const book_t *b = &db->books[i];
        printf("%-4zu %-15s %-30s %-20s %-8d %-8d\n",
               i+1, b->isbn, b->title, b->author, b->total_stock, b->available);
    }
    printf("\nTotal: %zu buku\n", db->books_count);
}

// Menu for borrower
void menu_peminjam(library_db_t *db) {
    if (!db) {
        printf("[!] Database belum diinisialisasi.\n");
        return;
    }

    borrower_t *current = NULL;
    printf("=== REGISTRASI/LOGIN PEMINJAM ===\n");
    char nim[32];
    printf("Masukkan NIM Anda: ");
    if (!read_line(nim, sizeof(nim))) return;
    
    if (!lib_validate_nim_format(nim)) {
        printf("[!] Format NIM tidak valid.\n");
        return;
    }

    current = lib_get_or_create_borrower_by_nim(db, nim, true);
    if (!current) {
        printf("[!] Gagal memproses data peminjam.\n");
        return;
    }

    if (current->name[0] == '\0') {
        printf("\nSilakan lengkapi data Anda:\n");
        printf("Nama lengkap   : ");
        read_line(current->name, LIB_MAX_NAME);
        printf("No. Telepon    : ");
        read_line(current->phone, sizeof(current->phone));
        printf("Email          : ");
        read_line(current->email, sizeof(current->email));
        lib_db_save(db);
        printf("Data berhasil disimpan.\n");
    }

    int running = 1;
    while (running) {
        printf("\n=== MENU PEMINJAM ===\n");
        printf("Selamat datang, %s (NIM: %s)\n\n", 
               current->name[0] ? current->name : current->nim, 
               current->nim);
        printf("1) Lihat Daftar Buku\n");
        printf("2) Cari dan Pinjam Buku\n");
        printf("3) Kembalikan Buku\n");
        printf("4) Lihat Riwayat Peminjaman\n");
        printf("5) Lapor Buku Hilang\n");
        printf("6) Logout / Kembali ke menu utama\n");
        printf("Pilih menu: ");

        char input[32];
        int choice = 0;
        if (read_line(input, sizeof(input))) {
            choice = atoi(input);
        }

        switch (choice) {
            case 1:
                tampilkan_daftar_buku(db);
                break;
                
            case 2: {
                const book_t *selected = NULL;
                printf("\nMasukkan ISBN atau kata kunci judul: ");
                char keyword[256];
                if (read_line(keyword, sizeof(keyword))) {
                    // Coba cari dengan ISBN dulu
                    if (strlen(keyword) < LIB_MAX_ISBN) {
                        selected = lib_find_book_by_isbn(db, keyword);
                    }
                    
                    // Jika tidak ditemukan, cari berdasarkan judul
                    if (!selected) {
                        const book_t *results[64];
                        size_t found = lib_search_books_by_title(db, keyword, results, 64);
                        if (found > 0) {
                            printf("\nDitemukan %zu buku:\n", found);
                            for (size_t i = 0; i < found; i++) {
                                printf("%zu) %s - %s\n", i+1, results[i]->isbn, results[i]->title);
                            }
                            printf("\nPilih nomor buku (0 untuk batal): ");
                            if (read_line(input, sizeof(input))) {
                                int sel = atoi(input) - 1;
                                if (sel >= 0 && sel < (int)found) {
                                    selected = results[sel];
                                }
                            }
                        }
                    }

                    if (selected) {
                        printf("\nDetail buku:\n");
                        ui_show_book_detail(selected);
                        if (selected->available > 0) {
                            printf("\nIngin meminjam buku ini? (y/n): ");
                            if (read_line(input, sizeof(input)) && (input[0] == 'y' || input[0] == 'Y')) {
                                lib_date_t now = lib_date_from_time_t(time(NULL));
                                lib_date_t due = now;
                                due.day += 7; // 7 hari peminjaman
                                
                                char loan_id[32];
                                lib_status_t st = lib_checkout_book(db, selected->isbn, current, now, due, loan_id);
                                if (st == LIB_OK) {
                                    printf("Peminjaman berhasil! ID: %s\n", loan_id);
                                    printf("Tanggal kembali: %04d-%02d-%02d\n", 
                                           due.year, due.month, due.day);
                                    lib_db_save(db);
                                } else {
                                    printf("[!] Gagal meminjam buku (kode: %d)\n", (int)st);
                                }
                            }
                        } else {
                            printf("[!] Maaf, buku sedang tidak tersedia.\n");
                        }
                    } else {
                        printf("[!] Buku tidak ditemukan.\n");
                    }
                }
                break;
            }
                
            case 3: {
                // Tampilkan pinjaman aktif
                loan_t *loans[64];
                size_t found = lib_find_loans_by_borrower(db, current->id, loans, 64);
                if (found == 0) {
                    printf("Tidak ada pinjaman aktif.\n");
                    break;
                }

                printf("\nPinjaman aktif:\n");
                for (size_t i = 0; i < found; i++) {
                    if (!loans[i]->is_returned) {
                        const book_t *book = lib_find_book_by_isbn(db, loans[i]->isbn);
                        printf("%zu) %s - %s (due: %04d-%02d-%02d)\n",
                               i+1, loans[i]->loan_id,
                               book ? book->title : loans[i]->isbn,
                               loans[i]->date_due.year,
                               loans[i]->date_due.month,
                               loans[i]->date_due.day);
                    }
                }

                printf("\nMasukkan ID pinjaman untuk dikembalikan (0 untuk batal): ");
                if (read_line(input, sizeof(input))) {
                    lib_date_t now = lib_date_from_time_t(time(NULL));
                    unsigned long fine = 0;
                    lib_status_t st = lib_return_book(db, input, now, &fine);
                    if (st == LIB_OK) {
                        printf("Buku berhasil dikembalikan!\n");
                        if (fine > 0) {
                            printf("[!] Denda keterlambatan: Rp%lu\n", fine);
                        }
                        lib_db_save(db);
                    } else {
                        printf("[!] Gagal mengembalikan buku (kode: %d)\n", (int)st);
                    }
                }
                break;
            }
                
            case 4: {
                loan_t *loans[64];
                size_t found = lib_find_loans_by_borrower(db, current->id, loans, 64);
                if (found == 0) {
                    printf("Belum ada riwayat peminjaman.\n");
                    break;
                }

                printf("\n=== RIWAYAT PEMINJAMAN ===\n");
                lib_date_t today = lib_date_from_time_t(time(NULL));
                for (size_t i = 0; i < found; i++) {
                    const loan_t *loan = loans[i];
                    const book_t *book = lib_find_book_by_isbn(db, loan->isbn);
                    
                    printf("\nPinjaman #%zu\n", i+1);
                    printf("ID: %s\n", loan->loan_id);
                    printf("Buku: %s\n", book ? book->title : loan->isbn);
                    printf("Tanggal Pinjam: %04d-%02d-%02d\n",
                           loan->date_borrow.year,
                           loan->date_borrow.month,
                           loan->date_borrow.day);
                    printf("Jatuh Tempo: %04d-%02d-%02d\n",
                           loan->date_due.year,
                           loan->date_due.month,
                           loan->date_due.day);
                           
                    if (loan->is_lost) {
                        printf("Status: HILANG (denda: Rp%ld)\n", loan->fine_paid);
                    } else if (loan->is_returned) {
                        printf("Status: Dikembalikan pada %04d-%02d-%02d\n",
                               loan->date_returned.year,
                               loan->date_returned.month,
                               loan->date_returned.day);
                        if (loan->fine_paid > 0) {
                            printf("Denda: Rp%ld\n", loan->fine_paid);
                        }
                    } else {
                        int late = lib_date_days_between(loan->date_due, today);
                        if (late > 0) {
                            unsigned long est = lib_calculate_fine(db, loan->date_due, today);
                            printf("Status: TERLAMBAT %d hari (est. denda: Rp%lu)\n",
                                   late, est);
                        } else {
                            printf("Status: Dipinjam (masih dalam masa pinjam)\n");
                        }
                    }
                }
                break;
            }
                
            case 5: {
                loan_t *loans[64];
                size_t found = lib_find_loans_by_borrower(db, current->id, loans, 64);
                bool has_active = false;
                
                printf("\nPinjaman aktif:\n");
                for (size_t i = 0; i < found; i++) {
                    if (!loans[i]->is_returned && !loans[i]->is_lost) {
                        has_active = true;
                        const book_t *book = lib_find_book_by_isbn(db, loans[i]->isbn);
                        printf("%zu) %s - %s\n",
                               i+1, loans[i]->loan_id,
                               book ? book->title : loans[i]->isbn);
                    }
                }
                
                if (!has_active) {
                    printf("Tidak ada pinjaman aktif.\n");
                    break;
                }
                
                printf("\nMasukkan ID pinjaman buku yang hilang: ");
                if (read_line(input, sizeof(input))) {
                    unsigned long fine = 0;
                    lib_status_t st = lib_mark_book_lost(db, input, &fine);
                    if (st == LIB_OK) {
                        printf("Laporan kehilangan dicatat.\n");
                        printf("Biaya penggantian: Rp%lu\n", fine);
                        lib_db_save(db);
                    } else {
                        printf("[!] Gagal mencatat kehilangan (kode: %d)\n", (int)st);
                    }
                }
                break;
            }
                
            case 6:
                printf("Logout berhasil. Sampai jumpa %s!\n", 
                       current->name[0] ? current->name : current->nim);
                running = 0;
                break;
                
            default:
                printf("[!] Pilihan tidak valid.\n");
                break;
        }
        
        if (running && choice != 1) {
            printf("\nTekan Enter untuk melanjutkan...");
            while (getchar() != '\n');
        }
    }
}