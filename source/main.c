#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "library.h"
#include "view.h"
#include "ui.h"
#include "peminjam.h"

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

    /* Initialize library database with default path */
    library_db_t *db;
    lib_status_t err;
    db = lib_db_open(NULL, &err);  /* NULL = use default path */
    if (!db || err != LIB_OK) {
        printf("[!] Gagal menginisialisasi database.\n");
        return 1;
    }

    /* Set fine policy (optional) */
    db->fine_per_day = LIB_DEFAULT_FINE_PER_DAY;

    int running = 1;
    while (running) {
        print_header("MENU UTAMA SISTEM PERPUSTAKAAN UKSW");
        printf("1. Login sebagai Admin\n");
        printf("2. Login sebagai Peminjam Buku\n");
        printf("3. Tampilkan Demo UI\n");
        printf("4. Keluar Program\n");
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

            case 2:
                if (login_peminjam()) {
                    menu_peminjam(db);
                } else {
                    printf("Login gagal.\n");
                    press_enter();
                }
                break;

            case 3:
                printf("\n=== DEMO UI ===\n");
                printf("\nContoh tampilan daftar buku dengan paging:\n");
                ui_render_book_list(0, 0, 80, 20, "Sample", 1, 0);

                printf("\nContoh tampilan detail buku:\n");
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