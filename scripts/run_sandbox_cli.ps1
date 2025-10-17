param(
    [string] = "build",
    [int] = 120,
    [switch],
    [switch],
    [switch],
    [switch]
)

Set-StrictMode -Version Latest
Continue = "Stop"

 = Resolve-Path -LiteralPath (Join-Path  "..")
 = Join-Path  

if (-not (Test-Path )) {
    New-Item -ItemType Directory -Path  | Out-Null
}

if ( -or -not (Test-Path (Join-Path  "CMakeCache.txt"))) {
    Write-Host "Configuring CMake project in ..." -ForegroundColor Cyan
    & cmake -S  -B 
}

if ( -or ) {
    Write-Host "Building project..." -ForegroundColor Cyan
    & cmake --build  --target genesis_sandbox_cli
} else {
    & cmake --build  --target genesis_sandbox_cli | Out-Null
}

 = Join-Path  "src/genesis-sandbox-cli.exe"
if (-not (Test-Path )) {
    throw "Failed to locate genesis-sandbox-cli.exe in /src"
}

 = Join-Path  "data/ascii_layout.json"

Write-Host "Launching sandbox CLI..." -ForegroundColor Green

 = @()
if (Test-Path ) {
     += "--layout"
     += 
}
if () {
     += "--no-clear"
}
if (-not ) {
     += "--commands"
     += "step ;quit"
}

&  @cliArgs
