#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "../include/library.h"
#include "../include/view.h"
#include "../include/ui.h"
#include "../include/peminjam.h"
#include "../include/animation.h"

/* ---------- Helper: safe input ---------- */
/* read_line: baca line dari stdin ke buffer, trim newline.
 * - Mengembalikan pointer ke buffer (sama seperti buf), atau NULL pada EOF/error.
 * - Gunakan ini untuk menggantikan scanf agar lebih aman.
 */
static char *input_line(char *buf, size_t size) {
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
    if (!input_line(tmp, sizeof(tmp))) return -1;
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

static void print_header(const char *title) {
    printf("\n================ %s ================\n", title ? title : "Menu");
}

int main(void) {
    /* Initialize UI */
    ui_init();

    /* Always show the animation demo on startup per user preference (option B). */
    animation_run_demo();
    /* Initialize library database with default path */
    library_db_t *db;
    lib_status_t err;
    /* Ensure sample data (books, admin) exists so peminjam can view books. Do this
       before opening the main DB instance so the newly created files are read. */
    ensure_sample_data();

    db = lib_db_open(NULL, &err);  /* NULL = use default path */
    if (!db || err != LIB_OK) {
        printf("[!] Gagal menginisialisasi database.\n");
        return 1;
    }

    /* Set fine policy (optional) */
    db->fine_per_day = LIB_DEFAULT_FINE_PER_DAY;

    int running = 1;
    while (running) {
        /* Clear any leftover animation output before rendering main menu */
        ui_clear_screen();
        print_header("MENU UTAMA SISTEM PERPUSTAKAAN UKSW");
        printf("1. Login sebagai Admin\n");
        printf("2. Login sebagai Peminjam Buku\n");
            printf("3. Tampilkan Demo UI\n");
            printf("4. Panduan Penggunaan Program\n");
            printf("5. Credit Pengembang\n");
            printf("6. Keluar Program\n");
        printf("Pilih menu: ");

        int choice = read_int_choice();
        switch (choice) {
            case 1:
                if (login_admin()) {
                    menu_admin(db);
                } else {
                    printf("Login gagal.\n");
                    press_enter();
                }
                break;

            case 2: {
                int lp = login_peminjam();
                if (lp == 1) {
                    menu_peminjam(db);
                } else if (lp == 2) {
                    /* Special: peminjam prompt allowed admin escalation */
                    printf("Mengalihkan ke menu Admin...\n");
                    menu_admin(db);
                } else {
                    printf("Login gagal.\n");
                    press_enter();
                }
                break;
            }

            case 3:
                printf("\n=== DEMO UI ===\n");
                animation_typewriter("[Demo] Menampilkan contoh tampilan...", 25);
                animation_delay(300);
                printf("\n--- Contoh Tampilan Daftar Buku ---\n\n");
                printf("%-15s | %-28s | %-18s | %-10s | %-6s | %s\n",
                       "ISBN", "Judul", "Pengarang", "Harga", "Total", "Tersedia");
                printf("%-15s-+-%-28s-+-%-18s-+-%-10s-+-%-6s-+-%s\n",
                       "===============", "============================", "==================", "==========", "======", "==========");
                printf("%-15s | %-28s | %-18s | Rp%8.2f | %-6d | %d\n",
                       "9781234567897", "Demo Book", "Demo Author", 50000.00, 5, 3);
                printf("\n--- Contoh Tampilan Detail Buku ---\n");
                book_t sample_book = {
                    .isbn = "9781234567897",
                    .title = "Demo Book",
                    .author = "Demo Author",
                    .year = 2023,
                    .total_stock = 5,
                    .available = 3,
                    .notes = "Demo book for UI testing"
                };
                ui_show_book_detail(&sample_book);
                press_enter();
                break;

            case 4:
                ui_clear_screen();
                printf("\n=== PANDUAN PENGGUNAAN PROGRAM ===\n\n");
                animation_typewriter("Selamat datang di Sistem Perpustakaan Digital UKSW!", 30);
                animation_delay(500);
                printf("\n\n");
                animation_typewriter("Panduan Lengkap untuk Penggunaan Sistem", 25);
                printf("\n\n");

                animation_typewriter("1. LOGIN DAN AUTENTIKASI", 20);
                printf("\n");
                animation_typewriter("- Admin: Gunakan username 'Kakak Admin' dan password '1234'", 15);
                printf("\n");
                animation_typewriter("- Peminjam: Login dengan NIM yang valid (format: 672025XXX)", 15);
                printf("\n\n");

                animation_typewriter("2. FITUR UNTUK ADMIN", 20);
                printf("\n");
                animation_typewriter("- Lihat Daftar Buku: Menampilkan semua buku yang tersedia", 15);
                printf("\n");
                animation_typewriter("- Tambah Buku: Menambahkan buku baru dengan ISBN, judul, pengarang, tahun, harga", 15);
                printf("\n");
                animation_typewriter("- Hapus Buku: Menghapus buku berdasarkan ISBN", 15);
                printf("\n");
                animation_typewriter("- Update Stok Buku: Mengubah jumlah stok buku yang tersedia", 15);
                printf("\n");
                animation_typewriter("- Cari Buku: Mencari buku berdasarkan judul", 15);
                printf("\n");
                animation_typewriter("- Lihat History Peminjaman: Melihat semua riwayat peminjaman", 15);
                printf("\n");
                animation_typewriter("- Tandai Buku Hilang: Menandai pinjaman terlambat sebagai hilang", 15);
                printf("\n");
                animation_typewriter("- Pengaturan: Mengubah denda per hari dan kebijakan penggantian", 15);
                printf("\n\n");

                animation_typewriter("3. FITUR UNTUK PEMINJAM", 20);
                printf("\n");
                animation_typewriter("- Lihat Daftar Buku: Melihat semua buku yang tersedia untuk dipinjam", 15);
                printf("\n");
                animation_typewriter("- Cari Buku: Mencari buku berdasarkan judul atau ISBN", 15);
                printf("\n");
                animation_typewriter("- Pinjam Buku: Meminjam buku dengan batas waktu 7 hari", 15);
                printf("\n");
                animation_typewriter("- Lihat Pinjaman Saya: Melihat daftar buku yang sedang dipinjam", 15);
                printf("\n");
                animation_typewriter("- Kembalikan Buku: Mengembalikan buku dan membayar denda jika terlambat", 15);
                printf("\n");
                animation_typewriter("- Lapor Buku Hilang: Melaporkan buku yang hilang dan membayar biaya penggantian", 15);
                printf("\n");
                animation_typewriter("- Lihat History: Melihat riwayat peminjaman pribadi", 15);
                printf("\n\n");

                animation_typewriter("4. FITUR TAMBAHAN", 20);
                printf("\n");
                animation_typewriter("- Demo UI: Menampilkan contoh tampilan antarmuka sistem", 15);
                printf("\n");
                animation_typewriter("- Panduan Penggunaan: Menampilkan panduan lengkap ini", 15);
                printf("\n");
                animation_typewriter("- Credit Pengembang: Menampilkan informasi pengembang dan terima kasih", 15);
                printf("\n\n");

                animation_typewriter("5. KETENTUAN DAN KEBIJAKAN", 20);
                printf("\n");
                animation_typewriter("- Denda keterlambatan: Rp1.000 per hari", 15);
                printf("\n");
                animation_typewriter("- Batas waktu pinjaman: 7 hari dari tanggal peminjaman", 15);
                printf("\n");
                animation_typewriter("- Buku hilang: Dikenakan biaya penggantian sesuai harga buku", 15);
                printf("\n");
                animation_typewriter("- Maksimal 1 bulan terlambat sebelum dianggap buku hilang", 15);
                printf("\n\n");

                animation_typewriter("Tekan Enter untuk kembali ke menu utama...", 20);
                press_enter();
                break;

            case 5:
                ui_clear_screen();
                printf("\n=== CREDIT PENGEMBANG ===\n\n");
                animation_typewriter("SISTEM PERPUSTAKAAN DIGITAL UKSW", 30);
                printf("\n\n");
                animation_typewriter("Dikembangkan dengan sepenuh hati oleh Mahasiswa UKSW", 25);
                printf("\n\n");

                animation_typewriter("DOSEN PEMBIMBING:", 20);
                printf("\n");
                animation_typewriter("AFIYATAR ASYER", 15);
                printf("\n");
                animation_typewriter("Terima kasih atas bimbingan dan dukungannya!", 15);
                printf("\n\n");

                animation_typewriter("TIM PENGEMBANG:", 20);
                printf("\n");
                animation_typewriter("Nicholas Karsono (672025037)", 15);
                printf("\n");
                animation_typewriter("Clemens Vencentio Widjojo (672025044)", 15);
                printf("\n");
                animation_typewriter("Elia Hans Hayudyo Purwanto (672025047)", 15);
                printf("\n");
                animation_typewriter("Augusta Nayra Naftali (672025055)", 15);
                printf("\n\n");

                animation_typewriter("TEKNOLOGI & PLATFORM:", 20);
                printf("\n");
                animation_typewriter("Bahasa Pemrograman: C (ISO C99)", 15);
                printf("\n");
                animation_typewriter("Platform: Windows dengan ANSI Escape Codes", 15);
                printf("\n");
                animation_typewriter("Interface: Command Line dengan UI Interaktif", 15);
                printf("\n\n");

                animation_typewriter("TERIMA KASIH KEPADA:", 20);
                printf("\n");
                animation_typewriter("- Dosen Pembimbing yang luar biasa", 15);
                printf("\n");
                animation_typewriter("- Asisten Dosen yang telah membantu", 15);
                printf("\n");
                animation_typewriter("- UKSW Salatiga yang memberikan inspirasi", 15);
                printf("\n");
                animation_typewriter("- Semua pihak yang mendukung pengembangan proyek ini", 15);
                printf("\n\n");

                animation_typewriter("INFORMASI PROYEK:", 20);
                printf("\n");
                animation_typewriter("Versi 1.0", 15);
                printf("\n");
                animation_typewriter("Tahun Rilis 2025", 15);
                printf("\n\n");

                animation_typewriter("Terima kasih telah menggunakan sistem kami!", 20);
                printf("\n");
                animation_typewriter("Tekan Enter untuk kembali ke menu utama...", 20);
                press_enter();
                break;

            case 6:
                printf("Terima kasih telah menggunakan Sistem Perpustakaan UKSW!\n");
                running = 0;
                break;

            default:
                printf("[!] Pilihan tidak valid!\n");
                press_enter();
                break;
        }
    }

    /* Cleanup */
    lib_db_close(db);
    ui_shutdown();
    return 0;
}