#ifndef UI_H
#define UI_H

#include "library.h"

/* Helper untuk membaca input dengan aman - internal use only */
char *ui_read_line(char *buf, size_t size);

/* Fungsi untuk menu dan UI */
void header_tampilan(const char *title);
void press_enter(void);
void ui_clear_screen(void);
/* Persist UI preferences (theme) */
int ui_load_theme(void); /* returns color code (0 = none/reset) */
int ui_save_theme(int color_code);

/* Fungsi login dan menu */
int login_admin(void);
void menu_admin(library_db_t *db);

/* Inisialisasi dan data sample */
void init_jenis_list(void);
void ensure_sample_data(void);

#endif /* UI_H */