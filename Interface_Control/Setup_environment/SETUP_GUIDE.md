# Hướng dẫn cài đặt & phát hành RVM1_Interface

## Yêu cầu tối thiểu

| Phần mềm | Phiên bản | Tải về |
|---|---|---|
| **Qt 6** (MinGW 64-bit) | 6.5 trở lên | https://www.qt.io/download-qt-installer |
| **CMake** | 3.19 trở lên | https://cmake.org/download/ (hoặc `winget install Kitware.CMake`) |
| **Git** | bất kỳ | https://git-scm.com/ (hoặc `winget install Git.Git`) |
| **vcpkg** | — | Script tự clone nếu chưa có |

> **Lưu ý:** Chỉ cần cài Qt6 trước, phần còn lại script tự xử lý.

---

## Dành cho thành viên nhóm — Build để lập trình

### Chạy `setup_build.ps1`

```powershell
powershell -ExecutionPolicy Bypass -File setup_build.ps1
```

Hoặc: **chuột phải** vào file → **"Run with PowerShell"**

**Script này sẽ tự động:**
1. Kiểm tra và cài CMake (qua `winget`) nếu chưa có
2. Tìm Qt6 trong `C:\Qt`, `D:\Qt`, `E:\Qt`, `%USERPROFILE%\Qt` — ưu tiên phiên bản mới nhất
3. Tìm hoặc clone vcpkg, sau đó cài thư viện **Eigen3**
4. Chạy `cmake configure`
5. Chạy `cmake build`

**Kết quả:** File thực thi tại:
```
RVM1_Interface\build\setup_build\RVM1_Interface.exe
```

### Nếu Qt không tự tìm được

Truyền đường dẫn thủ công khi chạy cmake:
```powershell
cmake -S RVM1_Interface -B RVM1_Interface\build\manual `
      -G "MinGW Makefiles" `
      -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\mingw_64" `
      -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake"
cmake --build RVM1_Interface\build\manual --parallel
```

---

## Dành cho phát hành — Tạo bản chạy được trên máy không cài Qt

### Chạy `deploy.ps1`

```powershell
powershell -ExecutionPolicy Bypass -File deploy.ps1
```

**Script này sẽ tự động:**
1. Tìm Qt6 và `windeployqt.exe`
2. Build bản **Release**
3. Chạy `windeployqt` — copy toàn bộ Qt DLL, plugin cần thiết vào thư mục
4. Nén thành file ZIP

**Kết quả:**
```
dist\
  RVM1_Interface\        ← thư mục chứa .exe + tất cả DLL
  RVM1_Interface_release.zip  ← file này gửi cho người dùng
```

> Người nhận chỉ cần **giải nén và chạy** — không cần cài Qt, không cần cài gì thêm.

---

## Sơ đồ tóm tắt

```
Thành viên nhóm (dev)          Người dùng cuối
─────────────────────          ────────────────
Cài Qt6                        Nhận file ZIP
     ↓
setup_build.ps1                Giải nén
     ↓
Build Debug/Release            Double-click RVM1_Interface.exe
     ↓
(chỉnh sửa code...)
     ↓
deploy.ps1
     ↓
dist/RVM1_Interface_release.zip  ──────────────→  Gửi cho người dùng
```

---

## Câu hỏi thường gặp

**Q: Script báo "Qt6 không tìm thấy"?**  
A: Đảm bảo đã cài Qt6 với component **MinGW 64-bit**. Kiểm tra trong Qt Maintenance Tool.

**Q: Script báo lỗi ExecutionPolicy?**  
A: Chạy lệnh sau một lần duy nhất trong PowerShell (quyền Admin):
```powershell
Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
```

**Q: Build lỗi "Eigen3 not found"?**  
A: Chạy thủ công:
```powershell
C:\vcpkg\vcpkg.exe install eigen3:x64-windows
```

**Q: windeployqt báo lỗi khi deploy?**  
A: Đảm bảo Qt6 được cài đúng phiên bản và đường dẫn `bin\windeployqt.exe` tồn tại trong thư mục Qt.
