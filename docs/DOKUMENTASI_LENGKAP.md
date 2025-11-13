# Dokumentasi Lengkap Sistem Perpustakaan Digital

## Daftar Isi
1. [Pendahuluan](#1-pendahuluan)
2. [Struktur Project](#2-struktur-project)
3. [Komponen Utama](#3-komponen-utama)
4. [Alur Program](#4-alur-program)
5. [Detail Implementasi](#5-detail-implementasi)
6. [Basis Data](#6-basis-data)
7. [Panduan Penggunaan](#7-panduan-penggunaan)

## 1. Pendahuluan

Sistem Perpustakaan Digital adalah aplikasi manajemen perpustakaan yang diimplementasikan dalam bahasa C. Sistem ini memungkinkan pengelolaan buku, peminjam, dan transaksi peminjaman dengan fitur-fitur seperti pendaftaran peminjam, pencarian buku, peminjaman, pengembalian, dan perhitungan denda.

### 1.1 Fitur Utama
- Manajemen buku (tambah, hapus, update stok)
- Pendaftaran dan manajemen peminjam
- Sistem peminjaman dan pengembalian
- Pencarian buku berdasarkan ISBN atau judul
- Perhitungan denda otomatis
- Sistem autentikasi admin
- Pencatatan riwayat peminjaman
- Penanganan buku hilang

## 2. Struktur Project

### 2.1 Direktori Utama
```
library-project/
├── include/           # Header files (.h)
├── source/           # Source files (.c)
├── data/            # File database
├── bin/             # File executable
└── build/           # File temporary dan output
```

### 2.2 File Penting
- `library.h`: Definisi struktur data dan deklarasi fungsi utama
- `peminjam.h`: Definisi fungsi terkait peminjam
- `ui.h`: Definisi fungsi antarmuka pengguna
- `view.h`: Definisi fungsi tampilan
- `main.c`: Program utama
- `library.c`: Implementasi fungsi-fungsi perpustakaan
- `admin.c`: Implementasi fungsi admin
- `peminjam.c`: Implementasi fungsi peminjam
- `ui.c`: Implementasi antarmuka pengguna
- `view.c`: Implementasi tampilan

## 3. Komponen Utama

### 3.1 Struktur Data Utama

#### 3.1.1 Buku (book_t)
```c
typedef struct {
    char isbn[LIB_MAX_ISBN];          // ISBN buku
    char title[LIB_MAX_TITLE];        // Judul buku
    char author[LIB_MAX_AUTHOR];      // Penulis
    int year;                         // Tahun terbit
    int total_stock;                  // Total stok
    int available;                    // Stok tersedia
    char notes[LIB_MAX_NOTES];        // Catatan tambahan
} book_t;
```

#### 3.1.2 Peminjam (borrower_t)
```c
typedef struct {
    char id[32];                      // ID unik internal
    char nim[32];                     // NIM mahasiswa
    char name[LIB_MAX_NAME];          // Nama lengkap
    char phone[32];                   // Nomor telepon
    char email[64];                   // Alamat email
} borrower_t;
```

#### 3.1.3 Peminjaman (loan_t)
```c
typedef struct {
    char loan_id[32];                 // ID peminjaman
    char isbn[LIB_MAX_ISBN];          // ISBN buku
    char borrower_id[32];             // ID peminjam
    lib_date_t date_borrow;           // Tanggal pinjam
    lib_date_t date_due;              // Tanggal jatuh tempo
    lib_date_t date_returned;         // Tanggal kembali
    bool is_returned;                 // Status pengembalian
    bool is_lost;                    // Status kehilangan
    long fine_paid;                  // Denda yang dibayar
} loan_t;
```

### 3.2 Fungsi-fungsi Penting

#### 3.2.1 Manajemen Database
- `lib_db_open()`: Membuka koneksi database
- `lib_db_save()`: Menyimpan perubahan ke database
- `lib_db_close()`: Menutup koneksi database

#### 3.2.2 Manajemen Buku
- `lib_add_book()`: Menambah buku baru
- `lib_remove_book()`: Menghapus buku
- `lib_find_book_by_isbn()`: Mencari buku berdasarkan ISBN
- `lib_search_books_by_title()`: Mencari buku berdasarkan judul
- `lib_update_book_stock()`: Mengupdate stok buku

#### 3.2.3 Manajemen Peminjam
- `lib_add_borrower()`: Menambah peminjam baru
- `lib_find_borrower_by_id()`: Mencari peminjam berdasarkan ID
- `lib_find_borrower_by_nim()`: Mencari peminjam berdasarkan NIM
- `lib_validate_nim_format()`: Validasi format NIM

#### 3.2.4 Manajemen Peminjaman
- `lib_checkout_book()`: Proses peminjaman buku
- `lib_return_book()`: Proses pengembalian buku
- `lib_mark_book_lost()`: Menandai buku hilang
- `lib_calculate_fine()`: Menghitung denda

## 4. Alur Program

### 4.1 Alur Menu Utama
1. Program dimulai dari `main.c`
2. Menampilkan menu utama dengan opsi:
   - Login sebagai Admin
   - Login sebagai Peminjam
   - Demo UI
   - Keluar Program

### 4.2 Alur Admin
1. Login dengan username dan password
2. Akses menu admin:
   - Manajemen buku
   - Lihat daftar peminjam
   - Lihat riwayat peminjaman
   - Pengaturan sistem

### 4.3 Alur Peminjam
1. Login/registrasi dengan NIM
2. Akses menu peminjam:
   - Lihat daftar buku
   - Cari buku
   - Pinjam buku
   - Lihat pinjaman aktif
   - Kembalikan buku
   - Lapor buku hilang

### 4.4 Alur Peminjaman Buku
1. Peminjam memilih buku berdasarkan ISBN
2. Sistem mengecek ketersediaan buku
3. Jika tersedia:
   - Membuat record peminjaman
   - Mengurangi stok buku
   - Menyimpan ke database
4. Menampilkan konfirmasi dan tanggal pengembalian

### 4.5 Alur Pengembalian Buku
1. Peminjam memilih pinjaman yang akan dikembalikan
2. Sistem menghitung denda (jika ada)
3. Memproses pengembalian:
   - Update status peminjaman
   - Menambah stok buku
   - Mencatat denda
4. Menampilkan rincian pengembalian dan denda

## 5. Detail Implementasi

### 5.1 Sistem Penyimpanan Data
- Menggunakan file CSV untuk penyimpanan data
- Terdapat tiga file utama:
  1. `library_db_books.csv`: Data buku
  2. `library_db_borrowers.csv`: Data peminjam
  3. `library_db_loans.csv`: Data peminjaman

### 5.2 Perhitungan Denda
```c
unsigned long lib_calculate_fine(const library_db_t *db,
                               lib_date_t due_date,
                               lib_date_t return_date) {
    int days = lib_date_days_between(due_date, return_date);
    if (days <= 0) return 0;
    return (unsigned long) days * (unsigned long) db->fine_per_day;
}
```
- Denda dihitung per hari keterlambatan
- Default: Rp1.000 per hari
- Menggunakan waktu sistem untuk perhitungan otomatis

### 5.3 Keamanan
- Password admin di-hash sebelum disimpan
- Validasi input untuk mencegah buffer overflow
- Atomic save untuk mencegah corrupted data
- Backup data otomatis

### 5.4 Manajemen Memori
- Alokasi memori dinamis untuk data
- Pembersihan memori otomatis saat program berakhir
- Penanganan error untuk alokasi gagal

## 6. Basis Data

### 6.1 Format File CSV

#### 6.1.1 Books CSV
```
isbn,title,author,year,total_stock,available,notes
```

#### 6.1.2 Borrowers CSV
```
id,nim,name,phone,email
```

#### 6.1.3 Loans CSV
```
loan_id,isbn,borrower_id,date_borrow,date_due,date_returned,is_returned,is_lost,fine_paid
```

### 6.2 Manajemen File
- Atomic save menggunakan file temporary
- Validasi format file
- Auto-recovery untuk file corrupt
- Backup otomatis

## 7. Panduan Penggunaan

### 7.1 Kompilasi Program
```bash
gcc -I./include source/*.c -o bin/main.exe
```

### 7.2 Menjalankan Program
```bash
./bin/main.exe
```

### 7.3 Akun Admin Default
- Username: admin
- Password: admin123

### 7.4 Tips Penggunaan
1. Selalu backup database secara berkala
2. Periksa stok buku secara rutin
3. Monitor peminjaman yang mendekati jatuh tempo
4. Verifikasi data peminjam baru
5. Lakukan perhitungan denda secara teliti

### 7.5 Penanganan Error
1. Database tidak dapat diakses:
   - Periksa permission file
   - Pastikan path benar
2. Kompilasi gagal:
   - Periksa dependensi
   - Pastikan include path benar
3. Runtime error:
   - Log error tersimpan di file log
   - Restart program jika diperlukan

## Penutup

Dokumentasi ini memberikan gambaran komprehensif tentang sistem perpustakaan digital. Untuk informasi lebih detail tentang fungsi spesifik, silakan merujuk ke komentar dalam kode sumber atau menghubungi pengembang.

---
Terakhir diperbarui: November 2025