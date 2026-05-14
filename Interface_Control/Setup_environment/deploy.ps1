# =============================================================================
# deploy.ps1 — Đóng gói bản Release + copy DLL Qt bằng windeployqt
# Tạo ra thư mục  dist\RVM1_Interface\  sẵn sàng phát hành (zip hoặc installer)
#
# Chạy bằng: PowerShell (nhấp chuột phải → "Run with PowerShell")
#            hoặc: powershell -ExecutionPolicy Bypass -File deploy.ps1
# =============================================================================
$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " RVM1_Interface — Đóng gói bản Release" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

# ─── 1. Tìm Qt6 ──────────────────────────────────────────────────────────────
Write-Host "`n[1/4] Tìm Qt6..." -ForegroundColor Yellow

$QtRoots = @("C:\Qt","D:\Qt","E:\Qt","$env:USERPROFILE\Qt")
$QtBin   = $null
foreach ($r in $QtRoots) {
    if (Test-Path $r) {
        $versions = Get-ChildItem $r -Directory -Filter "6.*" |
                    Sort-Object Name -Descending
        foreach ($v in $versions) {
            foreach ($arch in @("mingw_64","mingw64")) {
                $candidate = Join-Path $v.FullName "$arch\bin\windeployqt.exe"
                if (Test-Path $candidate) {
                    $QtBin = Join-Path $v.FullName "$arch\bin"
                    break
                }
            }
            if ($QtBin) { break }
        }
    }
    if ($QtBin) { break }
}

if (-not $QtBin) {
    Write-Host "[!] Không tìm thấy windeployqt.exe. Cài Qt6 trước." -ForegroundColor Red
    exit 1
}
$WinDeployQt = Join-Path $QtBin "windeployqt.exe"
Write-Host "  windeployqt: $WinDeployQt" -ForegroundColor Green

# Thêm MinGW vào PATH
$QtRoot  = Split-Path (Split-Path $QtBin)   # C:\Qt\6.x.x
$ToolsRoot = Join-Path $QtRoot "..\..\Tools"
if (Test-Path $ToolsRoot) {
    $mg = Get-ChildItem $ToolsRoot -Directory -Filter "mingw*" |
          Sort-Object Name -Descending | Select-Object -First 1
    if ($mg) {
        $env:PATH = "$($mg.FullName)\bin;$QtBin;$env:PATH"
    }
} else {
    $env:PATH = "$QtBin;$env:PATH"
}

# ─── 2. Build Release ─────────────────────────────────────────────────────────
Write-Host "`n[2/4] Build Release..." -ForegroundColor Yellow

# Tìm vcpkg
$VcpkgRoots = @("C:\vcpkg","D:\vcpkg",
                "$env:USERPROFILE\vcpkg","$env:LOCALAPPDATA\vcpkg")
$Toolchain  = $null
foreach ($r in $VcpkgRoots) {
    $tc = "$r\scripts\buildsystems\vcpkg.cmake"
    if (Test-Path $tc) { $Toolchain = $tc; break }
}
if (-not $Toolchain) {
    Write-Host "[!] Không tìm thấy vcpkg. Chạy setup_build.ps1 trước." -ForegroundColor Red
    exit 1
}

$QtPath   = Split-Path $QtBin   # C:\Qt\6.x.x\mingw_64
$BuildDir = Join-Path $Root "RVM1_Interface\build\release_deploy"

if (-not (Test-Path $BuildDir)) { New-Item -ItemType Directory $BuildDir | Out-Null }

cmake -S "$Root\RVM1_Interface" -B $BuildDir `
      -G "MinGW Makefiles" `
      -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_PREFIX_PATH="$QtPath" `
      -DCMAKE_TOOLCHAIN_FILE="$Toolchain"

if ($LASTEXITCODE -ne 0) { Write-Host "[!] Configure thất bại." -ForegroundColor Red; exit 1 }

cmake --build $BuildDir --parallel --config Release
if ($LASTEXITCODE -ne 0) { Write-Host "[!] Build thất bại." -ForegroundColor Red; exit 1 }

$ExePath = "$BuildDir\RVM1_Interface.exe"
Write-Host "  Executable: $ExePath" -ForegroundColor Green

# ─── 3. Tạo thư mục dist & chạy windeployqt ─────────────────────────────────
Write-Host "`n[3/4] Đóng gói với windeployqt..." -ForegroundColor Yellow

$DistDir = Join-Path $Root "dist\RVM1_Interface"
if (Test-Path $DistDir) { Remove-Item $DistDir -Recurse -Force }
New-Item -ItemType Directory $DistDir | Out-Null

# Copy exe
Copy-Item $ExePath $DistDir

# Chạy windeployqt — copy Qt DLL, QML, plugins vào cùng thư mục exe
& $WinDeployQt --release --no-translations --no-system-d3d-compiler `
               --dir $DistDir "$DistDir\RVM1_Interface.exe"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] windeployqt thất bại." -ForegroundColor Red; exit 1
}

# Copy assets (model 3D, icons)
if (Test-Path "$BuildDir\assets") {
    Copy-Item "$BuildDir\assets" "$DistDir\assets" -Recurse
}

# ─── 4. Nén thành ZIP ────────────────────────────────────────────────────────
Write-Host "`n[4/4] Tạo file ZIP..." -ForegroundColor Yellow

$ZipPath = Join-Path $Root "dist\RVM1_Interface_release.zip"
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Compress-Archive -Path $DistDir -DestinationPath $ZipPath

Write-Host "`n============================================================" -ForegroundColor Green
Write-Host " ĐÓNG GÓI THÀNH CÔNG!" -ForegroundColor Green
Write-Host " Thư mục : $DistDir" -ForegroundColor Green
Write-Host " File ZIP : $ZipPath" -ForegroundColor Green
Write-Host " Chỉ cần gửi file ZIP — máy nhận KHÔNG cần cài Qt." -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Read-Host "`nNhấn Enter để thoát"
