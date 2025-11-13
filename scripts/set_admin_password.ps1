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
    $secure = Read-Host -AsSecureString "New password for $Username"
    $ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    $Password = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr)
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr)
}

 $hash = Compute-AdminHash $Password
 # Resolve CSV path relative to script location
 $scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
 $projectRoot = Resolve-Path (Join-Path $scriptRoot "..")
 $csvPath = Join-Path $projectRoot "data\library_db_admins.csv"
if (-not (Test-Path $csvPath)) {
    New-Item -Path $csvPath -ItemType File -Force | Out-Null
}

# Read file, update or append
$lines = Get-Content $csvPath -ErrorAction SilentlyContinue | ForEach-Object { $_ }
$found = $false
for ($i = 0; $i -lt $lines.Length; $i++) {
    $line = $lines[$i].Trim()
    if ($line -eq '') { continue }
    $parts = $line -split ",",2
    if ($parts.Count -lt 2) { continue }
    $u = $parts[0].Trim()
    if ($u -eq $Username) {
        $lines[$i] = "$Username,$hash"
        $found = $true
        break
    }
}
if (-not $found) { $lines += "$Username,$hash" }

$lines | Set-Content $csvPath -Force
Write-Output "Updated admin CSV: $csvPath. User: $Username"
