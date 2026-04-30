#pragma once
#include <QObject>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/Qt3DWindow>
#include <array>

// Hiển thị robot RV-M1 bằng Qt3D
//
// Kiến trúc mỗi joint:
//   offsetEntity  (transform cố định: tịnh tiến DH + chỉnh gốc mesh)
//     └─ rotEntity  (transform động: chỉ chứa rotation)
//          └─ meshEntity (mesh + material)
//
// m_pivotTransforms[i] = transform của rotEntity[i], chỉ set rotation.

class RobotRenderer : public QObject {
    Q_OBJECT
public:
    explicit RobotRenderer(QObject* parent = nullptr);

    Qt3DExtras::Qt3DWindow* view() const;

public slots:
    void updateJointAngles(double q1, double q2, double q3,
                           double q4, double q5);

private:
    Qt3DCore::QEntity* buildScene(const QString& modelsPath);

    Qt3DExtras::Qt3DWindow*              m_view = nullptr;
    Qt3DCore::QEntity*                   m_root = nullptr;
    // rotEntity transform cho mỗi khớp — chỉ set rotation, không động đến translation
    std::array<Qt3DCore::QTransform*, 5> m_pivotTransforms{};
};
