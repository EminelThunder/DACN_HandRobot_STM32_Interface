#pragma once
#include <Eigen/Dense>
#include <array>
#include <cmath>

struct DHParams {
    double a;      // mm
    double alpha;  // rad
    double d;      // mm
    double theta0; // rad (offset cố định)
};

class Kinematics {
public:
    // DH params RV-M1 (5 khớp quay)
    static constexpr std::array<DHParams, 5> DH = {{
        {  0,  M_PI/2, 152, 0},  // Khớp 1 - Waist
        {250,       0,   0, 0},  // Khớp 2 - Shoulder
        {160,       0,   0, 0},  // Khớp 3 - Elbow
        { 72,  M_PI/2,   0, 0},  // Khớp 4 - Wrist pitch
        {107,       0,   0, 0}   // Khớp 5 - Wrist roll
    }};

    // Ma trận DH cho 1 khâu
    static Eigen::Matrix4d dhMatrix(double a, double alpha,
                                    double d, double theta);

    // Động học thuận: angles[5] (rad) -> ma trận T_0_5 (4x4)
    static Eigen::Matrix4d forwardKinematics(const double angles[5]);

    // Động học ngược: (x,y,z mm, pitch rad, roll rad) -> angles[5]
    // Trả về false nếu ngoài tầm với
    static bool inverseKinematics(double x, double y, double z,
                                  double pitch, double roll,
                                  double angles[5]);
};
