<#
build-and-test.ps1
Windows PowerShell script to build the project and run an automated test sequence:
 - build the executable with gcc
 - run an automated borrow/return flow using the sample data in data/
 - capture output and validate expected strings
#>

Set-StrictMode -Version Latest
$here = Split-Path -Parent $MyInvocation.MyCommand.Definition
Push-Location $here

$srcDir = "source"
$includeDir = "include"
$buildDir = "build"
$exe = Join-Path $buildDir "main.exe"

# use an absolute build path to avoid relative-location issues when script is invoked
$buildFull = Join-Path $here $buildDir
If (-Not (Test-Path $buildFull)) { New-Item -ItemType Directory -Path $buildFull -Force | Out-Null }
# use the full path for the exe and temp files
$exe = Join-Path $buildFull "main.exe"

# Compile
Write-Host "[>] Compiling..."
$gcc = "gcc"
$srcFiles = @("$srcDir/admin.c", "$srcDir/library.c", "$srcDir/main.c", "$srcDir/peminjam.c", "$srcDir/view.c")
Write-Host "[i] Running gcc with quoted paths (avoids issues with spaces)"
$args = @('-std=c99','-Wall','-Wextra','-I', $includeDir, '-o', $exe) + $srcFiles + @('-lm')
Write-Host "gcc args: $($args -join ' ')"
& $gcc @args
if ($LASTEXITCODE -ne 0) { Write-Error "Compilation failed (exit $LASTEXITCODE). Ensure gcc is on PATH."; Exit 1 }
Write-Host "[v] Compiled: $exe"

# Test 1: borrower borrows a book and program prints Loan ID
$testInput1 = @"
2
67202301
Existing Student
08120001111
existing@student.example
1
9781234567897
3
5
3
"@
# Explanation: 2=Menu Peminjam -> login nim -> supply name/phone/email (if needed) -> 1=Pinjam buku -> type ISBN -> 3=view loans -> 5=Logout -> 3=Exit program

$inFile1 = Join-Path $buildFull "test_input1.txt"
$outFile1 = Join-Path $buildFull "out1.txt"
Set-Content -Path $inFile1 -Value $testInput1 -Encoding ASCII
Write-Host "[>] Running test flow 1 (borrow + list)"
# Use Start-Process with redirected stdin/stdout to avoid PowerShell piping quirks
$si = New-Object System.Diagnostics.ProcessStartInfo
$si.FileName = $exe
$si.RedirectStandardInput = $true
$si.RedirectStandardOutput = $true
$si.UseShellExecute = $false
$si.WorkingDirectory = $here
$p = New-Object System.Diagnostics.Process
$p.StartInfo = $si
$p.Start() | Out-Null
[System.IO.File]::OpenText($inFile1).ReadToEnd() | % { $p.StandardInput.WriteLine($_) }
$p.StandardInput.Close()
$outText = $p.StandardOutput.ReadToEnd()
$p.WaitForExit()
[System.IO.File]::WriteAllText($outFile1, $outText)
Write-Host $outText

# Extract Loan ID from output
$loanLine = Select-String -Path $outFile1 -Pattern "Loan ID:\s*(L[0-9A-Za-z]+)" -AllMatches
if ($loanLine -and $loanLine.Matches.Count -gt 0) {
    $loan = $loanLine.Matches[0].Groups[1].Value
    Write-Host "[v] Found Loan ID: $loan"
} else {
    Write-Warning "[!] Could not find Loan ID in program output. See $outFile1"
    Write-Host "[i] Test run output follows:`n"; Get-Content $outFile1 | ForEach-Object { Write-Host $_ }
    Exit 2
}

# Test 2: return the loan using Loan ID captured
$testInput2 = @"
2
67202301
2
$loan
5
3
"@
$inFile2 = Join-Path $buildFull "test_input2.txt"
$outFile2 = Join-Path $buildFull "out2.txt"
Set-Content -Path $inFile2 -Value $testInput2 -Encoding ASCII
Write-Host "[>] Running test flow 2 (return)"
$si2 = New-Object System.Diagnostics.ProcessStartInfo
$si2.FileName = $exe
$si2.RedirectStandardInput = $true
$si2.RedirectStandardOutput = $true
$si2.UseShellExecute = $false
$si2.WorkingDirectory = $here
$p2 = New-Object System.Diagnostics.Process
$p2.StartInfo = $si2
$p2.Start() | Out-Null
[System.IO.File]::OpenText($inFile2).ReadToEnd() | % { $p2.StandardInput.WriteLine($_) }
$p2.StandardInput.Close()
$outText2 = $p2.StandardOutput.ReadToEnd()
$p2.WaitForExit()
[System.IO.File]::WriteAllText($outFile2, $outText2)
Write-Host $outText2

# Check results
$out2 = Get-Content $outFile2 -Raw
if ($out2 -match "Pengembalian berhasil" -and $out2 -match "Database disimpan otomatis") {
    Write-Host "[v] Return flow succeeded and DB auto-saved."
    Exit 0
} else {
    Write-Warning "[!] Return flow did not show expected success messages. See $outFile2"
    Exit 3
}

Pop-Location