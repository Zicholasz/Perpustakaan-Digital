/* library.c
 *
 * Implementasi lengkap untuk library.h
 * - Portable: compatibility layer untuk Windows (getline, gmtime, timegm)
 * - Atomic save: lib_db_save menulis ke .tmp lalu rename menjadi file final
 * - Semua API sesuai library.h (book/borrower/loan/admin/persistence)
 *
 * Standard: ISO C99
 */

#define _CRT_SECURE_NO_WARNINGS
#define _POSIX_C_SOURCE 200809L

#include <string.h>
#include "../include/library.h"

/* Our own strdup implementation */
static char *my_strdup(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char *new_str = malloc(len);
    if (new_str) {
        memcpy(new_str, str, len);
    }
    return new_str;
}

#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>
#include <stdarg.h>
#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#endif

/* strtok_r compatibility for Windows / non-POSIX */
#if !defined(_GNU_SOURCE) && !defined(_POSIX_VERSION)
/* Provide a fallback implementation mapping to strtok_s on Windows or strtok otherwise */
#ifndef HAVE_STRTOK_R
#if defined(_WIN32) || defined(_WIN64)
#include <string.h>
char *strtok_r(char *s, const char *delim, char **saveptr) {
    /* strtok_s has different signature in MSVC; map accordingly */
#if defined(_MSC_VER)
    return strtok_s(s, delim, saveptr);
#else
    /* MinGW may not provide strtok_s; fallback to strtok (not thread-safe)
       but this project is single-threaded so it's acceptable as a fallback. */
    (void)saveptr;
    return strtok(s, delim);
#endif
}
#endif
#endif
#endif

/* Platform includes for compatibility */
#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
  #if defined(_MSC_VER)
    #include <BaseTsd.h>
    typedef SSIZE_T ssize_t;
  #endif
#endif

/* ---------- compatibility: getline fallback ---------- */
#if !defined(_POSIX_VERSION) && !defined(HAVE_GETLINE)
static ssize_t portable_getline(char **lineptr, size_t *n, FILE *stream) {
    if (!lineptr || !n || !stream) return -1;
    char *buf = *lineptr;
    size_t cap = *n;
    int c;
    size_t len = 0;
    if (buf == NULL || cap == 0) {
        cap = 256;
        buf = malloc(cap);
        if (!buf) return -1;
    }
    while ((c = fgetc(stream)) != EOF) {
        if (len + 1 >= cap) {
            size_t newcap = cap * 2;
            char *t = realloc(buf, newcap);
            if (!t) { free(buf); return -1; }
            buf = t;
            cap = newcap;
        }
        buf[len++] = (char)c;
        if (c == '\n') break;
    }
    if (len == 0 && c == EOF) return -1;
    buf[len] = '\0';
    *lineptr = buf;
    *n = cap;
    return (ssize_t)len;
}
#define getline(p,q,r) portable_getline(p,q,r)
#endif

/* portable gmtime_r/gmtime_s wrapper */
static void tm_from_time(time_t t, struct tm *out_tm) {
#if defined(_WIN32) || defined(_WIN64)
    #if defined(_MSC_VER)
        gmtime_s(out_tm, &t);
    #else
        struct tm *tmp = gmtime(&t);
        if (tmp) *out_tm = *tmp;
        else memset(out_tm, 0, sizeof(*out_tm));
    #endif
#else
    gmtime_r(&t, out_tm);
#endif
}

/* portable timegm */
static time_t timegm_portable(struct tm *tm) {
#if defined(_WIN32) || defined(_WIN64)
    #if defined(_MSC_VER)
        return _mkgmtime(tm);
    #else
        #ifdef _mkgmtime
            return _mkgmtime(tm);
        #else
            return mktime(tm);
        #endif
    #endif
#else
    #ifdef HAVE_TIMEGM
        return timegm(tm);
    #else
        return mktime(tm);
    #endif
#endif
}

/* ---------- internal helpers ---------- */

#define INITIAL_CAPACITY LIB_DEFAULT_CAPACITY
static unsigned long _id_counter = 0;

/* Default replacement-cost multiplier (days) used when DB doesn't override it. */
#define LIB_REPLACEMENT_COST_DAYS_DEFAULT 30UL

static void trim_newline(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r' || s[len-1] == ' ' || s[len-1] == '\t')) {
        s[len-1] = '\0';
        len--;
    }
}

/* Ensure the directory portion of `path` exists. Returns 0 on success or
   if no directory part (path is just a filename), non-zero on failure. */
static int ensure_dir_for_path(const char *path) {
    if (!path) return -1;
    const char *sep = strrchr(path, '/');
    if (!sep) sep = strrchr(path, '\\');
    if (!sep) return 0; /* no directory part */
    size_t len = (size_t)(sep - path);
    if (len == 0) return 0;
    char tmp[1024];
    if (len >= sizeof(tmp)) return -1;
    memcpy(tmp, path, len);
    tmp[len] = '\0';
#if defined(_WIN32) || defined(_WIN64)
    if (_mkdir(tmp) == 0) return 0;
    if (errno == EEXIST) return 0;
    return -1;
#else
    if (mkdir(tmp, 0755) == 0) return 0;
    if (errno == EEXIST) return 0;
    return -1;
#endif
}

static void str_to_lower_inplace(char *s) {
    if (!s) return;
    for (; *s; ++s) *s = (char) tolower((unsigned char)*s);
}

static int contains_case_insensitive(const char *haystack, const char *needle) {
    if (!haystack || !needle) return 0;
    size_t hlen = strlen(haystack), nlen = strlen(needle);
    if (nlen == 0) return 1;
    char *hl = malloc(hlen + 1);
    char *nl = malloc(nlen + 1);
    if (!hl || !nl) { free(hl); free(nl); return 0; }
    strcpy(hl, haystack); strcpy(nl, needle);
    str_to_lower_inplace(hl); str_to_lower_inplace(nl);
    int res = (strstr(hl, nl) != NULL);
    free(hl); free(nl);
    return res;
}

static void generate_unique_id(const char *prefix, char *out, size_t out_sz) {
    time_t t = time(NULL);
    _id_counter++;
    unsigned long r = (unsigned long)rand();
    if (!prefix) prefix = "ID";
    snprintf(out, out_sz, "%s%lu%lu", prefix, (unsigned long)t, (unsigned long)(r ^ _id_counter));
}

/* ---------- Date helpers (public API implemented) ---------- */

lib_date_t lib_date_from_time_t(time_t t) {
    struct tm tm;
    tm_from_time(t, &tm);
    lib_date_t d = { tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday };
    return d;
}
time_t lib_time_t_from_date(lib_date_t d) {
    struct tm tm = {0};
    tm.tm_year = d.year - 1900;
    tm.tm_mon = d.month - 1;
    tm.tm_mday = d.day;
    tm.tm_hour = 0; tm.tm_min = 0; tm.tm_sec = 0;
    return timegm_portable(&tm);
}
int lib_date_days_between(lib_date_t a, lib_date_t b) {
    time_t ta = lib_time_t_from_date(a);
    time_t tb = lib_time_t_from_date(b);
    if (ta == (time_t)-1 || tb == (time_t)-1) return 0;
    double diff = difftime(tb, ta);
    return (int)(diff / 86400.0);
}

/* ---------- Dynamic capacity helpers ---------- */

static lib_status_t ensure_books_capacity(library_db_t *db) {
    if (!db) return LIB_ERR_INVALID_ARG;
    if (db->books_capacity == 0) {
        db->books_capacity = INITIAL_CAPACITY;
        db->books = calloc(db->books_capacity, sizeof(book_t));
        if (!db->books) return LIB_ERR_MEMORY;
    } else if (db->books_count >= db->books_capacity) {
        size_t newcap = db->books_capacity * 2;
        book_t *tmp = realloc(db->books, newcap * sizeof(book_t));
        if (!tmp) return LIB_ERR_MEMORY;
        db->books = tmp;
        db->books_capacity = newcap;
    }
    return LIB_OK;
}

static lib_status_t ensure_borrowers_capacity(library_db_t *db) {
    if (!db) return LIB_ERR_INVALID_ARG;
    if (db->borrowers_capacity == 0) {
        db->borrowers_capacity = INITIAL_CAPACITY;
        db->borrowers = calloc(db->borrowers_capacity, sizeof(borrower_t));
        if (!db->borrowers) return LIB_ERR_MEMORY;
    } else if (db->borrowers_count >= db->borrowers_capacity) {
        size_t newcap = db->borrowers_capacity * 2;
        borrower_t *tmp = realloc(db->borrowers, newcap * sizeof(borrower_t));
        if (!tmp) return LIB_ERR_MEMORY;
        db->borrowers = tmp;
        db->borrowers_capacity = newcap;
    }
    return LIB_OK;
}

static lib_status_t ensure_loans_capacity(library_db_t *db) {
    if (!db) return LIB_ERR_INVALID_ARG;
    if (db->loans_capacity == 0) {
        db->loans_capacity = INITIAL_CAPACITY;
        db->loans = calloc(db->loans_capacity, sizeof(loan_t));
        if (!db->loans) return LIB_ERR_MEMORY;
    } else if (db->loans_count >= db->loans_capacity) {
        size_t newcap = db->loans_capacity * 2;
        loan_t *tmp = realloc(db->loans, newcap * sizeof(loan_t));
        if (!tmp) return LIB_ERR_MEMORY;
        db->loans = tmp;
        db->loans_capacity = newcap;
    }
    return LIB_OK;
}

/* ---------- Path helper ---------- */

static char *alloc_path_with_suffix(const char *base, const char *suffix) {
    size_t len = strlen(base) + strlen(suffix) + 2;
    char *buf = malloc(len);
    if (!buf) return NULL;
    snprintf(buf, len, "%s%s", base, suffix);
    return buf;
}

/* forward declaration for meta-file reader used during DB open */
static void read_meta_file(library_db_t *db);

/* ---------- CSV writers to explicit path (used by atomic save) ---------- */

static lib_status_t write_books_csv_to(const library_db_t *db, const char *outfile) {
    if (!db || !outfile) return LIB_ERR_INVALID_ARG;
    /* ensure directory exists before attempting to open file */
    if (ensure_dir_for_path(outfile) != 0) {
        fprintf(stderr, "[lib] write_books_csv_to: cannot ensure directory for '%s'\n", outfile);
        return LIB_ERR_IO;
    }
    FILE *f = fopen(outfile, "w");
    if (!f) {
        fprintf(stderr, "[lib] write_books_csv_to: fopen('%s') failed: %s\n", outfile, strerror(errno));
        return LIB_ERR_IO;
    }
    if (fprintf(f, "isbn,title,author,year,total_stock,available,price,notes\n") < 0) { fclose(f); return LIB_ERR_IO; }
    for (size_t i = 0; i < db->books_count; ++i) {
        const book_t *b = &db->books[i];
        if (fprintf(f, "%s,%s,%s,%d,%d,%d,%.2f,%s\n",
                b->isbn, b->title, b->author, b->year, b->total_stock, b->available, b->price, b->notes) < 0) {
            fclose(f); return LIB_ERR_IO;
        }
    }
    fflush(f);
#if !defined(_WIN32) && !defined(_WIN64)
    fsync(fileno(f));
#endif
    fclose(f);
    return LIB_OK;
}

static lib_status_t write_borrowers_csv_to(const library_db_t *db, const char *outfile) {
    if (!db || !outfile) return LIB_ERR_INVALID_ARG;
    if (ensure_dir_for_path(outfile) != 0) {
        fprintf(stderr, "[lib] write_borrowers_csv_to: cannot ensure directory for '%s'\n", outfile);
        return LIB_ERR_IO;
    }
    FILE *f = fopen(outfile, "w");
    if (!f) {
        fprintf(stderr, "[lib] write_borrowers_csv_to: fopen('%s') failed: %s\n", outfile, strerror(errno));
        return LIB_ERR_IO;
    }
    if (fprintf(f, "id,nim,name,phone,email\n") < 0) { fclose(f); return LIB_ERR_IO; }
    for (size_t i = 0; i < db->borrowers_count; ++i) {
        const borrower_t *br = &db->borrowers[i];
        if (fprintf(f, "%s,%s,%s,%s,%s\n", br->id, br->nim, br->name, br->phone, br->email) < 0) {
            fclose(f); return LIB_ERR_IO;
        }
    }
    fflush(f);
#if !defined(_WIN32) && !defined(_WIN64)
    fsync(fileno(f));
#endif
    fclose(f);
    return LIB_OK;
}

static lib_status_t write_loans_csv_to(const library_db_t *db, const char *outfile) {
    if (!db || !outfile) return LIB_ERR_INVALID_ARG;
    if (ensure_dir_for_path(outfile) != 0) {
        fprintf(stderr, "[lib] write_loans_csv_to: cannot ensure directory for '%s'\n", outfile);
        return LIB_ERR_IO;
    }
    FILE *f = fopen(outfile, "w");
    if (!f) {
        fprintf(stderr, "[lib] write_loans_csv_to: fopen('%s') failed: %s\n", outfile, strerror(errno));
        return LIB_ERR_IO;
    }
    if (fprintf(f, "loan_id,isbn,borrower_id,date_borrow,date_due,date_returned,is_returned,is_lost,fine_paid\n") < 0) { fclose(f); return LIB_ERR_IO; }
    for (size_t i = 0; i < db->loans_count; ++i) {
        const loan_t *l = &db->loans[i];
        char db1[16] = "", db2[16] = "", db3[16] = "";
        snprintf(db1, sizeof(db1), "%04d-%02d-%02d", l->date_borrow.year, l->date_borrow.month, l->date_borrow.day);
        snprintf(db2, sizeof(db2), "%04d-%02d-%02d", l->date_due.year, l->date_due.month, l->date_due.day);
        if (l->is_returned) snprintf(db3, sizeof(db3), "%04d-%02d-%02d", l->date_returned.year, l->date_returned.month, l->date_returned.day);
        if (fprintf(f, "%s,%s,%s,%s,%s,%s,%d,%d,%ld\n",
                l->loan_id, l->isbn, l->borrower_id, db1, db2, db3, l->is_returned ? 1 : 0, l->is_lost ? 1 : 0, (long)l->fine_paid) < 0) {
            fclose(f); return LIB_ERR_IO;
        }
    }
    fflush(f);
#if !defined(_WIN32) && !defined(_WIN64)
    fsync(fileno(f));
#endif
    fclose(f);
    return LIB_OK;
}

/* ---------- CSV readers (full) ---------- */

static lib_status_t read_books_csv(library_db_t *db, const char *path) {
    if (!db || !path) return LIB_ERR_INVALID_ARG;
    char *p = alloc_path_with_suffix(path, "_books.csv");
    if (!p) return LIB_ERR_MEMORY;
    FILE *f = fopen(p, "r");
    if (!f) { free(p); return LIB_OK; } /* missing file -> empty */
    char *line = NULL; size_t len = 0; ssize_t r;
    bool skip_header = false;
    while ((r = getline(&line, &len, f)) != -1) {
        trim_newline(line);
        if (!skip_header) { skip_header = true; continue; }
        if (strlen(line) == 0) continue;
        char *s = my_strdup(line);
        char *ctx = NULL;
        char *tok = strtok_r(s, ",", &ctx);
        if (!tok) { free(s); continue; }
    book_t b; memset(&b,0,sizeof(b));
    strncpy(b.isbn, tok, LIB_MAX_ISBN-1);
    tok = strtok_r(NULL, ",", &ctx); if (!tok) { free(s); continue; } strncpy(b.title, tok, LIB_MAX_TITLE-1);
    tok = strtok_r(NULL, ",", &ctx); if (!tok) { free(s); continue; } strncpy(b.author, tok, LIB_MAX_AUTHOR-1);
    tok = strtok_r(NULL, ",", &ctx); if (tok) b.year = atoi(tok);
    tok = strtok_r(NULL, ",", &ctx); if (tok) b.total_stock = atoi(tok);
    tok = strtok_r(NULL, ",", &ctx); if (tok) b.available = atoi(tok);
    tok = strtok_r(NULL, ",", &ctx); if (tok) b.price = atof(tok);
    tok = strtok_r(NULL, "", &ctx); if (tok) strncpy(b.notes, tok, LIB_MAX_NOTES-1);
        lib_status_t st = ensure_books_capacity(db); if (st != LIB_OK) { free(s); free(line); fclose(f); free(p); return st; }
        db->books[db->books_count++] = b;
        free(s);
    }
    free(line); fclose(f); free(p);
    return LIB_OK;
}

static lib_status_t read_borrowers_csv(library_db_t *db, const char *path) {
    if (!db || !path) return LIB_ERR_INVALID_ARG;
    char *p = alloc_path_with_suffix(path, "_borrowers.csv");
    if (!p) return LIB_ERR_MEMORY;
    FILE *f = fopen(p, "r");
    if (!f) { free(p); return LIB_OK; }
    char *line = NULL; size_t len = 0; ssize_t r;
    bool skip_header = false;
    while ((r = getline(&line, &len, f)) != -1) {
        trim_newline(line);
        if (!skip_header) { skip_header = true; continue; }
        if (strlen(line) == 0) continue;
        int commas = 0; for (char *c = line; *c; ++c) if (*c == ',') commas++;
        char *s = my_strdup(line);
        char *ctx = NULL;
        borrower_t br; memset(&br,0,sizeof(br));
        if (commas >= 4) {
            char *tok = strtok_r(s, ",", &ctx); if (!tok) { free(s); continue; } strncpy(br.id, tok, sizeof(br.id)-1);
            tok = strtok_r(NULL, ",", &ctx); if (!tok) { free(s); continue; } strncpy(br.nim, tok, sizeof(br.nim)-1);
            tok = strtok_r(NULL, ",", &ctx); if (!tok) { free(s); continue; } strncpy(br.name, tok, LIB_MAX_NAME-1);
            tok = strtok_r(NULL, ",", &ctx); if (!tok) { free(s); continue; } strncpy(br.phone, tok, sizeof(br.phone)-1);
            tok = strtok_r(NULL, "", &ctx); if (tok) strncpy(br.email, tok, sizeof(br.email)-1);
        } else {
            char *tok = strtok_r(s, ",", &ctx); if (!tok) { free(s); continue; } strncpy(br.id, tok, sizeof(br.id)-1);
            tok = strtok_r(NULL, ",", &ctx); if (!tok) { free(s); continue; } strncpy(br.name, tok, LIB_MAX_NAME-1);
            tok = strtok_r(NULL, ",", &ctx); if (!tok) { free(s); continue; } strncpy(br.phone, tok, sizeof(br.phone)-1);
            tok = strtok_r(NULL, "", &ctx); if (tok) strncpy(br.email, tok, sizeof(br.email)-1);
            br.nim[0] = '\0';
        }
        lib_status_t st = ensure_borrowers_capacity(db); if (st != LIB_OK) { free(s); free(line); fclose(f); free(p); return st; }
        db->borrowers[db->borrowers_count++] = br;
        free(s);
    }
    free(line); fclose(f); free(p);
    return LIB_OK;
}

static lib_status_t read_loans_csv(library_db_t *db, const char *path) {
    if (!db || !path) return LIB_ERR_INVALID_ARG;
    char *p = alloc_path_with_suffix(path, "_loans.csv");
    if (!p) return LIB_ERR_MEMORY;
    FILE *f = fopen(p, "r");
    if (!f) { free(p); return LIB_OK; }
    char *line = NULL; size_t len = 0; ssize_t r;
    bool skip_header = false;
    while ((r = getline(&line, &len, f)) != -1) {
        trim_newline(line);
        if (!skip_header) { skip_header = true; continue; }
        if (strlen(line) == 0) continue;
        char *s = my_strdup(line);
        char *ctx = NULL;
        loan_t ln; memset(&ln,0,sizeof(ln));
        char *tok = strtok_r(s, ",", &ctx); if (!tok) { free(s); continue; } strncpy(ln.loan_id, tok, sizeof(ln.loan_id)-1);
        tok = strtok_r(NULL, ",", &ctx); if (!tok) { free(s); continue; } strncpy(ln.isbn, tok, LIB_MAX_ISBN-1);
        tok = strtok_r(NULL, ",", &ctx); if (!tok) { free(s); continue; } strncpy(ln.borrower_id, tok, sizeof(ln.borrower_id)-1);
        tok = strtok_r(NULL, ",", &ctx); if (tok) { int y,m,d; if (sscanf(tok, "%d-%d-%d", &y,&m,&d)==3) { ln.date_borrow.year=y; ln.date_borrow.month=m; ln.date_borrow.day=d; } }
        tok = strtok_r(NULL, ",", &ctx); if (tok) { int y,m,d; if (sscanf(tok, "%d-%d-%d", &y,&m,&d)==3) { ln.date_due.year=y; ln.date_due.month=m; ln.date_due.day=d; } }
        tok = strtok_r(NULL, ",", &ctx); if (tok && strlen(tok) >= 8) { int y,m,d; if (sscanf(tok, "%d-%d-%d", &y,&m,&d)==3) { ln.date_returned.year=y; ln.date_returned.month=m; ln.date_returned.day=d; ln.is_returned = true; } }
        tok = strtok_r(NULL, ",", &ctx); if (tok) ln.is_returned = atoi(tok) ? true : ln.is_returned;
        tok = strtok_r(NULL, ",", &ctx); if (tok) ln.is_lost = atoi(tok) ? true : false;
        tok = strtok_r(NULL, ",", &ctx); if (tok) ln.fine_paid = atol(tok);
        lib_status_t st = ensure_loans_capacity(db); if (st != LIB_OK) { free(s); free(line); fclose(f); free(p); return st; }
        db->loans[db->loans_count++] = ln;
        free(s);
    }
    free(line); fclose(f); free(p);
    return LIB_OK;
}

/* ---------- atomic rename helper ---------- */

static int replace_file_atomic(const char *tmp_path, const char *final_path) {
    if (!tmp_path || !final_path) return -1;
#if defined(_WIN32) || defined(_WIN64)
    /* On Windows, try remove final then rename; try MoveFileEx as fallback */
    if (remove(final_path) != 0) {
        if (errno != ENOENT) { /* ignore if not exists */ }
    }
    if (rename(tmp_path, final_path) == 0) return 0;
    #if defined(MOVEFILE_REPLACE_EXISTING)
        if (MoveFileExA(tmp_path, final_path, MOVEFILE_REPLACE_EXISTING)) return 0;
    #endif
    return -1;
#else
    if (rename(tmp_path, final_path) == 0) return 0;
    return -1;
#endif
}

/* ---------- Public DB management API ---------- */

library_db_t *lib_db_open(const char *path, lib_status_t *err) {
    if (err) *err = LIB_OK;
    library_db_t *db = calloc(1, sizeof(library_db_t));
    if (!db) { if (err) *err = LIB_ERR_MEMORY; return NULL; }
    db->books = NULL; db->books_count = db->books_capacity = 0;
    db->borrowers = NULL; db->borrowers_count = db->borrowers_capacity = 0;
    db->loans = NULL; db->loans_count = db->loans_capacity = 0;
    db->fine_per_day = LIB_DEFAULT_FINE_PER_DAY;
    db->max_book_types = LIB_MAX_BOOK_TYPES;
    db->replacement_cost_days = LIB_REPLACEMENT_COST_DAYS_DEFAULT;
    if (path) db->db_file_path = my_strdup(path);
    else db->db_file_path = my_strdup(LIB_DEFAULT_DB_FILE);
    if (!db->db_file_path) { free(db); if (err) *err = LIB_ERR_MEMORY; return NULL; }
    srand((unsigned)time(NULL));
    (void) read_books_csv(db, db->db_file_path);
    (void) read_borrowers_csv(db, db->db_file_path);
    (void) read_loans_csv(db, db->db_file_path);
    /* Read persisted policy meta if present (fine_per_day, replacement_cost_days) */
    (void) read_meta_file(db);
    if (err) *err = LIB_OK;
    return db;
}

/* In-place initializer: prepares a stack-allocated or pre-allocated library_db_t
   to be used by code that expects an already-initialized struct. This mirrors
   the older `lib_db_init` API used by legacy callers. */
lib_status_t lib_db_init(library_db_t *db) {
    if (!db) return LIB_ERR_INVALID_ARG;
    db->books = NULL; db->books_count = db->books_capacity = 0;
    db->borrowers = NULL; db->borrowers_count = db->borrowers_capacity = 0;
    db->loans = NULL; db->loans_count = db->loans_capacity = 0;
    db->fine_per_day = LIB_DEFAULT_FINE_PER_DAY;
    db->max_book_types = LIB_MAX_BOOK_TYPES;
    db->db_file_path = my_strdup(LIB_DEFAULT_DB_FILE);
    db->replacement_cost_days = LIB_REPLACEMENT_COST_DAYS_DEFAULT;
    if (!db->db_file_path) return LIB_ERR_MEMORY;
    /* Ensure data directory exists for the default DB path */
    ensure_dir_for_path(db->db_file_path);
    return LIB_OK;
}

/* ---------- lib_db_save (atomic write for each file) ---------- */

lib_status_t lib_db_save(library_db_t *db) {
    if (!db) return LIB_ERR_INVALID_ARG;
    lib_status_t st = LIB_OK;

    /* Books */
    char *final_books = alloc_path_with_suffix(db->db_file_path, "_books.csv");
    if (!final_books) return LIB_ERR_MEMORY;
    size_t tmp_len = strlen(final_books) + 5;
    char *tmp_books = malloc(tmp_len);
    if (!tmp_books) { free(final_books); return LIB_ERR_MEMORY; }
    snprintf(tmp_books, tmp_len, "%s.tmp", final_books);
    st = write_books_csv_to(db, tmp_books);
    if (st != LIB_OK) { free(final_books); free(tmp_books); return st; }
    if (replace_file_atomic(tmp_books, final_books) != 0) {
        free(final_books); free(tmp_books); return LIB_ERR_IO;
    }
    free(tmp_books); free(final_books);

    /* Borrowers */
    char *final_b = alloc_path_with_suffix(db->db_file_path, "_borrowers.csv");
    if (!final_b) return LIB_ERR_MEMORY;
    tmp_len = strlen(final_b) + 5;
    char *tmp_b = malloc(tmp_len);
    if (!tmp_b) { free(final_b); return LIB_ERR_MEMORY; }
    snprintf(tmp_b, tmp_len, "%s.tmp", final_b);
    st = write_borrowers_csv_to(db, tmp_b);
    if (st != LIB_OK) { free(final_b); free(tmp_b); return st; }
    if (replace_file_atomic(tmp_b, final_b) != 0) { free(final_b); free(tmp_b); return LIB_ERR_IO; }
    free(tmp_b); free(final_b);

    /* Loans */
    char *final_l = alloc_path_with_suffix(db->db_file_path, "_loans.csv");
    if (!final_l) return LIB_ERR_MEMORY;
    tmp_len = strlen(final_l) + 5;
    char *tmp_l = malloc(tmp_len);
    if (!tmp_l) { free(final_l); return LIB_ERR_MEMORY; }
    snprintf(tmp_l, tmp_len, "%s.tmp", final_l);
    st = write_loans_csv_to(db, tmp_l);
    if (st != LIB_OK) { free(final_l); free(tmp_l); return st; }
    if (replace_file_atomic(tmp_l, final_l) != 0) { free(final_l); free(tmp_l); return LIB_ERR_IO; }
    free(tmp_l); free(final_l);

    /* Meta: write policy file (fine_per_day, replacement_cost_days) */
    char *final_meta = alloc_path_with_suffix(db->db_file_path, "_meta.cfg");
    if (final_meta) {
        size_t tmpm_len = strlen(final_meta) + 5;
        char *tmp_meta = malloc(tmpm_len);
        if (tmp_meta) {
            snprintf(tmp_meta, tmpm_len, "%s.tmp", final_meta);
            FILE *mf = fopen(tmp_meta, "w");
            if (mf) {
                if (fprintf(mf, "fine_per_day=%ld\n", db->fine_per_day) < 0) { /* ignore */ }
                if (fprintf(mf, "replacement_cost_days=%lu\n", db->replacement_cost_days) < 0) { /* ignore */ }
                fflush(mf);
#if !defined(_WIN32) && !defined(_WIN64)
                fsync(fileno(mf));
#endif
                fclose(mf);
                replace_file_atomic(tmp_meta, final_meta);
            }
            free(tmp_meta);
        }
        free(final_meta);
    }

    return LIB_OK;
}

lib_status_t lib_db_close(library_db_t *db) {
    if (!db) return LIB_ERR_INVALID_ARG;
    if (db->books) free(db->books);
    if (db->borrowers) free(db->borrowers);
    if (db->loans) free(db->loans);
    if (db->db_file_path) free(db->db_file_path);
    free(db);
    return LIB_OK;
}

/* Read meta file if present: simple key=value lines */
static void read_meta_file(library_db_t *db) {
    if (!db || !db->db_file_path) return;
    char *meta = alloc_path_with_suffix(db->db_file_path, "_meta.cfg");
    if (!meta) return;
    FILE *f = fopen(meta, "r");
    if (!f) { free(meta); return; }
    char *line = NULL; size_t len = 0; ssize_t r;
    while ((r = getline(&line, &len, f)) != -1) {
        trim_newline(line);
        if (line[0] == '\0') continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0'; char *key = line; char *val = eq + 1;
        if (strcmp(key, "fine_per_day") == 0) db->fine_per_day = atol(val);
        else if (strcmp(key, "replacement_cost_days") == 0) db->replacement_cost_days = strtoul(val, NULL, 10);
    }
    if (line) free(line);
    fclose(f); free(meta);
}

/* ---------- max_book_types API ---------- */

lib_status_t lib_set_max_book_types(library_db_t *db, size_t max) {
    if (!db) return LIB_ERR_INVALID_ARG;
    if (max == 0) db->max_book_types = LIB_MAX_BOOK_TYPES;
    else {
        if (max > LIB_MAX_BOOK_TYPES_HARD_LIMIT) return LIB_ERR_INVALID_ARG;
        db->max_book_types = max;
    }
    return LIB_OK;
}
size_t lib_get_max_book_types(const library_db_t *db) {
    if (!db) return 0;
    return db->max_book_types;
}

/* ---------- Book management ---------- */

lib_status_t lib_add_book(library_db_t *db, const book_t *book) {
    if (!db || !book) return LIB_ERR_INVALID_ARG;
    if (db->books_count >= db->max_book_types) return LIB_ERR_MAX_TYPES;
    for (size_t i = 0; i < db->books_count; ++i) {
        if (strcmp(db->books[i].isbn, book->isbn) == 0) return LIB_ERR_EXISTS;
    }
    lib_status_t st = ensure_books_capacity(db); if (st != LIB_OK) return st;
    db->books[db->books_count++] = *book;
    return LIB_OK;
}

lib_status_t lib_remove_book(library_db_t *db, const char *isbn) {
    if (!db || !isbn) return LIB_ERR_INVALID_ARG;
    for (size_t i = 0; i < db->loans_count; ++i) {
        if (strcmp(db->loans[i].isbn, isbn) == 0 && !db->loans[i].is_returned) return LIB_ERR_INVALID_ARG;
    }
    size_t idx = SIZE_MAX;
    for (size_t i = 0; i < db->books_count; ++i) if (strcmp(db->books[i].isbn, isbn) == 0) { idx = i; break; }
    if (idx == SIZE_MAX) return LIB_ERR_NOT_FOUND;
    for (size_t i = idx; i + 1 < db->books_count; ++i) db->books[i] = db->books[i+1];
    db->books_count--;
    return LIB_OK;
}

const book_t *lib_find_book_by_isbn(const library_db_t *db, const char *isbn) {
    if (!db || !isbn) return NULL;
    for (size_t i = 0; i < db->books_count; ++i) if (strcmp(db->books[i].isbn, isbn) == 0) return &db->books[i];
    return NULL;
}

size_t lib_search_books_by_title(const library_db_t *db, const char *title_substr, const book_t **out, size_t out_capacity) {
    if (!db || !title_substr || !out) return 0;
    size_t found = 0;
    for (size_t i = 0; i < db->books_count && found < out_capacity; ++i) {
        if (contains_case_insensitive(db->books[i].title, title_substr)) out[found++] = &db->books[i];
    }
    return found;
}

lib_status_t lib_update_book_stock(library_db_t *db, const char *isbn, int delta) {
    if (!db || !isbn) return LIB_ERR_INVALID_ARG;
    for (size_t i = 0; i < db->books_count; ++i) {
        if (strcmp(db->books[i].isbn, isbn) == 0) {
            long new_total = (long)db->books[i].total_stock + delta;
            long new_avail = (long)db->books[i].available + delta;
            if (new_total < 0 || new_avail < 0) return LIB_ERR_NO_STOCK;
            db->books[i].total_stock = (int)new_total;
            db->books[i].available = (int)new_avail;
            return LIB_OK;
        }
    }
    return LIB_ERR_NOT_FOUND;
}

/* ---------- Borrower management (NIM) ---------- */

lib_status_t lib_add_borrower(library_db_t *db, const borrower_t *b) {
    if (!db || !b) return LIB_ERR_INVALID_ARG;
    if (b->nim[0] != '\0') {
        for (size_t i = 0; i < db->borrowers_count; ++i) {
            if (strcmp(db->borrowers[i].nim, b->nim) == 0) return LIB_ERR_EXISTS;
        }
    }
    for (size_t i = 0; i < db->borrowers_count; ++i) {
        if (strcmp(db->borrowers[i].id, b->id) == 0) return LIB_ERR_EXISTS;
    }
    lib_status_t st = ensure_borrowers_capacity(db); if (st != LIB_OK) return st;
    db->borrowers[db->borrowers_count++] = *b;
    return LIB_OK;
}

const borrower_t *lib_find_borrower_by_id(const library_db_t *db, const char *id) {
    if (!db || !id) return NULL;
    for (size_t i = 0; i < db->borrowers_count; ++i) if (strcmp(db->borrowers[i].id, id) == 0) return &db->borrowers[i];
    return NULL;
}

const borrower_t *lib_find_borrower_by_nim(const library_db_t *db, const char *nim) {
    if (!db || !nim) return NULL;
    for (size_t i = 0; i < db->borrowers_count; ++i) if (strcmp(db->borrowers[i].nim, nim) == 0) return &db->borrowers[i];
    return NULL;
}

borrower_t *lib_get_or_create_borrower_by_nim(library_db_t *db, const char *nim, bool create_if_missing) {
    if (!db || !nim) return NULL;
    for (size_t i = 0; i < db->borrowers_count; ++i) {
        if (strcmp(db->borrowers[i].nim, nim) == 0) return &db->borrowers[i];
    }
    if (!create_if_missing) return NULL;
    lib_status_t st = ensure_borrowers_capacity(db); if (st != LIB_OK) return NULL;
    borrower_t br; memset(&br,0,sizeof(br));
    generate_unique_id("B", br.id, sizeof(br.id));
    strncpy(br.nim, nim, sizeof(br.nim)-1);
    br.name[0] = '\0';
    br.phone[0] = '\0';
    br.email[0] = '\0';
    db->borrowers[db->borrowers_count++] = br;
    return &db->borrowers[db->borrowers_count - 1];
}

bool lib_validate_nim_format(const char *nim) {
    if (!nim) return false;
    size_t len = strlen(nim);
    if (len < 4 || len > 32) return false;
    for (size_t i = 0; i < len; ++i) {
        if (!isalnum((unsigned char)nim[i])) return false;
    }
    return true;
}

/* ---------- Loan management ---------- */

unsigned long lib_calculate_fine(const library_db_t *db, lib_date_t due_date, lib_date_t return_date) {
    if (!db) return 0;
    int days = lib_date_days_between(due_date, return_date);
    if (days <= 0) return 0;
    return (unsigned long) days * (unsigned long) db->fine_per_day;
}

lib_status_t lib_checkout_book(library_db_t *db, const char *isbn, const borrower_t *borrower, lib_date_t date_borrow, lib_date_t date_due, char out_loan_id[32]) {
    if (!db || !isbn || !borrower) return LIB_ERR_INVALID_ARG;
    size_t bi = SIZE_MAX;
    for (size_t i = 0; i < db->books_count; ++i) if (strcmp(db->books[i].isbn, isbn) == 0) { bi = i; break; }
    if (bi == SIZE_MAX) return LIB_ERR_NOT_FOUND;
    if (db->books[bi].available <= 0) return LIB_ERR_NO_STOCK;
    const borrower_t *exists = lib_find_borrower_by_id(db, borrower->id);
    if (!exists) {
        lib_status_t st = lib_add_borrower(db, borrower);
        if (st != LIB_OK) return st;
    }
    lib_status_t st = ensure_loans_capacity(db); if (st != LIB_OK) return st;
    loan_t ln; memset(&ln,0,sizeof(ln));
    generate_unique_id("L", ln.loan_id, sizeof(ln.loan_id));
    strncpy(ln.isbn, isbn, LIB_MAX_ISBN-1);
    strncpy(ln.borrower_id, borrower->id, sizeof(ln.borrower_id)-1);
    ln.date_borrow = date_borrow;
    ln.date_due = date_due;
    ln.is_returned = false; ln.is_lost = false; ln.fine_paid = 0;
    db->loans[db->loans_count++] = ln;
    db->books[bi].available -= 1;
    if (out_loan_id) strncpy(out_loan_id, ln.loan_id, 32);
    return LIB_OK;
}

lib_status_t lib_return_book(library_db_t *db, const char *loan_id, lib_date_t date_return, unsigned long *out_fine) {
    if (!db || !loan_id) return LIB_ERR_INVALID_ARG;
    size_t li = SIZE_MAX;
    for (size_t i = 0; i < db->loans_count; ++i) if (strcmp(db->loans[i].loan_id, loan_id) == 0) { li = i; break; }
    if (li == SIZE_MAX) return LIB_ERR_NOT_FOUND;
    loan_t *ln = &db->loans[li];
    /* If the loan was already returned or marked as lost, do not accept a normal return */
    if (ln->is_returned) return LIB_ERR_INVALID_ARG;
    if (ln->is_lost) return LIB_ERR_INVALID_ARG;

    ln->is_returned = true;
    ln->date_returned = date_return;
    unsigned long fine = lib_calculate_fine(db, ln->date_due, date_return);
    ln->fine_paid = fine;
    if (out_fine) *out_fine = fine;

    /* Restore available stock safely (don't overflow) */
    for (size_t i = 0; i < db->books_count; ++i) {
        if (strcmp(db->books[i].isbn, ln->isbn) == 0) {
            /* ensure available does not exceed total_stock */
            if (db->books[i].available < db->books[i].total_stock) db->books[i].available += 1;
            break;
        }
    }
    return LIB_OK;
}

/* Replacement cost policy: default multiplier in days of fine_per_day to estimate replacement cost */
/* Replacement default days used when DB doesn't override it */
#define LIB_REPLACEMENT_COST_DAYS_DEFAULT 30UL

lib_status_t lib_set_replacement_cost_days(library_db_t *db, unsigned long days) {
    if (!db) return LIB_ERR_INVALID_ARG;
    db->replacement_cost_days = days;
    return LIB_OK;
}

unsigned long lib_get_replacement_cost_days(const library_db_t *db) {
    if (!db) return LIB_REPLACEMENT_COST_DAYS_DEFAULT;
    return db->replacement_cost_days ? db->replacement_cost_days : LIB_REPLACEMENT_COST_DAYS_DEFAULT;
}

/* Fine policy accessors */
lib_status_t lib_set_fine_per_day(library_db_t *db, long fine) {
    if (!db) return LIB_ERR_INVALID_ARG;
    db->fine_per_day = fine;
    return LIB_OK;
}

long lib_get_fine_per_day(const library_db_t *db) {
    if (!db) return LIB_DEFAULT_FINE_PER_DAY;
    return db->fine_per_day;
}

lib_status_t lib_mark_book_lost(library_db_t *db, const char *loan_id, unsigned long *out_cost) {
    if (!db || !loan_id) return LIB_ERR_INVALID_ARG;
    size_t li = SIZE_MAX;
    for (size_t i = 0; i < db->loans_count; ++i) if (strcmp(db->loans[i].loan_id, loan_id) == 0) { li = i; break; }
    if (li == SIZE_MAX) return LIB_ERR_NOT_FOUND;
    loan_t *ln = &db->loans[li];
    if (ln->is_lost) return LIB_ERR_INVALID_ARG;

    /* Mark lost. If the book hasn't been returned yet, mark it returned at today
     * and compute a replacement cost as `fine_per_day * replacement_cost_days`.
     */
    ln->is_lost = true;
    if (!ln->is_returned) {
        ln->is_returned = true;
        ln->date_returned = lib_date_from_time_t(time(NULL));
    }

    /* Prefer computing replacement cost from known book price (persisted). If price is 0 (unknown),
     * fallback to configured `replacement_cost_days * fine_per_day`.
     */
    unsigned long cost = 0;
    for (size_t i = 0; i < db->books_count; ++i) {
        if (strcmp(db->books[i].isbn, ln->isbn) == 0) {
            double p = db->books[i].price;
            if (p > 0.0) {
                /* round to nearest currency unit */
                cost = (unsigned long) llround(p);
            } else {
                unsigned long days = lib_get_replacement_cost_days(db);
                cost = (unsigned long) db->fine_per_day * days;
            }
            /* Adjust library stock: reduce total and available safely (prevent negative values). */
            if (db->books[i].total_stock > 0) db->books[i].total_stock -= 1;
            if (db->books[i].available > 0) db->books[i].available -= 1;
            /* Ensure available never exceeds total_stock */
            if (db->books[i].available > db->books[i].total_stock) db->books[i].available = db->books[i].total_stock;
            break;
        }
    }
    ln->fine_paid = (long) cost;

    if (out_cost) *out_cost = cost;
    return LIB_OK;
}

lib_status_t lib_set_loan_payment(library_db_t *db, const char *loan_id, long amount) {
    if (!db || !loan_id) return LIB_ERR_INVALID_ARG;
    for (size_t i = 0; i < db->loans_count; ++i) {
        if (strcmp(db->loans[i].loan_id, loan_id) == 0) {
            db->loans[i].fine_paid = amount;
            return LIB_OK;
        }
    }
    return LIB_ERR_NOT_FOUND;
}

size_t lib_find_loans_by_borrower(const library_db_t *db, const char *borrower_id_or_name, loan_t **out, size_t out_capacity) {
    if (!db || !borrower_id_or_name || !out) return 0;
    size_t found = 0;
    for (size_t i = 0; i < db->loans_count && found < out_capacity; ++i) {
        const loan_t *ln = &db->loans[i];
        if (strcmp(ln->borrower_id, borrower_id_or_name) == 0) { out[found++] = (loan_t *)ln; continue; }
        const borrower_t *br = lib_find_borrower_by_id(db, ln->borrower_id);
        if (br && contains_case_insensitive(br->name, borrower_id_or_name)) { out[found++] = (loan_t *)ln; }
        else if (br && contains_case_insensitive(br->nim, borrower_id_or_name)) { out[found++] = (loan_t *)ln; }
    }
    return found;
}

/* ---------- Admin auth minimal (file-based) ---------- */

static void simple_hash_password(const char *plain, char *out_hash, size_t out_sz) {
    /* Use 64-bit accumulator and print lower 32 bits as hex to match previous behaviour. */
    unsigned long long h = 1469598103934665603ULL;
    for (const unsigned char *p = (const unsigned char *)plain; *p; ++p) h = (h ^ *p) * 1099511628211ULL;
    unsigned long long low = h & 0xffffffffULL;
    snprintf(out_hash, out_sz, "%08x", (unsigned int)low);
}

#define ADMIN_FILE_SUFFIX "_admins.csv"

lib_status_t lib_add_admin(const char *username, const char *password_plain) {
    if (!username || !password_plain) return LIB_ERR_INVALID_ARG;
    char *admin_path = alloc_path_with_suffix(LIB_DEFAULT_DB_FILE, ADMIN_FILE_SUFFIX);
    if (!admin_path) return LIB_ERR_MEMORY;
    FILE *f = fopen(admin_path, "a+");
    if (!f) { free(admin_path); return LIB_ERR_IO; }
    rewind(f);
    char *line = NULL; size_t len = 0;
    while (getline(&line, &len, f) != -1) {
        trim_newline(line);
        if (strlen(line) == 0) continue;
        char *tmp = my_strdup(line);
        char *ctx = NULL;
        char *tok = strtok_r(tmp, ",", &ctx);
        if (tok && strcmp(tok, username) == 0) { free(tmp); free(line); fclose(f); free(admin_path); return LIB_ERR_EXISTS; }
        free(tmp);
    }
    if (line) free(line);
    char hash[64]; simple_hash_password(password_plain, hash, sizeof(hash));
    fprintf(f, "%s,%s\n", username, hash);
    fclose(f); free(admin_path);
    return LIB_OK;
}

lib_status_t lib_verify_admin(const char *username, const char *password_plain, admin_user_t *out) {
    if (!username || !password_plain) return LIB_ERR_INVALID_ARG;
    char *admin_path = alloc_path_with_suffix(LIB_DEFAULT_DB_FILE, ADMIN_FILE_SUFFIX);
    if (!admin_path) return LIB_ERR_MEMORY;
    FILE *f = fopen(admin_path, "r");
    if (!f) { free(admin_path); return LIB_ERR_IO; }
    char *line = NULL; size_t len = 0;
    char hash[64]; simple_hash_password(password_plain, hash, sizeof(hash));
    lib_status_t res = LIB_ERR_AUTH;
    while (getline(&line, &len, f) != -1) {
        trim_newline(line);
        if (strlen(line) == 0) continue;
        char *tmp = my_strdup(line);
        char *ctx = NULL;
        char *tok = strtok_r(tmp, ",", &ctx);
        if (tok && strcmp(tok, username) == 0) {
            char *tok2 = strtok_r(NULL, ",", &ctx);
            if (tok2 && strcmp(tok2, hash) == 0) {
                if (out) { strncpy(out->username, username, sizeof(out->username)-1); strncpy(out->password_hash, tok2, sizeof(out->password_hash)-1); out->is_super = false; }
                res = LIB_OK;
            } else res = LIB_ERR_AUTH;
            free(tmp);
            break;
        }
        free(tmp);
    }
    if (line) free(line);
    fclose(f); free(admin_path);
    return res;
}

/* ---------- Print helpers ---------- */

void lib_print_book(const book_t *b, FILE *fp) {
    if (!b || !fp) return;
    fprintf(fp, "ISBN: %s\nTitle: %s\nAuthor: %s\nYear: %d\nStock: %d (available %d)\nNotes: %s\n",
            b->isbn, b->title, b->author, b->year, b->total_stock, b->available, b->notes);
}
void lib_print_borrower(const borrower_t *br, FILE *fp) {
    if (!br || !fp) return;
    fprintf(fp, "ID: %s\nNIM: %s\nName: %s\nPhone: %s\nEmail: %s\n", br->id, br->nim, br->name, br->phone, br->email);
}
void lib_print_loan(const loan_t *l, FILE *fp) {
    if (!l || !fp) return;
    fprintf(fp, "Loan ID: %s\nISBN: %s\nBorrower ID: %s\nBorrowed: %04d-%02d-%02d\nDue: %04d-%02d-%02d\nReturned: %s\nLost: %d\nFinePaid: %ld\n",
            l->loan_id, l->isbn, l->borrower_id,
            l->date_borrow.year, l->date_borrow.month, l->date_borrow.day,
            l->date_due.year, l->date_due.month, l->date_due.day,
            l->is_returned ? "YES" : "NO",
            l->is_lost, (long)l->fine_paid);
}

/* ---------- convenience alloc/free ---------- */

book_t *lib_book_create_empty(void) { return calloc(1, sizeof(book_t)); }
void lib_book_free(book_t *b) { if (b) free(b); }
borrower_t *lib_borrower_create_empty(void) { return calloc(1, sizeof(borrower_t)); }
void lib_borrower_free(borrower_t *b) { if (b) free(b); }
loan_t *lib_loan_create_empty(void) { return calloc(1, sizeof(loan_t)); }
void lib_loan_free(loan_t *l) { if (l) free(l); }

/* ---------- Import/Export wrappers ---------- */

lib_status_t lib_db_export_csv(const library_db_t *db, const char *path) {
    if (!db || !path) return LIB_ERR_INVALID_ARG;
    /* treat `path` as a base path (like LIB_DEFAULT_DB_FILE) and write the three suffixed files */
    char *p_books = alloc_path_with_suffix(path, "_books.csv");
    if (!p_books) return LIB_ERR_MEMORY;
    lib_status_t st = write_books_csv_to(db, p_books);
    free(p_books);
    if (st != LIB_OK) return st;

    char *p_borrowers = alloc_path_with_suffix(path, "_borrowers.csv");
    if (!p_borrowers) return LIB_ERR_MEMORY;
    st = write_borrowers_csv_to(db, p_borrowers);
    free(p_borrowers);
    if (st != LIB_OK) return st;

    char *p_loans = alloc_path_with_suffix(path, "_loans.csv");
    if (!p_loans) return LIB_ERR_MEMORY;
    st = write_loans_csv_to(db, p_loans);
    free(p_loans);
    return st;
}

lib_status_t lib_db_import_csv(library_db_t *db, const char *path) {
    if (!db || !path) return LIB_ERR_INVALID_ARG;
    if (db->books) { free(db->books); db->books = NULL; db->books_capacity = db->books_count = 0; }
    if (db->borrowers) { free(db->borrowers); db->borrowers = NULL; db->borrowers_capacity = db->borrowers_count = 0; }
    if (db->loans) { free(db->loans); db->loans = NULL; db->loans_capacity = db->loans_count = 0; }
    lib_status_t st = read_books_csv(db, path); if (st != LIB_OK) return st;
    st = read_borrowers_csv(db, path); if (st != LIB_OK) return st;
    st = read_loans_csv(db, path); return st;
}

/* Compatibility wrappers for older caller expectations */
/* Provide a mutable find and a delete alias used by admin.c */
book_t *lib_find_book_by_isbn_mutable(library_db_t *db, const char *isbn) {
    if (!db || !isbn) return NULL;
    for (size_t i = 0; i < db->books_count; ++i) if (strcmp(db->books[i].isbn, isbn) == 0) return &db->books[i];
    return NULL;
}

lib_status_t lib_delete_book(library_db_t *db, const char *isbn) {
    return lib_remove_book(db, isbn);
}
