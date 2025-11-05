#ifndef VIEW_H
#define VIEW_H

/* view.h
   UI module header. This header intentionally uses library.h as the
   source of truth for book_t and other shared types.
*/

#ifdef __cplusplus
extern "C" {
#endif

#include "library.h" /* harus menyediakan book_t, LIB_MAX_* defines */

/* UI lifecycle */
int ui_init(void);
void ui_shutdown(void);

/* Persistence helpers (wrap/save the view's internal DB view) */
int ui_save_db(const char *filename);
int ui_load_db(const char *filename);

/* Demo/interactive mainloop (blocking) */
void ui_demo_mainloop(void);

/* demo helper: add a book to the UI's in-memory store */
void ui_add_demo_book(const char *isbn, const char *title, const char *author, int year, int total_stock, int available);

/* Book-detail display (animated slide-in) */
void ui_show_book_detail(const book_t *b);

/* Autocomplete input (blocking). Returns 0 when confirmed, -1 on cancel. */
int ui_input_autocomplete(char *buffer, size_t bufsize);

/* Render grid of books with paging & highlight within a rectangular area. */
void ui_render_book_list(int top, int left, int width, int height, const char *query, int page, int selected_local_index);

/* Borrow / Return actions by global (UI) index - returns 0 success, -1 error */
int ui_borrow_book_by_index(int global_index);
int ui_return_book_by_index(int global_index);

/* Show history timeline (animated) */
void ui_show_history_timeline(void);

#ifdef __cplusplus
}
#endif

#endif /* VIEW_H */
