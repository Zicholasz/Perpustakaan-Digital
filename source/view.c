/* top of view.c — portable platform glue (replace existing platform block with this) */
#include "view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  /* include the Windows console API (GetConsoleMode / SetConsoleMode / ENABLE_VIRTUAL_TERMINAL_PROCESSING) */
  #include <windows.h>
  #include <conio.h>   /* _kbhit, _getch */

  /* Some toolchains might not define ENABLE_VIRTUAL_TERMINAL_PROCESSING macro;
     provide a safe fallback value if missing. */
  #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
  #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
  #endif

  static void enable_ansi_on_windows(void) {
      HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
      if (hOut == INVALID_HANDLE_VALUE) return;
      DWORD dwMode = 0;
      if (!GetConsoleMode(hOut, &dwMode)) return;
      dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
      SetConsoleMode(hOut, dwMode);
  }

  #define sleep_ms(ms) Sleep(ms)

#else /* POSIX fallback */
  #include <unistd.h>
  #include <termios.h>
  #include <sys/select.h>
  #include <sys/ioctl.h>
  #include <fcntl.h>
  #define sleep_ms(ms) usleep((ms) * 1000)

  static void enable_ansi_on_windows(void) { /* no-op on POSIX */ }
#endif


/* ---------------- Configuration ---------------- */
#define DB_FILENAME "library_db.txt"
#define HISTORY_FILENAME "history.log"
#define TITLE_STR "SISTEM PERPUSTAKAAN UKSW"
#define ORN_LEFT "[📚]"
#define ORN_RIGHT "[✧]"

#define FRAME_MS 60
#define TITLE_FRAMES 8
#define PARTICLE_COUNT 60

/* ---------------- Internal state (static) ---------------- */
/* reuse book_t from library.h */
static book_t s_books[1024];
static int s_book_count = 0;

typedef struct {
    char when[64];
    char action[16];
    char isbn[LIB_MAX_ISBN];
    char title[LIB_MAX_TITLE];
} hist_t;
static hist_t s_history[4096];
static int s_history_count = 0;

/* particle for background */
typedef struct { int y,x,spd; char ch; } particle_t;
static particle_t s_particles[PARTICLE_COUNT];

static int s_cols = 80, s_lines = 24;
static int s_running = 0;

/* ------------------ Cross-platform helpers ------------------ */
static void enable_ansi_on_windows(void) {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
}

static void clearscreen(void) { printf("\x1b[2J\x1b[H"); }
static void move_cursor(int r, int c) { printf("\x1b[%d;%dH", r+1, c+1); }
static void hide_cursor(void) { printf("\x1b[?25l"); fflush(stdout); }
static void show_cursor(void) { printf("\x1b[?25h"); fflush(stdout); }
static void set_color(const char *ansi) { printf("%s", ansi); }
static void reset_color(void) { printf("\x1b[0m"); fflush(stdout); }

static void query_terminal_size(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h && GetConsoleScreenBufferInfo(h, &csbi)) {
        s_cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        s_lines = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
#else
    struct winsize w;
    if (ioctl(0, TIOCGWINSZ, &w) == 0) {
        s_cols = w.ws_col; s_lines = w.ws_row;
    }
#endif
    if (s_cols < 40) s_cols = 40;
    if (s_lines < 12) s_lines = 12;
}

/* Non-blocking single char read: returns -1 if none */
static int nb_getch(void) {
#ifdef _WIN32
    if (_kbhit()) {
        int c = _getch();
        return c;
    }
    return -1;
#else
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    if (select(1, &fds, NULL, NULL, &tv) > 0) {
        char ch;
        if (read(0, &ch, 1) == 1) return (unsigned char)ch;
    }
    return -1;
#endif
}

/* Blocking get char (with echo off on POSIX) */
static int getch_block(void) {
#ifdef _WIN32
    return _getch();
#else
    struct termios oldt, newt;
    tcgetattr(0, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &newt);
    int c = getchar();
    tcsetattr(0, TCSANOW, &oldt);
    return c;
#endif
}

/* print centered string on row */
static void print_center(int row, const char *s) {
    int len = (int)strlen(s);
    int col = (s_cols - len) / 2;
    if (col < 0) col = 0;
    move_cursor(row, col);
    printf("%s", s);
}

/* flush stdout */
static void flush_out(void) { fflush(stdout); }

/* ------------------ Persistence & history ------------------ */
static void add_history(const char *action, const book_t *b) {
    if (s_history_count >= (int)(sizeof(s_history)/sizeof(s_history[0]))) return;
    hist_t *h = &s_history[s_history_count++];
    time_t t = time(NULL);
    struct tm tmp;
#ifdef _WIN32
    localtime_s(&tmp, &t);
#else
    localtime_r(&t, &tmp);
#endif
    strftime(h->when, sizeof(h->when), "%Y-%m-%d %H:%M:%S", &tmp);
    strncpy(h->action, action, sizeof(h->action)-1);
    strncpy(h->isbn, b->isbn, sizeof(h->isbn)-1);
    strncpy(h->title, b->title, sizeof(h->title)-1);
    FILE *f = fopen(HISTORY_FILENAME, "a");
    if (f) {
        fprintf(f, "%s|%s|%s|%s\n", h->when, h->action, h->isbn, h->title);
        fclose(f);
    }
}

int ui_save_db(const char *filename) {
    const char *fn = filename ? filename : DB_FILENAME;
    FILE *f = fopen(fn, "w");
    if (!f) return -1;
    for (int i = 0; i < s_book_count; ++i) {
        fprintf(f, "%s|%s|%s|%d|%d|%s\n",
                s_books[i].isbn,
                s_books[i].title,
                s_books[i].author,
                s_books[i].year,
                s_books[i].available,
                s_books[i].notes);
    }
    fclose(f);
    return 0;
}

int ui_load_db(const char *filename) {
    const char *fn = filename ? filename : DB_FILENAME;
    FILE *f = fopen(fn, "r");
    if (!f) return -1;
    s_book_count = 0;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *p = strchr(line, '\n'); if (p) *p = '\0';
        char *tok = strtok(line, "|"); if (!tok) continue;
        strncpy(s_books[s_book_count].isbn, tok, sizeof(s_books[s_book_count].isbn)-1);
        tok = strtok(NULL, "|"); if (!tok) continue;
        strncpy(s_books[s_book_count].title, tok, sizeof(s_books[s_book_count].title)-1);
        tok = strtok(NULL, "|"); if (!tok) continue;
        strncpy(s_books[s_book_count].author, tok, sizeof(s_books[s_book_count].author)-1);
        tok = strtok(NULL, "|"); if (!tok) continue;
        s_books[s_book_count].year = atoi(tok);
        tok = strtok(NULL, "|"); if (!tok) continue;
        s_books[s_book_count].available = atoi(tok);
        tok = strtok(NULL, "|"); if (!tok) continue;
        strncpy(s_books[s_book_count].notes, tok, sizeof(s_books[s_book_count].notes)-1);
        s_book_count++;
        if (s_book_count >= (int)(sizeof(s_books)/sizeof(s_books[0]))) break;
    }
    fclose(f);
    /* load history (optional) */
    FILE *hf = fopen(HISTORY_FILENAME, "r");
    s_history_count = 0;
    if (hf) {
        char l2[1024];
        while (fgets(l2, sizeof(l2), hf)) {
            char *p = strchr(l2, '\n'); if (p) *p = '\0';
            char *t1 = strtok(l2, "|"); if (!t1) continue;
            char *t2 = strtok(NULL, "|"); if (!t2) continue;
            char *t3 = strtok(NULL, "|"); if (!t3) continue;
            char *t4 = strtok(NULL, "|"); if (!t4) continue;
            hist_t *h = &s_history[s_history_count++];
            strncpy(h->when, t1, sizeof(h->when)-1);
            strncpy(h->action, t2, sizeof(h->action)-1);
            strncpy(h->isbn, t3, sizeof(h->isbn)-1);
            strncpy(h->title, t4, sizeof(h->title)-1);
            if (s_history_count >= (int)(sizeof(s_history)/sizeof(s_history[0]))) break;
        }
        fclose(hf);
    }
    return 0;
}

/* ------------------ Demo helper ------------------ */
void ui_add_demo_book(const char *isbn, const char *title, const char *author, int year, int total_stock, int available) {
    if (s_book_count >= (int)(sizeof(s_books)/sizeof(s_books[0]))) return;
    strncpy(s_books[s_book_count].isbn, isbn, sizeof(s_books[s_book_count].isbn)-1);
    strncpy(s_books[s_book_count].title, title, sizeof(s_books[s_book_count].title)-1);
    strncpy(s_books[s_book_count].author, author, sizeof(s_books[s_book_count].author)-1);
    s_books[s_book_count].year = year;
    s_books[s_book_count].total_stock = total_stock;
    s_books[s_book_count].available = available;
    s_books[s_book_count].notes[0] = '\0';
    s_book_count++;
}

/* ------------------ Visual primitives ------------------ */
static void draw_frame_box(int top, int left, int h, int w) {
    for (int r = 0; r < h; ++r) {
        move_cursor(top + r, left);
        if (r == 0) printf("+"); else if (r == h-1) printf("+"); else printf("|");
        for (int c = 0; c < w-2; ++c) {
            if (r == 0 || r == h-1) printf("-");
            else printf(" ");
        }
        if (r == 0) printf("+"); else if (r == h-1) printf("+"); else printf("|");
    }
}

static int ci_strstr_index(const char *hay, const char *needle) {
    if (!needle || !*needle) return 0;
    size_t n = strlen(needle);
    for (size_t i=0; hay[i]; ++i) {
        if (strncasecmp(&hay[i], needle, n) == 0) return (int)i;
    }
    return -1;
}

static void draw_wavy_title(int frame) {
    int len = (int)strlen(TITLE_STR);
    int base_row = 1;
    int start_col = (s_cols - len) / 2;
    const char *colors[3] = {"\x1b[36m", "\x1b[35m", "\x1b[33m"};
    for (int i = 0; i < len; ++i) {
        int yoff = (int)(1.5 * sin((frame + i) * 0.35));
        int row = base_row + yoff;
        move_cursor(row, start_col + i);
        set_color(colors[(i+frame)%3]);
        printf("%c", TITLE_STR[i]);
        reset_color();
    }
    move_cursor(0,0);
}

/* particles */
static void init_particles(void) {
    srand((unsigned)time(NULL));
    for (int i=0;i<PARTICLE_COUNT;++i) {
        s_particles[i].y = rand() % s_lines;
        s_particles[i].x = rand() % s_cols;
        s_particles[i].spd = 1 + rand()%3;
        s_particles[i].ch = (s_particles[i].spd==3)?'*':(s_particles[i].spd==2?'+':'.');
    }
}
static void step_and_draw_particles(void) {
    for (int i=0;i<PARTICLE_COUNT;++i) {
        move_cursor(s_particles[i].y, s_particles[i].x);
        printf(" ");
        s_particles[i].y -= s_particles[i].spd;
        if (s_particles[i].y < 2) {
            s_particles[i].y = s_lines - 2;
            s_particles[i].x = rand() % s_cols;
            s_particles[i].spd = 1 + rand()%3;
            s_particles[i].ch = (s_particles[i].spd==3)?'*':(s_particles[i].spd==2?'+':'.');
        }
        move_cursor(s_particles[i].y, s_particles[i].x);
        const char *col = (s_particles[i].spd==3) ? "\x1b[37m" : (s_particles[i].spd==2? "\x1b[36m": "\x1b[90m");
        set_color(col);
        printf("%c", s_particles[i].ch);
        reset_color();
    }
}

/* draw book card */
static void draw_book_card(int top, int left, int w, const book_t *b, const char *query, int selected) {
    int h = 5;
    for (int r=0;r<h;++r) {
        move_cursor(top + r, left);
        if (r==0) printf("+"); else if (r==h-1) printf("+"); else printf("|");
        for (int c=0;c<w-2;++c) {
            if (r==0||r==h-1) printf("-");
            else printf(" ");
        }
        if (r==0) printf("+"); else if (r==h-1) printf("+"); else printf("|");
    }
    char small[LIB_MAX_TITLE];
    strncpy(small, b->title, sizeof(small)-1);
    small[sizeof(small)-1] = '\0';
    /* limit length visually */
    if ((int)strlen(small) > w-6) small[w-6] = '\0';
    int idx = (query && *query) ? ci_strstr_index(small, query) : -1;
    move_cursor(top+1, left+2);
    if (idx >= 0) {
        char prefix[LIB_MAX_TITLE]; strncpy(prefix, small, idx); prefix[idx]=0;
        printf("%s", prefix);
        set_color("\x1b[1;33m"); printf("%.*s", (int)strlen(query), &small[idx]); reset_color();
        printf("%s", &small[idx + (int)strlen(query)]);
    } else {
        printf("%s", small);
    }
    move_cursor(top+2, left+2);
    printf("%s - %d", b->author, b->year);
    move_cursor(top+3, left+2);
    if (b->available) set_color("\x1b[32m"); else set_color("\x1b[31m");
    printf("%s", b->available ? "[Tersedia]" : "[Dipinjam]");
    reset_color();
    if (selected) {
        /* simple marker */
        move_cursor(top, left + w - 3);
        set_color("\x1b[7m"); printf("   "); reset_color();
    }
}

/* ------------------ UI public routines ------------------ */
int ui_init(void) {
    enable_ansi_on_windows();
    query_terminal_size();
    clearscreen();
    hide_cursor();
    s_running = 1;
    init_particles();
    return 0;
}

void ui_shutdown(void) {
    s_running = 0;
    show_cursor();
    reset_color();
    clearscreen();
    flush_out();
}

/* render grid */
void ui_render_book_list(int top, int left, int width, int height, const char *query, int page, int selected_local_index) {
    int card_w = 28; if (card_w > width) card_w = width;
    int cols = width / card_w; if (cols < 1) cols = 1;
    int rows = height / 6; if (rows < 1) rows = 1;
    int per_page = rows * cols;
    int start = page * per_page;
    int idx = start;
    int local = 0;
    for (int r=0;r<rows;++r) {
        for (int c=0;c<cols;++c) {
            if (idx >= s_book_count) return;
            int x = left + c*card_w;
            int y = top + r*6;
            int sel = (local == selected_local_index);
            draw_book_card(y,x,card_w,&s_books[idx], query, sel);
            idx++; local++;
        }
    }
}

/* detail slide-in */
void ui_show_book_detail(const book_t *b) {
    if (!b) return;
    int w = s_cols * 2 / 3;
    int h = s_lines * 2 / 3;
    int tx = (s_cols - w)/2;
    int ty = (s_lines - h)/2;
    for (int off = s_cols; off >= tx; off -= 4) {
        clearscreen();
        draw_wavy_title((s_cols - off) % TITLE_FRAMES);
        step_and_draw_particles();
        draw_frame_box(ty, off, h, w);
        move_cursor(ty+2, off+3); printf("ISBN : %s", b->isbn);
        move_cursor(ty+3, off+3); printf("Judul: %s", b->title);
        move_cursor(ty+4, off+3); printf("Pengarang: %s", b->author);
        move_cursor(ty+5, off+3); printf("Tahun: %d", b->year);
        move_cursor(ty+7, off+3); printf("Status: %s", b->available ? "Tersedia" : "Dipinjam");
        flush_out();
        sleep_ms(30);
    }
    move_cursor(ty + h - 2, tx + 2); printf("Tekan tombol apapun untuk kembali...");
    flush_out();
    getch_block();
}

/* flip visual */
static void flip_card_visual(const char *label, const char *done_msg) {
    int w = (s_cols>60) ? 60 : (s_cols-4);
    int h = 7;
    int x = (s_cols - w)/2;
    int y = (s_lines - h)/2;
    for (int step = 0; step < 8; ++step) {
        clearscreen();
        draw_wavy_title(step);
        step_and_draw_particles();
        draw_frame_box(y, x + step, h, w - step*2);
        move_cursor(y+2, x+3 + step); printf("%s", label);
        flush_out();
        sleep_ms(70);
    }
    move_cursor(y+3, x+3); printf("%s", done_msg);
    move_cursor(y+5, x+3); printf("Tekan tombol apapun...");
    flush_out();
    getch_block();
}

/* borrow/return */
int ui_borrow_book_by_index(int global_index) {
    if (global_index < 0 || global_index >= s_book_count) return -1;
    if (!s_books[global_index].available) return -1;
    flip_card_visual("Meminjam buku...", "Meminjam selesai!");
    s_books[global_index].available = 0;
    add_history("BORROW", &s_books[global_index]);
    ui_save_db(DB_FILENAME);
    return 0;
}
int ui_return_book_by_index(int global_index) {
    if (global_index < 0 || global_index >= s_book_count) return -1;
    if (s_books[global_index].available) return -1;
    flip_card_visual("Mengembalikan buku...", "Pengembalian selesai!");
    s_books[global_index].available = 1;
    add_history("RETURN", &s_books[global_index]);
    ui_save_db(DB_FILENAME);
    return 0;
}

/* history timeline */
void ui_show_history_timeline(void) {
    int w = s_cols*2/3; if (w < 40) w = 40;
    int h = s_lines*2/3; if (h < 10) h = 10;
    int x = (s_cols - w)/2; int y = (s_lines - h)/2;
    for (int i = 0; i < s_history_count && i < h-4; ++i) {
        int idx = s_history_count - 1 - i;
        char line[512];
        snprintf(line, sizeof(line), "%s | %s | %s - %s", s_history[idx].when, s_history[idx].action, s_history[idx].isbn, s_history[idx].title);
        for (int off = s_cols; off >= x+2; off -= 6) {
            clearscreen();
            draw_wavy_title(i % TITLE_FRAMES);
            step_and_draw_particles();
            draw_frame_box(y, x, h, w);
            move_cursor(y+1, x+2); printf("Riwayat (Terbaru diatas)");
            for (int j=0;j<i;++j) {
                int idx2 = s_history_count - 1 - j;
                move_cursor(y+3 + j, x+2); printf("%s", s_history[idx2].when);
                move_cursor(y+3 + j, x+24); printf("%s", s_history[idx2].action);
            }
            move_cursor(y+3 + i, off); printf("%s", line);
            flush_out();
            sleep_ms(25);
        }
    }
    move_cursor(y + h - 2, x+2); printf("Tekan tombol apapun untuk kembali...");
    flush_out();
    getch_block();
}

/* autocomplete input (blocking) */
int ui_input_autocomplete(char *buffer, size_t bufsize) {
    if (!buffer || bufsize == 0) return -1;
    buffer[0] = '\0';
    size_t pos = 0;
    while (1) {
        clearscreen();
        draw_wavy_title(0);
        step_and_draw_particles();
        print_center(4, "Masukkan kata pencarian (ENTER konfirmasi, ESC batal, TAB autocomplete):");
        move_cursor(6, (s_cols - 40)/2);
        printf("> %s", buffer);
        int shown = 0;
        for (int i=0;i<s_book_count && shown < 5;++i) {
            if (pos == 0 || (strncasecmp(s_books[i].title, buffer, pos) == 0)) {
                move_cursor(8 + shown, (s_cols - 60)/2);
                printf("- %s", s_books[i].title);
                shown++;
            }
        }
        flush_out();
        int c = nb_getch();
        if (c == -1) { sleep_ms(FRAME_MS); continue; }
        if (c == 27) return -1;
        if (c == '\r' || c == '\n') return 0;
        if (c == '\t') {
            for (int i=0;i<s_book_count;++i) {
                if (pos == 0 || (strncasecmp(s_books[i].title, buffer, pos) == 0)) {
                    strncpy(buffer, s_books[i].title, bufsize-1);
                    buffer[bufsize-1]=0; pos = strlen(buffer);
                    break;
                }
            }
            continue;
        }
        if (c == 127 || c == 8) { if (pos>0) { buffer[--pos]='\0'; } continue; }
        if (c >= 32 && c < 127 && pos < bufsize-1) {
            buffer[pos++] = (char)c;
            buffer[pos] = '\0';
            continue;
        }
    }
    return -1;
}

/* main interactive loop (blocking) */
void ui_demo_mainloop(void) {
    int page = 0;
    int selected_local = 0;
    char query[128] = "";

    while (1) {
        query_terminal_size();
        clearscreen();
        draw_wavy_title((int)time(NULL) % TITLE_FRAMES);
        step_and_draw_particles();

        int panel_w = s_cols/3;
        draw_frame_box(3,2,10,panel_w);
        move_cursor(4,4); printf("Welcome, Nico!");
        move_cursor(6,4); printf("Books stored: %d", s_book_count);
        move_cursor(8,4); printf("Commands: s=search n/p pages b/r borrow/return h=history q=quit");
        move_cursor(9,4); printf("Use arrows to navigate, ENTER detail");

        int shelf_left = panel_w + 6;
        int shelf_w = s_cols - shelf_left - 4;
        int shelf_h = s_lines - 8;
        ui_render_book_list(3, shelf_left, shelf_w, shelf_h, (query[0]?query:NULL), page, selected_local);

        move_cursor(s_lines-2,2);
        printf("Page %d | Selected (local) %d | Query: %s", page+1, selected_local, query);
        flush_out();

        int ch = nb_getch();
        if (ch == -1) { sleep_ms(FRAME_MS); continue; }
        if (ch == 'q' || ch == 'Q') break;
        else if (ch == 's' || ch == 'S') {
            if (ui_input_autocomplete(query, sizeof(query)) == 0 && query[0]) {
                int found = -1;
                for (int i=0;i<s_book_count;++i) if (ci_strstr_index(s_books[i].title, query) >= 0) { found = i; break;}
                if (found >= 0) {
                    int card_w = 28;
                    int cols = shelf_w / card_w; if (cols < 1) cols = 1;
                    int rows = shelf_h / 6; if (rows < 1) rows = 1;
                    int per = rows*cols;
                    page = found / per;
                    selected_local = found % per;
                    ui_show_book_detail(&s_books[found]);
                }
            }
        } else if (ch == 'n' || ch == 'N') { page++; }
        else if (ch == 'p' || ch == 'P') { if (page>0) page--; }
        else if (ch == 'h' || ch == 'H') { ui_show_history_timeline(); }
        else if (ch == '\r' || ch == '\n') {
            int card_w = 28;
            int cols = shelf_w / card_w; if (cols < 1) cols = 1;
            int rows = shelf_h / 6; if (rows < 1) rows = 1;
            int per = rows*cols;
            int idx = page*per + selected_local;
            if (idx >=0 && idx < s_book_count) ui_show_book_detail(&s_books[idx]);
        } else if (ch == 'b' || ch == 'B') {
            int card_w = 28;
            int cols = shelf_w / card_w; if (cols < 1) cols = 1;
            int rows = shelf_h / 6; if (rows < 1) rows = 1;
            int per = rows*cols;
            int idx = page*per + selected_local;
            ui_borrow_book_by_index(idx);
        } else if (ch == 'r' || ch == 'R') {
            int card_w = 28;
            int cols = shelf_w / card_w; if (cols < 1) cols = 1;
            int rows = shelf_h / 6; if (rows < 1) rows = 1;
            int per = rows*cols;
            int idx = page*per + selected_local;
            ui_return_book_by_index(idx);
        } else {
            if (ch == 27) { /* ignore */ }
            else {
                /* minimal arrow handling for POSIX (may be sequences) */
                if (ch == 'A' || ch == 65) { /* up */ selected_local -= (shelf_w/28); if (selected_local<0) selected_local=0; }
                else if (ch == 'B' || ch == 66) { /* down */ selected_local += (shelf_w/28); }
                else if (ch == 'C' || ch == 67) { /* right */ selected_local++; }
                else if (ch == 'D' || ch == 68) { /* left */ if (selected_local>0) selected_local--; }
            }
        }
        /* clamp selection/page */
        if (selected_local < 0) selected_local = 0;
        {
            int card_w = 28;
            int cols = shelf_w / card_w; if (cols < 1) cols = 1;
            int rows = shelf_h / 6; if (rows < 1) rows = 1;
            int per = rows*cols;
            int max_page = (s_book_count + per - 1) / per;
            if (page >= max_page) page = max_page - 1;
        }
        sleep_ms(FRAME_MS);
    }
}

/* End of file view.c */
