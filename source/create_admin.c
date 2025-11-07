#include <stdio.h>
#include <stdlib.h>
#include "library.h"

int main(void) {
    /* Initialize library */
    lib_status_t st = lib_add_admin("Kakak Admin", "1234");
    if (st == LIB_OK) {
        printf("Admin account created successfully!\n");
        return 0;
    } else if (st == LIB_ERR_EXISTS) {
        printf("Admin account already exists.\n");
        return 0;
    } else {
        printf("Failed to create admin account (error: %d)\n", (int)st);
        return 1;
    }
}