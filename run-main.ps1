param(
    [int]$Theme = 0,
    [switch]$ShowDemo
)

# Run the main binary from bin/. Optionally set a LIB_BG theme code (256-color).
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
$exe = Join-Path $scriptRoot "bin\main.exe"
if ($Theme -ne 0) {
    Write-Output "Setting LIB_BG=$Theme for this run"
    $env:LIB_BG = [string]$Theme
}
if ($ShowDemo) {
    Write-Output "Enabling demo on startup (LIB_SHOW_DEMO=1)"
    $env:LIB_SHOW_DEMO = '1'
}
if (-not (Test-Path $exe)) {
    Write-Output "Binary not found: $exe"
    Write-Output "Build the project first (e.g. use the Makefile or your build task to produce bin\main.exe)"
    exit 1
}
Write-Output "Running: $exe"
if ($Theme -ne 0) {
    # Emit escape to set background and clear screen before launching so change is visible
    $esc = [char]27
    # 48;5;{n} sets 256-color background, 2J clears, H moves cursor home
    Write-Host ("{0}[48;5;{1}m{0}[2J{0}[H" -f $esc, $Theme) -NoNewline
}
& $exe
