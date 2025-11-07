#include <stdio.h>
#include <stdlib.h>
#include "library.h"

int main(void) {
    /* Initialize library */
    // Delete any existing admin account first to reset
    remove("data/library_db_admins.csv");
    
    lib_status_t st = lib_add_admin("Kakak Admin", "1234");
    if (st == LIB_OK) {
        printf("Admin account created successfully!\n");
        printf("Username: Kakak Admin\n");
        printf("Password: 1234\n");
        return 0;
    } else if (st == LIB_ERR_EXISTS) {
        printf("Admin account already exists.\n");
        return 0;
    } else {
        printf("Failed to create admin account (error: %d)\n", (int)st);
        return 1;
    }
}