# Hướng Dẫn Xây Dựng Giao Diện Điều Khiển Robot RV-M1 (C++)

## Tổng Quan Dự Án

Xây dựng phần mềm giao diện điều khiển robot cánh tay **Mitsubishi RV-M1** (6 bậc tự do) bằng **C++**, gồm:
- Môi trường mô phỏng 3D robot
- Giao diện Virtual Teachbox (tham khảo file `InterfaceRobot.pdf`)
- Kết nối COM port để nhận lệnh từ phần mềm ngoài (STM32, Python, v.v.)

---

## Công Nghệ Sử Dụng

| Thành phần | Công cụ | Lý do |
|---|---|---|
| GUI + Serial Port | **Qt 6** (Qt Widgets + Qt SerialPort) | Miễn phí, C++, hỗ trợ COM port sẵn |
| Render 3D | **Qt3D** hoặc **OpenGL + Qt** | Tích hợp trong Qt, không cần thư viện ngoài |
| Load file 3D | **Assimp** (Open Asset Import Library) | Hỗ trợ OBJ, STL, STEP export |
| Tính toán ma trận | **Eigen 3** | Thư viện C++ chuẩn cho ma trận/vector |
| Build system | **CMake** | Chuẩn C++ hiện đại |

---

## Các Bước Thực Hiện

---

### BƯỚC 1: Chuẩn Bị File 3D Robot (CATPart → STL) bằng SolidWorks

File đã tải về gồm **24 file `.CATPart`** và **1 file `robot.CATProduct`** (định dạng CATIA).
SolidWorks mở trực tiếp được cả 2 định dạng này, cần convert sang **STL** để dùng trong Qt3D/OpenGL.

---

#### 1.1 Mở Robot và Xác Nhận Cấu Trúc Khâu

1. Mở SolidWorks
2. `File` → `Open` → chọn file `robot.CATProduct` → nhấn **Open**
3. Hộp thoại hiện ra hỏi về import options → chọn **OK** (giữ mặc định)
4. Robot 3D hiện ra đầy đủ — quan sát **Feature Tree** bên trái cửa sổ
5. Click vào từng bộ phận trên màn hình 3D → Feature Tree sẽ highlight tên file `.CATPart` tương ứng
6. Xác nhận nhóm khâu dựa theo bảng dưới:

| Khâu | File CATPart thuộc về | Tên STL output |
|------|----------------------|----------------|
| Link 0 — Đế | `BASE.CATPart`, `BASE2.CATPart`, `BASE2A.CATPart`, `BASE2B.CATPart`, `Part1.CATPart` | `link0_base.stl` |
| Link 1 — Eo (Waist) | `motoraxis1.CATPart`, `ARM1.CATPart`, `ARM1A.CATPart` | `link1_waist.stl` |
| Link 2 — Vai (Shoulder) | `ARM2.CATPart`, `ARM2A.CATPart`, `axis2A.CATPart`, `axis2B.CATPart`, `motoraxis2.CATPart`, `axismotor2.CATPart`, `pasek1.CATPart`, `pasek2.CATPart` | `link2_shoulder.stl` |
| Link 3 — Khuỷu (Elbow) | `axis3.CATPart`, `motoraxis3.CATPart`, `pasek3.CATPart` | `link3_elbow.stl` |
| Link 4 — Cổ tay Pitch | `axis4.CATPart`, `pasek4.CATPart` | `link4_wrist_pitch.stl` |
| Link 5 — Cổ tay Roll | `motoraxis5.CATPart` | `link5_wrist_roll.stl` |
| Link 6 — Tay kẹp | `GRIPPER1.CATPart`, `GRIPPER2.CATPart` | `link6_gripper.stl` |

> **Lưu ý:** Bảng trên là ước tính dựa theo tên file. Sau khi mở `robot.CATProduct` trong SolidWorks và click vào từng bộ phận, hãy điều chỉnh lại nhóm nếu cần.

---

#### 1.2 Convert Hàng Loạt CATPart → STL

##### Cách duy nhất hoạt động — SolidWorks Macro trên Assembly đang mở

> **Nguyên lý:** SolidWorks CÓ THỂ mở CATPart thủ công (translator chạy ở chế độ GUI).
> Macro bên dưới **KHÔNG mở lại file** mà làm việc trực tiếp trên assembly **đang mở sẵn** → không cần license API.

**Các bước:**

1. Trong SolidWorks: `File` → `Open` → chọn `robot.CATProduct` → **Open** → chờ load xong
2. `Tools` → `Macro` → `New...` → đặt tên `ExportSTL_FromAssy` → **Save**
3. **Xóa toàn bộ code mặc định**, paste đoạn sau:

```vbnet
' Macro SolidWorks VBA: Xuất tất cả Parts trong Assembly đang mở → STL
' Yêu cầu: robot.CATProduct phải đang mở trong SolidWorks (mở thủ công)
' KHÔNG cần CATIA Translator Add-in vì không mở file qua API

Dim swApp As SldWorks.SldWorks

Sub main()
    Set swApp = Application.SldWorks

    ' Lấy assembly đang active (robot.CATProduct đang mở)
    Dim swAssy As SldWorks.ModelDoc2
    Set swAssy = swApp.ActiveDoc

    If swAssy Is Nothing Then
        MsgBox "Khong co file nao dang mo!" & vbCrLf & "Hay mo robot.CATProduct truoc.", vbExclamation
        Exit Sub
    End If

    Dim sOutput As String
    sOutput = "e:\DACN_HandRoBot\Eminel\DACN_HandRobot_STM32\Interface_Control\3D\STL_parts\"
    If Dir(sOutput, vbDirectory) = "" Then MkDir sOutput

    Dim swAssyDoc As SldWorks.AssemblyDoc
    Set swAssyDoc = swAssy

    ' Lấy tất cả components trong assembly (bao gồm sub-assembly)
    Dim vComps As Variant
    vComps = swAssyDoc.GetComponents(False)

    If IsEmpty(vComps) Then
        MsgBox "Khong tim thay components trong assembly!", vbExclamation
        Exit Sub
    End If

    Dim count As Integer
    count = 0
    Dim savedNames() As String
    ReDim savedNames(UBound(vComps))
    Dim savedCount As Integer
    savedCount = 0

    Dim i As Integer
    For i = 0 To UBound(vComps)
        Dim swComp As SldWorks.Component2
        Set swComp = vComps(i)

        Dim swCompModel As SldWorks.ModelDoc2
        Set swCompModel = swComp.GetModelDoc2

        If Not swCompModel Is Nothing Then
            ' Lấy tên file (bỏ đường dẫn và extension)
            Dim sFullPath As String
            sFullPath = swCompModel.GetPathName

            Dim sName As String
            sName = Mid(sFullPath, InStrRev(sFullPath, "\") + 1)
            Dim dotPos As Integer
            dotPos = InStrRev(sName, ".")
            If dotPos > 0 Then sName = Left(sName, dotPos - 1)
            ' Bỏ khoảng trắng thừa
            sName = Trim(sName)

            ' Kiểm tra đã xuất file này chưa (tránh trùng lặp)
            Dim alreadySaved As Boolean
            alreadySaved = False
            Dim j As Integer
            For j = 0 To savedCount - 1
                If LCase(savedNames(j)) = LCase(sName) Then
                    alreadySaved = True
                    Exit For
                End If
            Next j

            If Not alreadySaved Then
                Dim sSTLPath As String
                sSTLPath = sOutput & sName & ".stl"

                Dim errors As Long, warnings As Long
                Dim bRet As Boolean
                bRet = swCompModel.SaveAs2(sSTLPath, swSaveAsCurrentVersion, True, False)

                If bRet Then
                    savedNames(savedCount) = sName
                    savedCount = savedCount + 1
                    count = count + 1
                End If
            End If
        End If

        Set swCompModel = Nothing
        Set swComp = Nothing
    Next i

    MsgBox "Hoan thanh! Da xuat " & count & " file STL vao:" & vbCrLf & sOutput, vbInformation, "Xong"
End Sub
```

4. Nhấn **F5** để chạy → macro tự động lưu tất cả parts thành STL vào `3D\STL_parts\`

---

#### 1.3 Gộp STL Theo Khâu Bằng Python (chạy trong PowerShell bình thường)

Sau khi có các file STL riêng lẻ trong `STL_parts\`, chạy script này để gộp thành 7 file theo khâu:

```powershell
pip install numpy-stl
python "e:\DACN_HandRoBot\Eminel\DACN_HandRobot_STM32\Interface_Control\3D\merge_stl_by_link.py"
```

Script [3D/merge_stl_by_link.py](3D/merge_stl_by_link.py) đã được tạo sẵn trong workspace, tự động gộp theo bảng nhóm khâu ở mục 1.1.

---

#### 1.4 Kết Quả Mong Đợi

Sau bước này, thư mục `assets/models/` sẽ có:
```
assets/models/
├── link0_base.stl
├── link1_waist.stl
├── link2_shoulder.stl
├── link3_elbow.stl
├── link4_wrist_pitch.stl
├── link5_wrist_roll.stl
└── link6_gripper.stl
```

> **Lưu ý:** Phải tách từng khâu riêng để có thể xoay độc lập khi mô phỏng.

---

### BƯỚC 2: Cài Đặt Môi Trường Lập Trình

#### 2.1 Cài Qt 6
- Tải Qt Online Installer: https://www.qt.io/download-qt-installer
- Chọn cài: `Qt 6.x` + `Qt Creator` + `MinGW 64-bit` (hoặc MSVC)
- Tích thêm: `Qt Serial Port`, `Qt 3D`

#### 2.2 Cài Eigen 3
```bash
# Dùng vcpkg (khuyên dùng)
git clone https://github.com/microsoft/vcpkg
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install eigen3
.\vcpkg install assimp
```

-> -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

#### 2.3 Tạo Project Qt trong Qt Creator
- `File` → `New Project` → `Qt Widgets Application`
- Đặt tên: `RVM1_Interface`
- Chọn Kit: MinGW hoặc MSVC

---

### BƯỚC 3: Cấu Trúc Project

```
RVM1_Interface/
├── CMakeLists.txt
├── main.cpp
├── core/
│   ├── Kinematics.h/.cpp       ← Động học thuận/ngược
│   ├── RobotModel.h/.cpp       ← Mô hình robot (DH params)
│   └── SerialManager.h/.cpp    ← Quản lý COM port
├── ui/
│   ├── MainWindow.h/.cpp       ← Cửa sổ chính
│   ├── TeachBox.h/.cpp         ← Virtual Teachbox
│   └── RobotRenderer.h/.cpp    ← Render 3D robot
└── assets/
    ├── models/                 ← Các file OBJ của từng khâu
    └── icons/
```

---

### BƯỚC 4: Lập Trình Động Học (Kinematics.h)

Dựa theo tài liệu `InterfaceRobot.pdf`, DH parameters của RV-M1:

```cpp
// Kinematics.h
#pragma once
#include <Eigen/Dense>
#include <array>

struct DHParams {
    double a;      // mm
    double alpha;  // rad
    double d;      // mm
    double theta;  // rad (biến)
};

class Kinematics {
public:
    // DH params cố định cho RV-M1 (từ Table 1 trong PDF)
    static constexpr std::array<DHParams, 5> DH_RVM1 = {{
        {0,   M_PI/2,  152, 0},  // Khớp 1
        {250, 0,       0,   0},  // Khớp 2
        {160, 0,       0,   0},  // Khớp 3
        {72,  M_PI/2,  0,   0},  // Khớp 4
        {107, 0,       0,   0}   // Khớp 5
    }};

    // Ma trận biến đổi DH cho 1 khâu
    static Eigen::Matrix4d dhMatrix(double a, double alpha,
                                     double d, double theta);

    // Động học thuận: angles[] -> pose (x,y,z,pitch,roll)
    static Eigen::Matrix4d forwardKinematics(const double angles[5]);

    // Động học ngược: (x,y,z,pitch,roll) -> angles[]
    static bool inverseKinematics(double x, double y, double z,
                                   double pitch, double roll,
                                   double angles[5]);
};
```

```cpp
// Kinematics.cpp - Công thức từ PDF (Section 2)
Eigen::Matrix4d Kinematics::dhMatrix(double a, double alpha,
                                       double d, double theta) {
    Eigen::Matrix4d T;
    double ct = cos(theta), st = sin(theta);
    double ca = cos(alpha), sa = sin(alpha);
    T << ct,  -st*ca,  st*sa,  a*ct,
         st,   ct*ca, -ct*sa,  a*st,
         0,    sa,     ca,     d,
         0,    0,      0,      1;
    return T;
}

Eigen::Matrix4d Kinematics::forwardKinematics(const double angles[5]) {
    Eigen::Matrix4d T = Eigen::Matrix4d::Identity();
    for (int i = 0; i < 5; i++) {
        auto& p = DH_RVM1[i];
        T = T * dhMatrix(p.a, p.alpha, p.d, angles[i] + p.theta);
    }
    return T;
}

bool Kinematics::inverseKinematics(double x, double y, double z,
                                    double pitch, double roll,
                                    double angles[5]) {
    // Khớp 1: theta1 = atan2(y, x)
    angles[0] = atan2(y, x);

    double L2 = 250, L3 = 160, L4 = 179;
    double xr = sqrt(x*x + y*y);
    double x3 = xr - L4 * cos(pitch);
    double z3 = z  - L4 * sin(pitch);

    // Khớp 3: cos(theta3) = (x3^2 + z3^2 - L2^2 - L3^2) / (2*L2*L3)
    double c3 = (x3*x3 + z3*z3 - L2*L2 - L3*L3) / (2*L2*L3);
    if (fabs(c3) > 1.0) return false; // Ngoài tầm với
    angles[2] = atan2(sqrt(1 - c3*c3), c3); // Lấy nghiệm dương

    // Khớp 2: theta2 = beta - alpha
    double alpha = atan2(L3*sin(angles[2]), L3*cos(angles[2]) + L2);
    double beta  = atan2(z3, x3);
    angles[1] = beta - alpha;

    // Khớp 4: theta4 = pitch - (theta2 + theta3)
    angles[3] = pitch - (angles[1] + angles[2]);

    // Khớp 5: roll
    angles[4] = roll;

    return true;
}
```

---

### BƯỚC 5: Lập Trình COM Port (SerialManager.h)

```cpp
// SerialManager.h
#pragma once
#include <QObject>
#include <QSerialPort>
#include <QString>

// Định nghĩa giao thức giao tiếp qua COM
// Ví dụ: "JOINT:0.5,1.2,-0.8,0.3,0.0,0.0\n"  <- điều khiển góc khớp
//        "POSE:100,200,50,-1.57,0.0\n"          <- điều khiển vị trí

class SerialManager : public QObject {
    Q_OBJECT
public:
    explicit SerialManager(QObject* parent = nullptr);

    bool openPort(const QString& portName, int baudRate = 115200);
    void closePort();
    QStringList availablePorts();
    void sendStatus(const double angles[5]); // Gửi trạng thái về

signals:
    void jointAnglesReceived(double q1, double q2, double q3,
                              double q4, double q5, double q6);
    void poseReceived(double x, double y, double z,
                      double pitch, double roll);
    void emergencyStop();

private slots:
    void onDataReceived();

private:
    QSerialPort* m_serial;
    QByteArray   m_buffer;
    void parseCommand(const QString& cmd);
};
```

```cpp
// SerialManager.cpp
void SerialManager::parseCommand(const QString& cmd) {
    // Giao thức nhận: "JOINT:q1,q2,q3,q4,q5,q6\n"
    if (cmd.startsWith("JOINT:")) {
        QStringList vals = cmd.mid(6).split(',');
        if (vals.size() >= 5) {
            emit jointAnglesReceived(vals[0].toDouble(), vals[1].toDouble(),
                                     vals[2].toDouble(), vals[3].toDouble(),
                                     vals[4].toDouble(), 0.0);
        }
    }
    // Giao thức nhận: "POSE:x,y,z,pitch,roll\n"
    else if (cmd.startsWith("POSE:")) {
        QStringList vals = cmd.mid(5).split(',');
        if (vals.size() >= 5) {
            emit poseReceived(vals[0].toDouble(), vals[1].toDouble(),
                              vals[2].toDouble(), vals[3].toDouble(),
                              vals[4].toDouble());
        }
    }
    else if (cmd.startsWith("ESTOP")) {
        emit emergencyStop();
    }
}
```

---

### BƯỚC 6: Render 3D Robot

#### Cách A — Dùng Qt3D (khuyên dùng cho người mới)

```cpp
// RobotRenderer.cpp
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QMesh>
#include <Qt3DCore/QTransform>

// Tạo entity cho mỗi khâu
Qt3DCore::QEntity* createLink(Qt3DCore::QEntity* root,
                               const QString& objFile) {
    auto* entity    = new Qt3DCore::QEntity(root);
    auto* mesh      = new Qt3DRender::QMesh();
    auto* transform = new Qt3DCore::QTransform();

    mesh->setSource(QUrl::fromLocalFile(objFile));
    entity->addComponent(mesh);
    entity->addComponent(transform);
    return entity;
}

// Cập nhật góc khớp khi nhận lệnh
void RobotRenderer::updateJointAngles(const double angles[5]) {
    // Xoay từng khâu theo trục tương ứng
    m_transforms[0]->setRotationY(qRadiansToDegrees(angles[0])); // Waist: Z
    m_transforms[1]->setRotationX(qRadiansToDegrees(angles[1])); // Shoulder: X
    // ... tương tự cho các khớp còn lại
}
```

#### Cách B — Dùng OpenGL thuần (nâng cao hơn)
- Tham khảo thêm nếu cần hiệu năng cao hoặc hiệu ứng đẹp hơn.

---

### BƯỚC 7: Giao Diện Virtual Teachbox (tham khảo Fig.7-10 trong PDF)

Tạo panel bên trái giống trong PDF, gồm các nút:

```cpp
// TeachBox.h - các nút theo đúng layout trong PDF
// Hàng 1: ON/OFF | EMERGENCY
// Hàng 2: INC | DEC | X+/B+ | X-/B-
// Hàng 3: PS  | PC  | Y+/S+ | Y-/S-
// Hàng 4: NST | ORG | Z/E+/4| Z/E-/9
// Hàng 5: TRN | WRT | P+/3  | P-/8
// Hàng 6: MOVE| STEP| R+/2  | R-/7
// Hàng 7: PTP | XYZ | OPT/1 | OPT-/6
// Hàng 8: ENT | TOOL| <O>   | >C<
// Hàng 9: GET | GRASP | PLACE | RELEASE
```

---

### BƯỚC 8: Tích Hợp và Luồng Dữ Liệu

```
[COM Port Input]
     ↓ SerialManager::parseCommand()
     ↓
[Kinematics::inverseKinematics() nếu nhận POSE]
     ↓
[angles[5]]
     ↓
[RobotRenderer::updateJointAngles()]  →  [Màn hình 3D cập nhật]
     ↓
[SerialManager::sendStatus()]         →  [Gửi phản hồi ra COM]
```

---

### BƯỚC 9: CMakeLists.txt Mẫu

```cmake
cmake_minimum_required(VERSION 3.16)
project(RVM1_Interface)

set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort 3DCore 3DRender 3DExtras)
find_package(Eigen3 REQUIRED)

qt_add_executable(RVM1_Interface
    main.cpp
    core/Kinematics.cpp
    core/SerialManager.cpp
    ui/MainWindow.cpp
    ui/TeachBox.cpp
    ui/RobotRenderer.cpp
)

target_link_libraries(RVM1_Interface
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::3DCore
    Qt6::3DRender
    Qt6::3DExtras
    Eigen3::Eigen
)
```

---

## Thứ Tự Thực Hiện (Khuyến Nghị)

```
[Tuần 1] Bước 1+2: Chuẩn bị file 3D + cài môi trường
[Tuần 2] Bước 3+4: Cấu trúc project + lập trình kinematics
[Tuần 3] Bước 5:   COM port communication + test với Python/Serial monitor
[Tuần 4] Bước 6:   Render 3D robot hiển thị đúng
[Tuần 5] Bước 7+8: Teachbox UI + tích hợp toàn bộ
[Tuần 6] Test + sửa lỗi + viết báo cáo
```

---

## Test Nhanh COM Port (Python)

Dùng đoạn Python này để test giao tiếp với phần mềm C++ trước khi có STM32:

```python
import serial, time

ser = serial.Serial('COM3', 115200, timeout=1)
time.sleep(2)

# Test điều khiển góc khớp
ser.write(b'JOINT:0.5,0.8,-0.8,0.3,0.0,0.0\n')
time.sleep(0.5)

# Test điều khiển vị trí
ser.write(b'POSE:0,289,36.6,-1.57,0.0\n')

response = ser.readline()
print("Phan hoi:", response.decode())
ser.close()
```

---

## Tài Liệu Tham Khảo

- `Document/InterfaceRobot.pdf` — Bài báo gốc về Virtual RV-M1 (MATLAB)
- Qt SerialPort docs: https://doc.qt.io/qt-6/qtserialport-index.html
- Qt 3D docs: https://doc.qt.io/qt-6/qt3d-index.html
- Eigen docs: https://eigen.tuxfamily.org/dox/
- DH Parameters RV-M1: Table 1 trong PDF (a, alpha, d, theta)
