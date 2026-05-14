# =============================================================================
# setup_build.ps1 — Cài đặt môi trường & build dự án RVM1_Interface
# Chạy bằng: PowerShell (nhấp chuột phải → "Run with PowerShell")
#            hoặc: powershell -ExecutionPolicy Bypass -File setup_build.ps1
# =============================================================================
$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " RVM1_Interface — Thiết lập môi trường build" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

# ─── 1. Kiểm tra CMake ───────────────────────────────────────────────────────
Write-Host "`n[1/5] Kiểm tra CMake..." -ForegroundColor Yellow
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "  CMake chưa cài. Đang cài qua winget..." -ForegroundColor Red
    winget install Kitware.CMake --silent
    # Reload PATH
    $env:PATH = [System.Environment]::GetEnvironmentVariable("PATH","Machine") +
                ";" + [System.Environment]::GetEnvironmentVariable("PATH","User")
}
$cmakeVer = cmake --version | Select-Object -First 1
Write-Host "  OK: $cmakeVer" -ForegroundColor Green

# ─── 2. Tìm Qt6 ──────────────────────────────────────────────────────────────
Write-Host "`n[2/5] Tìm Qt6 (MinGW 64-bit)..." -ForegroundColor Yellow

$QtRoots = @("C:\Qt","D:\Qt","E:\Qt","$env:USERPROFILE\Qt")
$QtPath  = $null
foreach ($r in $QtRoots) {
    if (Test-Path $r) {
        $versions = Get-ChildItem $r -Directory -Filter "6.*" |
                    Sort-Object Name -Descending
        foreach ($v in $versions) {
            foreach ($arch in @("mingw_64","mingw64")) {
                $candidate = Join-Path $v.FullName "$arch\lib\cmake\Qt6"
                if (Test-Path $candidate) {
                    $QtPath = Join-Path $v.FullName $arch
                    break
                }
            }
            if ($QtPath) { break }
        }
    }
    if ($QtPath) { break }
}

if (-not $QtPath) {
    Write-Host @"

  [!] Không tìm thấy Qt6 trên máy này.
  Vui lòng tải và cài Qt6 (MinGW 64-bit) từ:
    https://www.qt.io/download-qt-installer

  Sau khi cài xong, chạy lại script này.
  (Hoặc truyền thủ công: cmake -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\mingw_64 ...)
"@ -ForegroundColor Red
    Read-Host "Nhấn Enter để thoát"
    exit 1
}
Write-Host "  OK: $QtPath" -ForegroundColor Green

# Thêm MinGW vào PATH (cần cho build)
$MinGWBin = Join-Path (Split-Path $QtPath) "..\Tools\mingw_64\bin"
if (-not (Test-Path $MinGWBin)) {
    # Thử tìm trong Qt tools
    $toolsDir = Join-Path (Split-Path $QtPath) "..\..\..\Tools"
    if (Test-Path $toolsDir) {
        $mg = Get-ChildItem $toolsDir -Directory -Filter "mingw*" |
              Sort-Object Name -Descending | Select-Object -First 1
        if ($mg) { $MinGWBin = Join-Path $mg.FullName "bin" }
    }
}
if (Test-Path $MinGWBin) {
    $env:PATH = "$MinGWBin;$env:PATH"
    Write-Host "  MinGW: $MinGWBin" -ForegroundColor Green
}

# ─── 3. Tìm / cài vcpkg + Eigen3 ─────────────────────────────────────────────
Write-Host "`n[3/5] Tìm vcpkg & Eigen3..." -ForegroundColor Yellow

$VcpkgRoots = @("C:\vcpkg","D:\vcpkg",
                "$env:USERPROFILE\vcpkg","$env:LOCALAPPDATA\vcpkg")
$VcpkgRoot  = $null
foreach ($r in $VcpkgRoots) {
    if (Test-Path "$r\vcpkg.exe") { $VcpkgRoot = $r; break }
}

if (-not $VcpkgRoot) {
    Write-Host "  vcpkg chưa có. Đang clone vào C:\vcpkg..." -ForegroundColor Red
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        winget install Git.Git --silent
        $env:PATH = [System.Environment]::GetEnvironmentVariable("PATH","Machine") +
                    ";" + [System.Environment]::GetEnvironmentVariable("PATH","User")
    }
    git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
    C:\vcpkg\bootstrap-vcpkg.bat -disableMetrics
    $VcpkgRoot = "C:\vcpkg"
}
Write-Host "  vcpkg: $VcpkgRoot" -ForegroundColor Green

# Cài Eigen3 nếu chưa có
$EigenMarker = "$VcpkgRoot\installed\x64-windows\share\eigen3\copyright"
if (-not (Test-Path $EigenMarker)) {
    Write-Host "  Cài đặt Eigen3 (x64-windows)..." -ForegroundColor Yellow
    & "$VcpkgRoot\vcpkg.exe" install eigen3:x64-windows
}
Write-Host "  Eigen3: OK" -ForegroundColor Green

$Toolchain = "$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"

# ─── 4. CMake Configure ───────────────────────────────────────────────────────
Write-Host "`n[4/5] CMake configure (Debug)..." -ForegroundColor Yellow

$BuildDir = Join-Path $Root "RVM1_Interface\build\setup_build"
if (-not (Test-Path $BuildDir)) { New-Item -ItemType Directory $BuildDir | Out-Null }

$cmakeArgs = @(
    "-S", "$Root\RVM1_Interface",
    "-B", $BuildDir,
    "-G", "MinGW Makefiles",
    "-DCMAKE_BUILD_TYPE=Debug",
    "-DCMAKE_PREFIX_PATH=$QtPath",
    "-DCMAKE_TOOLCHAIN_FILE=$Toolchain"
)
Write-Host "  cmake $($cmakeArgs -join ' ')"
cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host "`n[!] CMake configure thất bại." -ForegroundColor Red; exit 1
}

# ─── 5. Build ─────────────────────────────────────────────────────────────────
Write-Host "`n[5/5] Build..." -ForegroundColor Yellow
cmake --build $BuildDir --parallel
if ($LASTEXITCODE -ne 0) {
    Write-Host "`n[!] Build thất bại." -ForegroundColor Red; exit 1
}

Write-Host "`n============================================================" -ForegroundColor Green
Write-Host " BUILD THÀNH CÔNG!" -ForegroundColor Green
Write-Host " Executable: $BuildDir\RVM1_Interface.exe" -ForegroundColor Green
Write-Host " Để tạo bản phát hành: chạy deploy.ps1" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Read-Host "`nNhấn Enter để thoát"
