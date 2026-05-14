#include "Kinematics.h"
#include <cmath>

// ============================================================
// Kinematics.cpp
// Triển khai các phương thức động học cho robot RV-M1 (5 DOF).
// Xem Kinematics.h để biết mô tả chi tiết về từng hàm và bảng DH.
// ============================================================


// ------------------------------------------------------------
// dhMatrix() — Xây dựng ma trận biến đổi thuần nhất DH 4×4
//
// Công thức ma trận DH chuẩn:
//
//   T_i = Rot_z(θ) · Trans_z(d) · Trans_x(a) · Rot_x(α)
//
// Sau khi khai triển cho ta dạng ma trận:
//
//   [ cosθ  -sinθ·cosα   sinθ·sinα   a·cosθ ]
//   [ sinθ   cosθ·cosα  -cosθ·sinα   a·sinθ ]
//   [   0      sinα         cosα        d   ]
//   [   0        0            0         1   ]
//
// ct = cos(θ), st = sin(θ), ca = cos(α), sa = sin(α)
// ------------------------------------------------------------
Eigen::Matrix4d Kinematics::dhMatrix(double a, double alpha,
                                     double d, double theta)
{
    Eigen::Matrix4d T;

    // Tính trước sin/cos để tránh gọi lại nhiều lần
    double ct = std::cos(theta), st = std::sin(theta); // sin/cos của góc khớp θ
    double ca = std::cos(alpha), sa = std::sin(alpha); // sin/cos của góc xoắn α

    // Điền ma trận theo công thức DH chuẩn (hàng theo hàng)
    T << ct,  -st*ca,   st*sa,  a*ct,   // Hàng 1: thành phần X
         st,   ct*ca,  -ct*sa,  a*st,   // Hàng 2: thành phần Y
          0,      sa,      ca,     d,   // Hàng 3: thành phần Z
          0,       0,       0,     1;   // Hàng 4: hàng thuần nhất (homogeneous)

    return T;
}


// ------------------------------------------------------------
// forwardKinematics() — Động học THUẬN
//
// Nhân chuỗi 5 ma trận DH để thu được ma trận biến đổi tổng:
//   T_0_5 = T_01(θ1) × T_12(θ2) × T_23(θ3) × T_34(θ4) × T_45(θ5)
//
// Vòng lặp duyệt lần lượt qua 5 khớp, mỗi lần lấy tham số DH
// của khớp đó và nhân tích lũy vào ma trận T.
//
// Lưu ý: góc khớp thực tế = angles[i] + DH[i].theta0
//        (theta0 là offset hiệu chỉnh vị trí zero, hiện tại đều = 0)
// ------------------------------------------------------------
Eigen::Matrix4d Kinematics::forwardKinematics(const double angles[5])
{
    // Bắt đầu từ ma trận đơn vị (hệ tọa độ gốc tại đế robot)
    Eigen::Matrix4d T = Eigen::Matrix4d::Identity();

    for (int i = 0; i < 5; i++) {
        const auto& p = DH[i]; // Lấy tham số DH của khớp thứ i

        // Nhân tích lũy: T = T × T_i
        // Góc khớp = góc biến thiên + offset cố định (theta0)
        T = T * dhMatrix(p.a, p.alpha, p.d, angles[i] + p.theta0);
    }

    // Sau vòng lặp, T là ma trận biến đổi từ hệ gốc đến TCP:
    //   T[0..2][3] = {x, y, z} — tọa độ vị trí của đầu công cụ [mm]
    //   T[0..2][0..2]           — ma trận quay 3×3 mô tả hướng công cụ
    return T;
}


// ------------------------------------------------------------
// inverseKinematics() — Động học NGƯỢC (phương pháp hình học)
//
// Bài toán: cho trước vị trí (x, y, z) và hướng (pitch, roll)
// của TCP, tính ngược ra 5 góc khớp cần thiết.
//
// Chiến lược tách bài toán:
//   1. Từ (x, y, z) và pitch → tính vị trí "tâm cổ tay" (wrist center)
//      bằng cách lùi lại L4 dọc theo hướng pitch.
//   2. Từ tâm cổ tay → giải hình học phẳng cho khớp 2 và 3
//      (bài toán tam giác 2 thanh L2–L3).
//   3. Khớp 1 từ hình chiếu trên mặt phẳng nằm ngang.
//   4. Khớp 4 bù lại góc để đạt đúng pitch mong muốn.
//   5. Khớp 5 = roll trực tiếp.
//
// Chiều dài các đoạn tay (mm):
//   L2 = 250  (cánh tay trên: shoulder → elbow)
//   L3 = 160  (cánh tay dưới: elbow → wrist)
//   L4 = 179  (cổ tay + đầu công cụ: wrist → TCP)
//             = a_4 + a_5 = 72 + 107 = 179 mm
// ------------------------------------------------------------
bool Kinematics::inverseKinematics(double x, double y, double z,
                                   double pitch, double roll,
                                   double angles[5])
{
    // Chiều dài các đoạn tay [mm]
    const double L2 = 250.0; // Cánh tay trên (shoulder → elbow)
    const double L3 = 160.0; // Cánh tay dưới (elbow → wrist center)
    const double L4 = 179.0; // Cổ tay + TCP   (wrist center → TCP) = 72 + 107
    const double D1 = 152.0; // Chiều cao khớp 1 so với đế (d1 trong bảng DH)
                              // Phải trừ đi để chuyển z từ hệ đế sang hệ khớp 2

    // --------------------------------------------------------
    // Bước 1: Khớp 1 (Waist) — xoay eo quanh trục Z thẳng đứng
    //
    // Nhìn từ trên xuống, TCP nằm tại (x, y) trên mặt phẳng XY.
    // Khớp 1 chỉ cần xoay đúng hướng đến mục tiêu.
    //   θ1 = atan2(y, x)
    // --------------------------------------------------------
    angles[0] = std::atan2(y, x);

    // --------------------------------------------------------
    // Bước 2: Tính tọa độ "tâm cổ tay" (wrist center)
    //
    // TCP nằm cách tâm cổ tay một đoạn L4 theo hướng pitch:
    //   TCP_x = wrist_x + L4 * cos(pitch)   (theo phương ngang)
    //   TCP_z = wrist_z + L4 * sin(pitch)   (theo phương đứng)
    //
    // Đảo ngược để tìm tâm cổ tay:
    //   xr = r_xy - L4 * cos(pitch)    với r_xy = sqrt(x²+y²)
    //   zr = z    - L4 * sin(pitch)
    //
    // xr: khoảng cách ngang từ trục khớp 2 đến tâm cổ tay
    // zr: chiều cao từ mặt đất đến tâm cổ tay
    // --------------------------------------------------------
    double r_xy = std::sqrt(x*x + y*y); // Bán kính ngang của TCP
    double xr   = r_xy - L4 * std::cos(pitch); // Tọa độ ngang của tâm cổ tay
    // Trừ D1 để chuyển z từ hệ tọa độ đế (FK output) sang hệ khớp 2 (shoulder).
    // FK luôn cộng d1=152mm vào z, nên IK phải trừ lại để giải đúng.
    double zr   = (z - D1) - L4 * std::sin(pitch); // Tọa độ đứng tâm cổ tay so với khớp 2

    // --------------------------------------------------------
    // Bước 3: Khớp 3 (Elbow) — dùng định lý cosin
    //
    // Hai thanh L2 và L3 tạo thành tam giác với đường chéo D:
    //   D² = xr² + zr²
    //
    // Theo định lý cosin:
    //   D² = L2² + L3² - 2·L2·L3·cos(π - θ3)
    //      = L2² + L3² + 2·L2·L3·cos(θ3)
    //
    // Suy ra:
    //   cos(θ3) = (D² - L2² - L3²) / (2·L2·L3)
    //           = (xr² + zr² - L2² - L3²) / (2·L2·L3)
    //
    // Nếu |c3| > 1: điểm đích nằm ngoài tầm với → vô nghiệm.
    //
    // sin(θ3) được lấy dương (cấu hình khuỷu tay hướng lên trên - "elbow up")
    // --------------------------------------------------------
    double c3 = (xr*xr + zr*zr - L2*L2 - L3*L3) / (2.0*L2*L3);

    // Kiểm tra điều kiện tầm với: nếu |cos(θ3)| > 1 thì vô nghiệm
    if (std::fabs(c3) > 1.0) return false;

    // Tính θ3: lấy nghiệm dương (khuỷu tay hướng lên)
    angles[2] = std::atan2(std::sqrt(1.0 - c3*c3), c3);

    // --------------------------------------------------------
    // Bước 4: Khớp 2 (Shoulder) — tính góc vai
    //
    // Trong mặt phẳng (xr, zr), cần tính góc θ2 sao cho tổng
    // của hai thanh L2 và L3 chỉ đúng vào tâm cổ tay.
    //
    // Gọi:
    //   beta      = atan2(zr, xr)  — góc từ gốc đến tâm cổ tay
    //   alpha_ang = góc nhỏ giữa L2 và đường thẳng gốc–tâm cổ tay
    //             = atan2(L3·sin(θ3), L2 + L3·cos(θ3))
    //
    // θ2 = beta - alpha_ang
    //    (góc vai = góc hướng chung - góc lệch do khuỷu gây ra)
    // --------------------------------------------------------
    double alpha_ang = std::atan2(L3 * std::sin(angles[2]),
                                  L2 + L3 * std::cos(angles[2])); // Góc lệch khuỷu tay
    double beta      = std::atan2(zr, xr);                        // Góc hướng tới tâm cổ tay
    angles[1] = beta - alpha_ang; // θ2 = góc hướng chung - góc lệch khuỷu

    // --------------------------------------------------------
    // Bước 5: Khớp 4 (Wrist Pitch) — bù góc ngả công cụ
    //
    // Hướng của đầu công cụ (pitch) bằng tổng góc của các khớp
    // trước đó cộng thêm góc khớp 4:
    //   pitch = θ2 + θ3 + θ4
    //
    // Suy ra:
    //   θ4 = pitch - (θ2 + θ3)
    // --------------------------------------------------------
    angles[3] = pitch - (angles[1] + angles[2]);

    // --------------------------------------------------------
    // Bước 6: Khớp 5 (Wrist Roll) — xoay công cụ
    //
    // Khớp 5 xoay độc lập quanh trục dọc của công cụ, không
    // ảnh hưởng đến vị trí TCP. Giá trị góc bằng đúng roll mong muốn.
    // --------------------------------------------------------
    angles[4] = roll;

    return true; // Bài toán có nghiệm hợp lệ
}
