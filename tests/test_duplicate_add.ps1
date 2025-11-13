# Test script: attempt to add a book then add again (duplicate) and update stock
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$exe = Join-Path $scriptDir '..\bin\main.exe'
$exe = Resolve-Path -Path $exe -ErrorAction SilentlyContinue
if (-not $exe) {
    Write-Host "Executable not found: ..\bin\main.exe (run build first)"
    exit 1
}
$exe = $exe.Path

$input = @()
# Login as admin
$input += '1'
$input += 'Kakak Admin'
$input += '1234'

# Add a new book (option 2)
$input += '2'
$input += '999TEST'
$input += 'Test Duplicate Book'
$input += 'Test Author'
$input += '2025'
$input += '2'
$input += '5000.00'
$input += 'Notes here'

# Press Enter to continue at admin menu (simulates Enter)
$input += ''

# Try adding same ISBN again
$input += '2'
$input += '999TEST'
# When prompted to update, say Y
$input += 'Y'
# add 1 stock
$input += '1'
# do not change price
$input += 'N'

# Exit admin and program
$input += '0'
$input += '5'

# Run program with piped input
$input -join "`n" | & $exe

Write-Host "Test complete. Check output above for duplicate handling." 
