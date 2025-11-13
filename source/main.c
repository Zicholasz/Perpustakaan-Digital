#include <stdio.h>
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
            printf("4. Ubah Tema / Background\n");
            printf("5. Keluar Program\n");
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

                case 4: {
                    /* Theme/background chooser: present a few 256-color options */
                    printf("\n=== Pilih Tema Background ===\n\n");
                    printf("1. Default (reset)\n");
                    printf("2. Soft parchment (brown)\n");
                    printf("3. Midnight (dark blue)\n");
                    printf("4. Forest (dark green)\n\n");
                    printf("Pilihan : ");
                    int t = read_int_choice();
                    switch (t) {
                        case 2: { int code = 94; animation_set_bg_color(code); ui_save_theme(code); break; }
                        case 3: { int code = 17; animation_set_bg_color(code); ui_save_theme(code); break; }
                        case 4: { int code = 22; animation_set_bg_color(code); ui_save_theme(code); break; }
                        default: { int code = 0; animation_set_bg_color(code); ui_save_theme(code); break; }
                    }
                    animation_delay(200);
                    ui_clear_screen();
                    printf("Tema berhasil diubah. Jika warna tidak tampil, pastikan terminal mendukung ANSI 256-colors.\n\n");
                    press_enter();
                    break;
                }

            case 5:
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