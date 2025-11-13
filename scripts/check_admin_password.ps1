param(
    [Parameter(Mandatory=$true)][string]$Username,
    [Parameter(Mandatory=$false)][string]$Password
)

function Compute-AdminHash([string]$pwd) {
    Add-Type -AssemblyName System.Numerics | Out-Null
    $h = [System.Numerics.BigInteger]::Parse("1469598103934665603")
    $prime = [System.Numerics.BigInteger]::Parse("1099511628211")
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($pwd)
    foreach ($b in $bytes) { $h = ($h -bxor $b) * $prime }
    $mask = ([System.Numerics.BigInteger]::One -shl 32) - 1
    $lowBig = $h -band $mask
    $low = [uint32]$lowBig
    return ("{0:x8}" -f $low)
}

if (-not $Password) {
    $secure = Read-Host -AsSecureString "Password"
    $ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    $Password = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr)
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr)
}

$hash = Compute-AdminHash $Password
# Resolve CSV path relative to script location
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
$projectRoot = Resolve-Path (Join-Path $scriptRoot "..")
$csv = Join-Path $projectRoot "data\library_db_admins.csv"
if (-not (Test-Path $csv)) { Write-Error "Admin CSV not found at $csv"; exit 2 }

$found = $false
Get-Content $csv | ForEach-Object {
    $line = $_.Trim()
    if ($line -eq '') { return }
    $parts = $line -split ",", 2
    if ($parts.Count -lt 2) { return }
    $u = $parts[0].Trim(); $h = $parts[1].Trim()
    if ($u -eq $Username) {
        $found = $true
        if ($h -ieq $hash) { Write-Output "OK: Password matches for user '$Username'"; exit 0 }
        else { Write-Output "FAIL: Password does NOT match for user '$Username'"; exit 1 }
    }
}
if (-not $found) { Write-Output "NOT FOUND: No admin entry for user '$Username'"; exit 3 }
