#include "Kinematics.h"
#include <cmath>

Eigen::Matrix4d Kinematics::dhMatrix(double a, double alpha,
                                     double d, double theta)
{
    Eigen::Matrix4d T;
    double ct = std::cos(theta), st = std::sin(theta);
    double ca = std::cos(alpha), sa = std::sin(alpha);
    T << ct, -st*ca,  st*sa, a*ct,
         st,  ct*ca, -ct*sa, a*st,
          0,     sa,     ca,    d,
          0,      0,      0,    1;
    return T;
}

Eigen::Matrix4d Kinematics::forwardKinematics(const double angles[5])
{
    Eigen::Matrix4d T = Eigen::Matrix4d::Identity();
    for (int i = 0; i < 5; i++) {
        const auto& p = DH[i];
        T = T * dhMatrix(p.a, p.alpha, p.d, angles[i] + p.theta0);
    }
    return T;
}

bool Kinematics::inverseKinematics(double x, double y, double z,
                                   double pitch, double roll,
                                   double angles[5])
{
    const double L2 = 250.0, L3 = 160.0, L4 = 179.0;

    // Khớp 1
    angles[0] = std::atan2(y, x);

    // Vị trí cổ tay (bỏ phần L4)
    double xr = std::sqrt(x*x + y*y) - L4 * std::cos(pitch);
    double zr = z - L4 * std::sin(pitch);

    // Khớp 3 theo law of cosines
    double c3 = (xr*xr + zr*zr - L2*L2 - L3*L3) / (2.0*L2*L3);
    if (std::fabs(c3) > 1.0) return false;
    angles[2] = std::atan2(std::sqrt(1.0 - c3*c3), c3);

    // Khớp 2
    double alpha_ang = std::atan2(L3*std::sin(angles[2]),
                                  L2 + L3*std::cos(angles[2]));
    double beta      = std::atan2(zr, xr);
    angles[1] = beta - alpha_ang;

    // Khớp 4 (wrist pitch)
    angles[3] = pitch - (angles[1] + angles[2]);

    // Khớp 5 (wrist roll)
    angles[4] = roll;

    return true;
}
