# Build and run script for Windows
Write-Host "Cleaning previous build..."
if (Test-Path "source\main.exe") {
    Remove-Item "source\main.exe"
}

Write-Host "Building program..."
$buildCommand = "gcc -std=c99 -Wall -Wextra -Iinclude -o source/main.exe source/main.c source/admin.c source/library.c source/peminjam.c source/ui.c source/view.c"
Invoke-Expression $buildCommand

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful! Running program..."
    Set-Location source
    ./main.exe
} else {
    Write-Host "Build failed with error code $LASTEXITCODE"
    exit $LASTEXITCODE
}