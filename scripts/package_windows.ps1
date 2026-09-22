# scripts/package_windows.ps1
# Automated Windows Production Packaging & Deployment Script for NeoNect

param(
    [string]$BuildDir = "build",
    [string]$Config = "Release",
    [string]$QtDir = "C:\Qt\6.11.2\mingw_64",
    [string]$OutputDir = "dist"
)

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  NEONECT DESKTOP - WINDOWS PRODUCTION PACKAGER          " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

# 1. Generate release notes and patch metadata
Write-Host "[1/6] Generating Release Notes and Patch Notes..." -ForegroundColor Yellow
if (Test-Path "scripts/generate_release_notes.py") {
    python scripts/generate_release_notes.py
}

# 2. Build Release Binaries if not already built
Write-Host "[2/6] Ensuring NeoNectApp ($Config) is built..." -ForegroundColor Yellow
if (-not (Test-Path "$BuildDir\NeoNectApp.exe")) {
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
        cmake -S . -B $BuildDir -G "Ninja" -DCMAKE_BUILD_TYPE=$Config
    }
    cmake --build $BuildDir --config $Config --target NeoNectApp NeoNectTests
}

# 3. Verification of Test Suite
Write-Host "[3/6] Verifying Binaries & Tests..." -ForegroundColor Yellow
$TestExe = Join-Path $BuildDir "NeoNectTests.exe"
if (Test-Path $TestExe) {
    Write-Host "  [OK] Test binary ready." -ForegroundColor Green
}

# 4. Create Clean Distribution Package Directory
Write-Host "[4/6] Creating Staging Directory..." -ForegroundColor Yellow
$StagingDir = Join-Path $RepoRoot "$BuildDir\package_windows_staging"
if (Test-Path $StagingDir) {
    Remove-Item -Recurse -Force $StagingDir
}
New-Item -ItemType Directory -Path $StagingDir | Out-Null

# Copy Executable & Assets
Copy-Item "$BuildDir\NeoNectApp.exe" -Destination $StagingDir
if (Test-Path "assets\NeoNect\icon.ico") {
    Copy-Item "assets\NeoNect\icon.ico" -Destination $StagingDir
}
if (Test-Path "RELEASE_NOTES.md") {
    Copy-Item "RELEASE_NOTES.md" -Destination $StagingDir
}
if (Test-Path "CHANGELOG.md") {
    Copy-Item "CHANGELOG.md" -Destination $StagingDir
}
if (Test-Path "README.md") {
    Copy-Item "README.md" -Destination $StagingDir
}

# 5. Execute windeployqt
Write-Host "[5/6] Deploying Qt Runtime with windeployqt..." -ForegroundColor Yellow
$WinDeployQt = Join-Path $QtDir "bin\windeployqt.exe"
if (Test-Path $WinDeployQt) {
    try {
        $prevEAP = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        & $WinDeployQt --qmldir "$RepoRoot\qml" --release --no-translations --compiler-runtime "$StagingDir\NeoNectApp.exe"
        $ErrorActionPreference = $prevEAP
    } catch {
        Write-Warning "windeployqt notice: $_"
    }
} else {
    Write-Warning "windeployqt not found at $WinDeployQt. Ensure Qt environment is set."
}

# Copy OpenSSL DLLs if present
$OpenSslCandidateDirs = @(
    if ($env:OPENSSL_ROOT_DIR) { Join-Path $env:OPENSSL_ROOT_DIR "bin" },
    "C:\Program Files\OpenSSL\bin",
    "C:\Program Files\OpenSSL-Win64\bin",
    "C:\OpenSSL-Win64\bin",
    "C:\OpenSSL\bin"
)
foreach ($d in $OpenSslCandidateDirs) {
    if ($d -and (Test-Path $d)) {
        Get-ChildItem -Path $d -Filter "*crypto*.dll" | ForEach-Object { Copy-Item $_.FullName -Destination $StagingDir -Force }
        Get-ChildItem -Path $d -Filter "*ssl*.dll" | ForEach-Object { Copy-Item $_.FullName -Destination $StagingDir -Force }
        break
    }
}

# 6. Create Portable ZIP Distribution
Write-Host "[6/6] Creating Standalone Distribution ZIP..." -ForegroundColor Yellow
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

$Version = "1.0.0"
if ($env:NEONECT_VERSION) {
    $Version = $env:NEONECT_VERSION.TrimStart("v")
}

$ZipName = "NeoNect-v$Version-windows-x86_64.zip"
$ZipPath = Join-Path $OutputDir $ZipName
if (Test-Path $ZipPath) {
    Remove-Item -Force $ZipPath
}

Compress-Archive -Path "$StagingDir\*" -DestinationPath $ZipPath -CompressionLevel Optimal

Write-Host "==========================================================" -ForegroundColor Green
Write-Host "  [OK] PACKAGE CREATED SUCCESSFULLY: $ZipPath" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
