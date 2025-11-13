Developer notes — build & run (Windows PowerShell)

Quick steps

1. Build the main program (creates `source/main.exe`):

   mingw32-make

2. Build helper to create admin (optional):

   mingw32-make create-admin

   then run:

   .\source\create_admin.exe

   This will attempt to add the admin account `Kakak Admin` with password `1234`.

3. Run the main program (interactive):

   .\source\main.exe

4. Run the automated smoke test (uses `run_input.txt`):

   mingw32-make test

Notes
- Data files are stored in the `data/` folder: `data/library_db_admins.csv`, `data/library_db_books.csv`, etc.
- If you want to remove the helper, delete `source/create_admin.c` and run `mingw32-make clean`.
