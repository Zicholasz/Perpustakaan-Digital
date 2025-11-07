#ifndef PEMINJAM_H
#define PEMINJAM_H

#include "library.h"

/* Fungsi untuk login */
int login_peminjam(void);

/* Menu utama peminjam */
void menu_peminjam(library_db_t *db);

#endif /* PEMINJAM_H */