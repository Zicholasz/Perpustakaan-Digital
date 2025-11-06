#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "../include/library.h"

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

static void print_header(const char *title) {
    printf("\n================ %s ================\n", title ? title : "Menu");
}

int main(){
    int opt;
    print_header("SELAMAT DATANG DI PERPUSTAKAAN DIGITAL");

    /* initialize in-memory library data */
    library_init();

    printf("Anda masuk sebagai : \n1. Admin\n2. Peminjam\n3. Demo\n9. get the fuck out\nPilihan anda : ");
    /* read an integer choice safely */
    opt = read_int_choice();
    switch (opt)
    {
    case 1:
        admin_menu();
        break;
    case 2:
        login_peminjam();
        break;
    case 3:
        printf("\n=== DEMO UI ===\n");
                // Menampilkan daftar buku dengan ui_render_book_list
                printf("\nContoh tampilan daftar buku dengan paging:\n");
                ui_render_book_list(0, 0, 80, 20, "Sample", 1, 0);

                // Menampilkan detail buku dengan ui_show_book_detail
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
        break;
    default:
        break;
    }
    return 0;
}