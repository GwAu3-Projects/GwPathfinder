# ===================================================================
# GWPathfinder - Complete Build Script
# Usage: .\Build.ps1
# ===================================================================

param(
    [switch]$Clean,
    [switch]$Debug
)

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  GWPathfinder Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Configuration
$BuildType = if ($Debug) { "Debug" } else { "Release" }
$VcpkgRoot = "C:\vcpkg"
$VcpkgExe = Join-Path $VcpkgRoot "vcpkg.exe"
$VcpkgToolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"

Write-Host "Configuration: $BuildType" -ForegroundColor Gray
Write-Host ""

# Step 1: Check and install vcpkg dependencies
Write-Host "[1/3] vcpkg dependencies..." -ForegroundColor Yellow

if (-not (Test-Path $VcpkgExe)) {
    Write-Host "  ERROR: vcpkg not found in $VcpkgRoot" -ForegroundColor Red
    Write-Host "  Install from: https://github.com/microsoft/vcpkg" -ForegroundColor Gray
    exit 1
}

Write-Host "  Installing packages (x86-windows-static)..." -ForegroundColor Gray
$env:VCPKG_DEFAULT_TRIPLET = "x86-windows-static"
Push-Location $PSScriptRoot
& $VcpkgExe install --triplet x86-windows-static
Pop-Location

if ($LASTEXITCODE -ne 0) {
    Write-Host "  ERROR: vcpkg installation failed" -ForegroundColor Red
    exit 1
}
Write-Host "  OK" -ForegroundColor Green

# Step 2: CMake Configuration
Write-Host ""
Write-Host "[2/3] CMake configuration..." -ForegroundColor Yellow

if ($Clean -and (Test-Path "build")) {
    Write-Host "  Cleaning..." -ForegroundColor Gray
    Remove-Item "build" -Recurse -Force
}

if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

Push-Location "build"

cmake .. `
    -G "Visual Studio 17 2022" `
    -A Win32 `
    -DCMAKE_BUILD_TYPE=$BuildType `
    -DVCPKG_TARGET_TRIPLET=x86-windows-static `
    -DCMAKE_TOOLCHAIN_FILE="$VcpkgToolchain" | Out-Host

if ($LASTEXITCODE -ne 0) {
    Write-Host "  ERROR: CMake configuration failed" -ForegroundColor Red
    Pop-Location
    exit 1
}

Write-Host "  OK" -ForegroundColor Green

# Step 3: Build
Write-Host ""
Write-Host "[3/3] Building..." -ForegroundColor Yellow

cmake --build . --config $BuildType --parallel | Out-Host

if ($LASTEXITCODE -ne 0) {
    Write-Host "  ERROR: Build failed" -ForegroundColor Red
    Pop-Location
    exit 1
}

Pop-Location

Write-Host "  OK" -ForegroundColor Green

# Final result
Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  BUILD SUCCESSFUL!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""

$DllPath = "build\$BuildType\GWPathfinder.dll"
if (Test-Path $DllPath) {
    $DllSize = (Get-Item $DllPath).Length / 1MB

    Write-Host "Created files:" -ForegroundColor Cyan
    Write-Host "  DLL: $DllPath ($([math]::Round($DllSize, 1)) MB)" -ForegroundColor Gray

    $TestServerPath = "build\$BuildType\TestServer.exe"
    if (Test-Path $TestServerPath) {
        $ExeSize = (Get-Item $TestServerPath).Length / 1KB
        Write-Host "  EXE: $TestServerPath ($([math]::Round($ExeSize, 0)) KB)" -ForegroundColor Gray
    }

    # Check maps.rar status
    $OutputRar = "build\$BuildType\maps.rar"
    if (Test-Path $OutputRar) {
        $RarSize = (Get-Item $OutputRar).Length / 1MB
        Write-Host "  RAR: $OutputRar ($([math]::Round($RarSize, 1)) MB)" -ForegroundColor Gray
    } else {
        Write-Host ""
        Write-Host "  NOTE: maps.rar not found in output directory." -ForegroundColor Yellow
        Write-Host "  Place maps.rar or maps/ folder next to GWPathfinder.dll" -ForegroundColor Yellow
    }

    if (Test-Path $TestServerPath) {
        Write-Host ""
        Write-Host "To test with DLL Tester:" -ForegroundColor Yellow
        Write-Host "  cd build\$BuildType" -ForegroundColor Gray
        Write-Host "  .\TestServer.exe" -ForegroundColor Gray
        Write-Host "  Then open http://localhost:8080 in your browser" -ForegroundColor Gray
    }
} else {
    Write-Host "ERROR: DLL not created!" -ForegroundColor Red
    exit 1
}

Write-Host ""
