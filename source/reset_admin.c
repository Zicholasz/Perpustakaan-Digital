#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simple hash function - must match the one in library.c */
static void simple_hash_password(const char *plain, char *out_hash, size_t out_sz) {
    unsigned long long h = 1469598103934665603ULL;
    for (const unsigned char *p = (const unsigned char *)plain; *p; ++p) 
        h = (h ^ *p) * 1099511628211ULL;
    unsigned long long low = h & 0xffffffffULL;
    snprintf(out_hash, out_sz, "%08x", (unsigned int)low);
}

int main(void) {
    const char *username = "Kakak Admin";
    const char *password = "1234";
    char hash[64];
    
    /* Compute the hash */
    simple_hash_password(password, hash, sizeof(hash));
    
    /* Open and clear the admins file */
    FILE *f = fopen("c:\\Users\\user\\Documents\\Projek DDP Sistem Perpustakaan Digital\\library-project\\data\\library_db_admins.csv", "w");
    if (!f) {
        printf("Failed to open admin database!\n");
        return 1;
    }
    
    /* Write the admin entry */
    fprintf(f, "%s,%s\n", username, hash);
    fclose(f);
    
    printf("Admin account created:\n");
    printf("Username: %s\n", username);
    printf("Password: %s\n", password);
    printf("Hash: %s\n", hash);
    return 0;
}