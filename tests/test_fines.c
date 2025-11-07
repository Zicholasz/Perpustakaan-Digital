#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../include/library.h"

int main(void) {
    library_db_t db_stack;
    if (lib_db_init(&db_stack) != LIB_OK) {
        fprintf(stderr, "Failed to init db\n");
        return 2;
    }
    library_db_t *db = &db_stack;

    /* Set fine policy small for test */
    db->fine_per_day = 10; /* 10 units per day */
    lib_set_replacement_cost_days(db, 7); /* 7 days replacement multiplier */

    /* Add a book */
    book_t b = {0};
    strncpy(b.isbn, "TESTISBN001", sizeof(b.isbn)-1);
    strncpy(b.title, "Test Book", sizeof(b.title)-1);
    strncpy(b.author, "Author", sizeof(b.author)-1);
    b.year = 2020; b.total_stock = 2; b.available = 2;
    if (lib_add_book(db, &b) != LIB_OK) { fprintf(stderr, "add book failed\n"); return 3; }

    /* Add borrower */
    borrower_t br = {0};
    strncpy(br.id, "BR1", sizeof(br.id)-1);
    strncpy(br.nim, "NIM1", sizeof(br.nim)-1);
    strncpy(br.name, "Tester", sizeof(br.name)-1);
    lib_add_borrower(db, &br);

    /* Checkout book with due date 5 days ago */
    lib_date_t borrow = lib_date_from_time_t(time(NULL) - 10*24*3600);
    lib_date_t due = lib_date_from_time_t(time(NULL) - 5*24*3600);
    char loan_id[32] = {0};
    if (lib_checkout_book(db, b.isbn, &br, borrow, due, loan_id) != LIB_OK) {
        fprintf(stderr, "checkout failed\n"); return 4;
    }

    /* Return today -> expect fine = 5 * fine_per_day = 50 */
    unsigned long fine = 0;
    lib_date_t today = lib_date_from_time_t(time(NULL));
    if (lib_return_book(db, loan_id, today, &fine) != LIB_OK) {
        fprintf(stderr, "return failed\n"); return 5;
    }
    printf("Returned loan %s, fine=%lu (expected 50)\n", loan_id, fine);

    /* Checkout another and mark lost */
    char loan2[32] = {0};
    if (lib_checkout_book(db, b.isbn, &br, borrow, due, loan2) != LIB_OK) { fprintf(stderr, "checkout2 failed\n"); return 6; }
    unsigned long cost = 0;
    if (lib_mark_book_lost(db, loan2, &cost) != LIB_OK) { fprintf(stderr, "mark lost failed\n"); return 7; }
    printf("Marked loan %s lost, replacement cost=%lu (expected %lu)\n", loan2, cost, db->fine_per_day * lib_get_replacement_cost_days(db));

    return 0;
}
