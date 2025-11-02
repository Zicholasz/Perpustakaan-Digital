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

#include "library.h"  /* kontrak publik: fungsi lib_* dan tipe data */

/* ---------- Konfigurasi modul ---------- */
#define LOCAL_READBUF 256
#define SEARCH_RESULTS_CAP 64   
#define DEFAULT_LOAN_DURATION_DAYS 7 /* durasi pinjam default */

/* ---------- Helper: safe input ---------- */
/* read_line: baca line dari stdin ke buffer, trim newline.
 * - Mengembalikan pointer ke buffer (sama seperti buf), atau NULL pada EOF/error.
 * - Gunakan ini untuk menggantikan scanf agar lebih aman.
 */
static char *read_line(char *buf, size_t size) {
    if (!buf || size == 0) return NULL;
    if (fgets(buf, (int)size, stdin) == NULL) return NULL; /* EOF atau error */
    /* hapus newline dan carriage return di akhir */
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
        buf[--len] = '\0';
    }
    return buf;
}

/* read_int_choice: membaca satu baris dan parse int sederhana.
 * - Kembalikan -1 jika tidak ada input atau parse gagal.
 */
static int read_int_choice(void) {
    char tmp[64];
    if (!read_line(tmp, sizeof(tmp))) return -1;
    /* skip leading whitespace */
    char *p = tmp;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '\0') return -1;
    return atoi(p);
}

/* ---------- Helper: waktu string (timestamp) ---------- */
/* now_str: isi out (minimal 20 bytes) dengan "YYYY-MM-DD HH:MM:SS" lokal.
 * - Gunakan localtime_r/localtime_s compat jika tersedia.
 */
static void now_str(char *out, size_t out_sz) {
    if (!out || out_sz < 20) return;
    time_t t = time(NULL);
#if defined(_WIN32) || defined(_WIN64)
    struct tm tm;
    /* MSVC: use localtime_s */
    #if defined(_MSC_VER)
        localtime_s(&tm, &t);
    #else
        struct tm *tmp = localtime(&t);
        if (tmp) tm = *tmp;
        else memset(&tm, 0, sizeof(tm));
    #endif
#else
    struct tm tm;
    localtime_r(&t, &tm);
#endif
    strftime(out, out_sz, "%Y-%m-%d %H:%M:%S", &tm);
}

/* ---------- UI helpers ---------- */
static void print_header(const char *title) {
    printf("\n================ %s ================\n", title ? title : "Menu");
}

/* ---------- LOGIN / REGISTER by NIM ---------- */
/* peminjam_login_by_nim:
 *  - meminta NIM dari user
 *  - validasi format via lib_validate_nim_format()
 *  - memanggil lib_get_or_create_borrower_by_nim(db, nim, true)
 *  - jika borrower baru (name kosong), meminta nama/phone/email untuk melengkapi
 *  - mengembalikan pointer internal borrower_t (tidak perlu free)
 */
static borrower_t *peminjam_login_by_nim(library_db_t *db) {
    char nim[64];
    while (1) {
        printf("Masukkan NIM (atau 'q' untuk kembali)\t: ");
        if (!read_line(nim, sizeof(nim))) return NULL;
        if (strcmp(nim, "q") == 0 || strcmp(nim, "Q") == 0) return NULL;

        /* validasi format NIM sesuai kebijakan library */
        if (!lib_validate_nim_format(nim)) {
            printf("[!] Format NIM tidak valid (contoh: 67202301).\n");
            continue;
        }

        /* ambil atau buat borrower berdasarkan nim (create_if_missing = true) */
        borrower_t *br = lib_get_or_create_borrower_by_nim(db, nim, true);
        if (!br) {
            printf("[!] Gagal membuat/ambil data peminjam.\nSilakan coba lagi atau hubungi admin.\n");
            return NULL;
        }

        /* jika baru dibuat, nama masih kosong -> minta data tambahan */
        if (br->name[0] == '\0') {
            printf("NIM belum terdaftar. Silakan lengkapi data singkat\t:\n");
            printf("Nama lengkap peminjam\t: ");
            read_line(br->name, LIB_MAX_NAME);
            printf("No. Telepon (opsional)\t: ");
            read_line(br->phone, sizeof(br->phone));
            printf("Email (opsional)\t: ");
            read_line(br->email, sizeof(br->email));
            printf("Pendaftaran berhasil. ID internal\t: %s\n", br->id);
            /* tidak memanggil lib_add_borrower karena lib_get_or_create sudah menambah internal */
        } else {
            printf("Selamat datang kembali, %s (NIM\t: %s)\n",
                   br->name[0] ? br->name : br->nim, br->nim);
        }
        return br;
    }
}

/* ---------- SEARCH BOOK interactive ----------
 * - User masukkan keyword atau ISBN
 * - Jika keyword tampak seperti ISBN (no spaces, panjang < LIB_MAX_ISBN), coba lib_find_book_by_isbn
 * - Jika tidak, lib_search_books_by_title -> isi results array and present to user
 * - Kembalikan const book_t* yang dipilih (atau NULL jika cancel)
 */
static const book_t *search_books_interactive(const library_db_t *db) {
    char kw[256];
    printf("Masukkan kata kunci judul atau ISBN (atau 'q' untuk kembali)\t: ");
    if (!read_line(kw, sizeof(kw))) return NULL;
    if (strcmp(kw, "q") == 0 || strcmp(kw, "Q") == 0) return NULL;

    size_t klen = strlen(kw);
    /* heuristik: jika tidak ada spasi dan panjang < LIB_MAX_ISBN -> coba ISBN exact */
    if (klen > 0 && klen < LIB_MAX_ISBN && strchr(kw, ' ') == NULL) {
        const book_t *b = lib_find_book_by_isbn(db, kw);
        if (b) {
            /* tampilkan ringkasan */
            printf("[i] Ditemukan (ISBN exact)\t: %s | %s | tersedia: %d\n", b->isbn, b->title, b->available);
            return b;
        }
        /* jika tidak ditemukan, lanjut ke search by title */
    }

    const book_t *results[SEARCH_RESULTS_CAP];
    size_t found = lib_search_books_by_title(db, kw, results, SEARCH_RESULTS_CAP);
    if (found == 0) {
        printf("[i] Tidak ditemukan buku untuk '%s'\n", kw);
        return NULL;
    }

    printf("[i] Ditemukan %zu hasil (menampilkan max %d):\n", found, SEARCH_RESULTS_CAP);
    for (size_t i = 0; i < found; ++i) {
        printf("%2zu) ISBN: %s | Judul: %s | Penulis: %s | Tersedia: %d\n",
               i+1, results[i]->isbn, results[i]->title, results[i]->author, results[i]->available);
    }

    printf("Pilih nomor buku untuk detail/pinjam, 0 untuk batal\t: ");
    int choice = read_int_choice();
    if (choice <= 0 || (size_t)choice > found) {
        printf("[i] Batal memilih buku.\n");
        return NULL;
    }

    const book_t *sel = results[choice-1];
    printf("\n-- Detail Buku --\n");
    lib_print_book(sel, stdout);  /* util printing dari library.c */
    return sel;
}

/* ---------- CHECKOUT (peminjaman) ----------
 * - Memilih buku terlebih dahulu
 * - Memeriksa available stock
 * - Hitung due date default (DEFAULT_LOAN_DURATION_DAYS)
 * - Panggil lib_checkout_book(...)
 * - Jika sukses: auto-save via lib_db_save() dan tampilkan timestamp
 */
static void checkout_flow(library_db_t *db, borrower_t *br) {
    if (!db || !br) return;

    const book_t *b = search_books_interactive(db);
    if (!b) return; /* user batal atau tidak ditemukan */

    if (b->available <= 0) {
        printf("[!] Maaf, stok buku '%s' sedang habis.\n", b->title);
        return;
    }

    /* tanggal pinjam = hari ini */
    time_t now_t = time(NULL);
    lib_date_t date_borrow = lib_date_from_time_t(now_t);

    /* due date = now + DEFAULT_LOAN_DURATION_DAYS */
    time_t due_t = now_t + (time_t)DEFAULT_LOAN_DURATION_DAYS * 24 * 3600;
    lib_date_t date_due = lib_date_from_time_t(due_t);

    /* lakukan checkout */
    char loan_id[32] = {0};
    lib_status_t st = lib_checkout_book(db, b->isbn, br, date_borrow, date_due, loan_id);
    if (st == LIB_OK) {
        printf("Berhasil meminjam '%s' (Loan ID: %s)\n", b->title, loan_id);
        printf("    Pinjam: %04d-%02d-%02d | Jatuh tempo: %04d-%02d-%02d\n",
               date_borrow.year, date_borrow.month, date_borrow.day,
               date_due.year, date_due.month, date_due.day);

        /* AUTO-SAVE: simpan DB segera setelah perubahan */
        if (lib_db_save(db) == LIB_OK) {
            char ts[32]; now_str(ts, sizeof(ts));
            printf("Database disimpan otomatis pada %s\n", ts);
        } else {
            printf("[!] Gagal menyimpan database otomatis.\nSilakan hubungi admin.\n");
        }
    } else if (st == LIB_ERR_NO_STOCK) {
        printf("[!] Gagal: stok tidak cukup.\n");
    } else {
        printf("[!] Gagal meminjam (kode error: %d)\n", (int)st);
    }
}

/* ---------- VIEW loans for borrower ----------
 * - Ambil loans milik borrower via lib_find_loans_by_borrower
 * - Tampilkan ringkasan tiap loan (status, due date, estimasi denda jika terlambat)
 */
static void view_loans_for_borrower(const library_db_t *db, const borrower_t *br) {
    if (!db || !br) return;

    loan_t *out[256];
    size_t n = lib_find_loans_by_borrower(db, br->id, out, 256);
    if (n == 0) {
        printf("[i] Belum ada transaksi pinjaman untuk NIM %s\n", br->nim);
        return;
    }

    printf("[i] Menampilkan %zu transaksi:\n", n);
    for (size_t i = 0; i < n; ++i) {
        loan_t *ln = out[i]; /* pointer internal dari library */
        printf("----\n");
        printf("Loan ID: %s | ISBN: %s\n", ln->loan_id, ln->isbn);
        printf("Pinjam: %04d-%02d-%02d | Due: %04d-%02d-%02d\n",
               ln->date_borrow.year, ln->date_borrow.month, ln->date_borrow.day,
               ln->date_due.year, ln->date_due.month, ln->date_due.day);
        if (ln->is_returned) {
            printf("Status: RETURNED on %04d-%02d-%02d\n",
                   ln->date_returned.year, ln->date_returned.month, ln->date_returned.day);
            if (ln->fine_paid > 0) printf("Denda tercatat: %ld\n", (long)ln->fine_paid);
        } else {
            /* belum returned -> hitung keterlambatan relatif ke hari ini */
            lib_date_t today = lib_date_from_time_t(time(NULL));
            int late_days = lib_date_days_between(ln->date_due, today);
            if (late_days > 0) {
                unsigned long est = lib_calculate_fine(db, ln->date_due, today);
                printf("Status: PENDING (terlambat %d hari) -> estimasi denda: %lu\n", late_days, est);
            } else {
                printf("Status: PENDING (belum lewat jatuh tempo)\n");
            }
        }
        if (ln->is_lost) printf("NOTE: Buku dilaporkan HILANG. Biaya/ganti rugi tercatat: %ld\n", (long)ln->fine_paid);
    }
}

/* ---------- RETURN (pengembalian)
 * - Input Loan ID, panggil lib_return_book
 * - Tampilkan denda bila ada
 * - AUTO-SAVE setelah sukses
 */
static void return_flow(library_db_t *db) {
    char loan_id[64];
    printf("Masukkan Loan ID yang akan dikembalikan (atau 'q' untuk kembali)\t: ");
    if (!read_line(loan_id, sizeof(loan_id))) return;
    if (strcmp(loan_id, "q") == 0 || strcmp(loan_id, "Q") == 0) return;

    lib_date_t date_return = lib_date_from_time_t(time(NULL));
    unsigned long fine = 0;
    lib_status_t st = lib_return_book(db, loan_id, date_return, &fine);
    if (st == LIB_OK) {
        printf("Pengembalian berhasil.\n");
        if (fine > 0) printf("[!] Terdapat denda keterlambatan sebesar: %lu (silakan bayar ke admin).\n", fine);
        else printf("[i] Tidak ada denda.\n");

        /* AUTO-SAVE */
        if (lib_db_save(db) == LIB_OK) {
            char ts[32]; now_str(ts, sizeof(ts));
            printf("Database disimpan otomatis pada %s\n", ts);
        } else {
            printf("[!] Gagal menyimpan database otomatis setelah pengembalian.\n");
        }
    } else if (st == LIB_ERR_NOT_FOUND) {
        printf("[!] Loan ID tidak ditemukan.\n");
    } else {
        printf("[!] Gagal mengembalikan (kode: %d)\n", (int)st);
    }
}

/* ---------- REPORT LOST (lapor buku hilang)
 * - Input Loan ID, panggil lib_mark_book_lost
 * - Tampilkan biaya/ganti rugi
 * - AUTO-SAVE setelah sukses
 */
static void report_lost_flow(library_db_t *db) {
    char loan_id[64];
    printf("Masukkan Loan ID buku yang hilang (atau 'q' untuk kembali)\t: ");
    if (!read_line(loan_id, sizeof(loan_id))) return;
    if (strcmp(loan_id, "q") == 0 || strcmp(loan_id, "Q") == 0) return;

    unsigned long cost = 0;
    lib_status_t st = lib_mark_book_lost(db, loan_id, &cost);
    if (st == LIB_OK) {
        printf("Buku ditandai HILANG. Estimasi biaya/ganti rugi\t: %lu (hubungi admin untuk pembayaran).\n", cost);
        if (lib_db_save(db) == LIB_OK) {
            char ts[32]; now_str(ts, sizeof(ts));
            printf("Database disimpan otomatis pada %s\n", ts);
        } else {
            printf("[!] Gagal menyimpan database otomatis setelah pelaporan hilang.\n");
        }
    } else if (st == LIB_ERR_NOT_FOUND) {
        printf("[!] Loan ID tidak ditemukan.\n");
    } else if (st == LIB_ERR_INVALID_ARG) {
        printf("[!] Operasi tidak valid (mungkin sudah dilaporkan hilang sebelumnya).\n");
    } else {
        printf("[!] Gagal menandai hilang (kode: %d)\n", (int)st);
    }
}

/* ---------- ENTRY POINT: peminjam_menu ----------
 * - dipanggil dari main.c dengan pointer db ter-inisialisasi
 * - tidak memanggil lib_db_save() saat logout karena autosave sudah dilakukan di aksi penting
 */
void peminjam_menu(library_db_t *db) {
    if (!db) {
        printf("[!] Database belum diinisialisasi.\nSilakan hubungi admin.\n");
        return;
    }

    print_header("Menu Peminjam");

    /* 1) Login by NIM (atau register otomatis) */
    borrower_t *current = peminjam_login_by_nim(db);
    if (!current) {
        printf("[i] Kembali ke menu utama.\n");
        return;
    }

    /* 2) Loop menu peminjam */
    int running = 1;
    while (running) {
        printf("\nMenu Peminjam - Pilih aksi:\n");
        printf("1) Pinjam buku\n");
        printf("2) Kembalikan buku (input Loan ID)\n");
        printf("3) Lihat daftar pinjaman saya\n");
        printf("4) Lapor buku hilang\n");
        printf("5) Logout / kembali ke menu utama\n");
        printf("> ");

        int opt = read_int_choice();
        switch (opt) {
            case 1:
                checkout_flow(db, current);
                break;
            case 2:
                return_flow(db);
                break;
            case 3:
                view_loans_for_borrower(db, current);
                break;
            case 4:
                report_lost_flow(db);
                break;
            case 5:
            case -1:
                printf("[i] Logout. Sampai jumpa, %s.\n", current->name[0] ? current->name : current->nim);
                running = 0;
                break;
            default:
                printf("[!] Pilihan tidak valid. Masukkan 1-5.\n");
                break;
        }
    }
    /* caller (main.c) boleh memanggil lib_db_save(db) lagi jika ingin double-check saat exit */
}
