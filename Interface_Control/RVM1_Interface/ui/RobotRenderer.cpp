#include "RobotRenderer.h"

#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DExtras/QConeMesh>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DRender/QMesh>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QPointLight>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>

#include <QUrl>
#include <QFile>
#include <QCoreApplication>
#include <QtMath>

// ═══════════════════════════════════════════════════════════════════════════
//  BANG DIEU CHINH VI TRI TUNG PART (don vi mm va do)
//
//  tx, ty, tz : dich chuyen (mm) trong frame cua khop tuong ung
//  rx, ry, rz : xoay Euler XYZ (do)
//
//  Tat ca part ban dau dung rx=-90 de chuyen Z-up CATIA sang Y-up Qt3D.
//  Cac gia tri tx/tz duoc dat de dua CENTER cua moi part ve goc toa do (0,0,0)
//  cua frame khop. Chinh lai neu can sau khi chay va xem ket qua.
//
//  Bounding box tham khao (STL goc, don vi mm):
//    BASE       : X[0..250]  Y[0..190]  Z[0..148]   center(125, 95, 74)
//    Part1      : X[0..250]  Y[0..190]  Z[0.. 15]   center(125, 95,  8)
//    BASE2      : X[0..260]  Y[0..160]  Z[0..160]   center(130, 80, 80)
//    BASE2A     : X[0..230]  Y[0..295]  Z[0.. 67]   center(115,148, 34)
//    BASE2B     : X[0..230]  Y[0..295]  Z[0.. 67]   center(115,148, 34)
//    ARM1       : X[0..360]  Y[0..130]  Z[0..106]   center(180, 65, 53)
//    ARM1A      : X[0.. 82]  Y[0.. 12]  Z[0.. 82]   center( 41,  6, 41)
//    ARM2       : X[0..241]  Y[0.. 88]  Z[0.. 90]   center(121, 44, 45)
//    ARM2A      : X[0.. 23]  Y[0..135]  Z[0.. 80]   center( 12, 68, 40)
//    GRIPPER1   : X[0.. 77]  Y[0.. 70]  Z[0.. 32]   center( 39, 35, 16)
//    GRIPPER2   : X[0.. 80]  Y[0.. 80]  Z[0.. 30]   center( 40, 40, 15)
// ═══════════════════════════════════════════════════════════════════════════
struct PartAdjust {
    const char* file;
    float tx, ty, tz;    // mm
    float rx, ry, rz;    // degrees
    QColor color;
};

// --- Link 0: De robot (khop co dinh) ---
static const PartAdjust LINK0[] = {
  { "BASE.STL",   -168.0f, 15.0f, 95.0f,  -90.0f, 0.0f, 0.0f,  QColor(160,160,165) },
};

// --- Link 1: Eo (Waist) - gan vao joint1 ---
static const PartAdjust LINK1[] = {
  { "BASE2.STL",  -200.0f, -15.0f, 80.0f,  -90.0f, 0.0f, 0.0f,  QColor(220,  90,  40) },
  { "BASE2A.STL", -227.5f, 85.0f, -115.0f,  -90.0f, -90.0f, 0.0f,  QColor(200,  80,  30) },
  { "BASE2B.STL", -227.5f, 152.0f, -115.0f,  -90.0f, -90.0f, 0.0f,  QColor(200,  80,  30) },
};

// --- Link 2: Vai (Shoulder) - gan vao joint2 ---
static const PartAdjust LINK2[] = {
  { "ARM1.STL",   -65.0f, -65.0f, -53.0f,   0.0f,  0.0f,   0.0f,  QColor( 50, 140, 220) },
  { "ARM1A.STL",  209.0f, -41.0f, 55.0f,  -90.0f, 0.0f,   0.0f,  QColor( 40, 120, 200) },  // ben 1
  { "ARM1A.STL",  291.0f, -41.0f, -55.0f,  -90.0f, 0.0f, 180.0f,  QColor( 40, 120, 200) },  // ben 2 (doi xung)
};

// --- Link 3: Khuyu (Elbow) - gan vao joint3 ---
static const PartAdjust LINK3[] = {
  { "ARM2.STL",   -44.0f, -44.0f, -45.0f,  -0.0f, 0.0f, 0.0f,  QColor( 50, 200, 100) },
  { "ARM2A.STL",   194.74f, -40.0f, -35.0f,  -90.0f, 90.0f, 0.0f,  QColor( 40, 180,  90) },
  { "ARM2A.STL",   59.74f, -40.0f, 35.0f,  -90.0f, 90.0f, 180.0f,  QColor( 40, 180,  90) },
};

// --- Link 4: Co tay (Wrist) - gan vao joint4 ---
static const PartAdjust LINK4[] = {
  { "GRIPPER1.STL", -35.0f, -35.0f, -16.0f,  0.0f, 0.0f, 0.0f,  QColor(220, 195,  40) },
  { "GRIPPER2.STL", 42.0f, -40.0f, 40.0f,  0.0f, 90.0f, 0.0f,  QColor(200, 175,  30) },
};

// -----------------------------------------------------------------------
static void addPart(Qt3DCore::QEntity*  parent,
                    const QString&      partsDir,
                    const PartAdjust&   p)
{
    QString path = partsDir + p.file;
    if (!QFile::exists(path)) {
        // Thu chu thuong neu khong tim thay
        path = partsDir + QString(p.file).toLower();
        if (!QFile::exists(path)) return;
    }

    auto* entity = new Qt3DCore::QEntity(parent);

    auto* t = new Qt3DCore::QTransform(entity);
    t->setScale(0.001f);
    t->setRotation(QQuaternion::fromEulerAngles(p.rx, p.ry, p.rz));
    t->setTranslation(QVector3D(p.tx * 0.001f, p.ty * 0.001f, p.tz * 0.001f));
    entity->addComponent(t);

    auto* mesh = new Qt3DRender::QMesh(entity);
    mesh->setSource(QUrl::fromLocalFile(path));
    entity->addComponent(mesh);

    auto* mat = new Qt3DExtras::QPhongMaterial(entity);
    mat->setDiffuse(p.color);
    mat->setAmbient(p.color.darker(250));
    mat->setSpecular(QColor(220, 220, 220));
    mat->setShininess(60.0f);
    entity->addComponent(mat);
}

template<size_t N>
static void addLink(Qt3DCore::QEntity* parent,
                    const QString&     partsDir,
                    const PartAdjust (&parts)[N])
{
    for (size_t i = 0; i < N; ++i)
        addPart(parent, partsDir, parts[i]);
}

// -----------------------------------------------------------------------
// Ve mot truc toa do: hinh tru lam than + hinh non lam mui ten
// axis : 0=X (do), 1=Y (xanh la), 2=Z (xanh duong)
// length: chieu dai truc (met)
// -----------------------------------------------------------------------
static void addAxisArrow(Qt3DCore::QEntity* parent, int axis, float length)
{
    const QColor colors[3] = { QColor(220,50,50), QColor(50,200,50), QColor(50,100,220) };
    QColor col = colors[axis];

    const float shaftR  = length * 0.018f;
    const float headH   = length * 0.15f;
    const float shaftH  = length - headH;

    // -- Phan than (cylinder) --
    auto* shaftEnt = new Qt3DCore::QEntity(parent);
    auto* shaft    = new Qt3DExtras::QCylinderMesh(shaftEnt);
    shaft->setRadius(shaftR);
    shaft->setLength(shaftH);
    shaft->setRings(4);
    shaft->setSlices(12);
    shaftEnt->addComponent(shaft);

    auto* shaftMat = new Qt3DExtras::QPhongMaterial(shaftEnt);
    shaftMat->setDiffuse(col);
    shaftMat->setAmbient(col);
    shaftEnt->addComponent(shaftMat);

    auto* shaftT = new Qt3DCore::QTransform(shaftEnt);
    // Qt3D: cylinder chieu doc theo Y, dich len half-length de go tai goc
    QVector3D trans;
    QQuaternion rot;
    if (axis == 0) { // X: xoay -90 quanh Z
        rot   = QQuaternion::fromAxisAndAngle(0,0,1,-90.0f);
        trans = QVector3D(shaftH * 0.5f, 0, 0);
    } else if (axis == 1) { // Y: giu nguyen
        rot   = QQuaternion();
        trans = QVector3D(0, shaftH * 0.5f, 0);
    } else { // Z: xoay 90 quanh X
        rot   = QQuaternion::fromAxisAndAngle(1,0,0, 90.0f);
        trans = QVector3D(0, 0, shaftH * 0.5f);
    }
    shaftT->setRotation(rot);
    shaftT->setTranslation(trans);
    shaftEnt->addComponent(shaftT);

    // -- Phan mui ten (cone) --
    auto* coneEnt = new Qt3DCore::QEntity(parent);
    auto* cone    = new Qt3DExtras::QConeMesh(coneEnt);
    cone->setBottomRadius(shaftR * 2.5f);
    cone->setLength(headH);
    cone->setRings(4);
    cone->setSlices(12);
    coneEnt->addComponent(cone);

    auto* coneMat = new Qt3DExtras::QPhongMaterial(coneEnt);
    coneMat->setDiffuse(col);
    coneMat->setAmbient(col);
    coneEnt->addComponent(coneMat);

    auto* coneT = new Qt3DCore::QTransform(coneEnt);
    QVector3D coneTrans;
    if (axis == 0) {
        coneT->setRotation(QQuaternion::fromAxisAndAngle(0,0,1,-90.0f));
        coneTrans = QVector3D(shaftH + headH * 0.5f, 0, 0);
    } else if (axis == 1) {
        coneT->setRotation(QQuaternion());
        coneTrans = QVector3D(0, shaftH + headH * 0.5f, 0);
    } else {
        coneT->setRotation(QQuaternion::fromAxisAndAngle(1,0,0, 90.0f));
        coneTrans = QVector3D(0, 0, shaftH + headH * 0.5f);
    }
    coneT->setTranslation(coneTrans);
    coneEnt->addComponent(coneT);
}

// Ve 3 truc XYZ tai mot vi tri bat ky
static void addAxes(Qt3DCore::QEntity* parent, float length = 0.20f)
{
    addAxisArrow(parent, 0, length); // X - do
    addAxisArrow(parent, 1, length); // Y - xanh la
    addAxisArrow(parent, 2, length); // Z - xanh duong
}

// -----------------------------------------------------------------------
RobotRenderer::RobotRenderer(QObject* parent)
    : QObject(parent)
{
    m_view = new Qt3DExtras::Qt3DWindow();
    m_view->defaultFrameGraph()->setClearColor(QColor(45, 45, 55));

    QString partsDir = QCoreApplication::applicationDirPath() + "/assets/parts/";
    m_root = buildScene(partsDir);
    m_view->setRootEntity(m_root);
}

Qt3DExtras::Qt3DWindow* RobotRenderer::view() const { return m_view; }

// -----------------------------------------------------------------------
Qt3DCore::QEntity* RobotRenderer::buildScene(const QString& partsDir)
{
    auto* root = new Qt3DCore::QEntity();

    // Camera
    auto* cam = m_view->camera();
    cam->lens()->setPerspectiveProjection(45.0f, 16.f/9.f, 0.001f, 20.0f);
    cam->setPosition(QVector3D(1.6f, 1.1f, 1.6f));
    cam->setViewCenter(QVector3D(0.0f, 0.25f, 0.0f));
    cam->setUpVector(QVector3D(0.0f, 1.0f, 0.0f));

    auto* camCtrl = new Qt3DExtras::QOrbitCameraController(root);
    camCtrl->setCamera(cam);
    camCtrl->setLinearSpeed(0.5f);
    camCtrl->setLookSpeed(120.0f);

    // Anh sang
    auto makeLight = [&](QVector3D pos, QColor col, float intensity) {
        auto* le = new Qt3DCore::QEntity(root);
        auto* lt = new Qt3DCore::QTransform(le);
        lt->setTranslation(pos);
        le->addComponent(lt);
        auto* ll = new Qt3DRender::QPointLight(le);
        ll->setColor(col);
        ll->setIntensity(intensity);
        le->addComponent(ll);
    };
    makeLight({ 1.5f,  2.0f,  1.5f}, Qt::white,           1.3f);
    makeLight({-1.0f,  1.5f, -1.0f}, QColor(180,200,230), 0.5f);

    // Tao joint node phan cap:
    //   parent -> offsetEnt (tinh tien DH co dinh) -> rotEnt (rotation dong)
    auto makeJoint = [&](Qt3DCore::QEntity* par,
                         QVector3D          dhOffset,
                         int                idx) -> Qt3DCore::QEntity*
    {
        auto* offsetEnt = new Qt3DCore::QEntity(par);
        auto* offsetT   = new Qt3DCore::QTransform(offsetEnt);
        offsetT->setTranslation(dhOffset);
        offsetEnt->addComponent(offsetT);

        auto* rotEnt = new Qt3DCore::QEntity(offsetEnt);
        auto* rotT   = new Qt3DCore::QTransform(rotEnt);
        rotEnt->addComponent(rotT);
        m_pivotTransforms[idx] = rotT;
        return rotEnt;
    };

    // Link 0 - de co dinh tai goc toa do
    auto* baseEnt = new Qt3DCore::QEntity(root);
    baseEnt->addComponent(new Qt3DCore::QTransform(baseEnt));
    addLink(baseEnt, partsDir, LINK0);

    // DH offsets (met):
    //   J1 d1=152mm len Y (truc dung)
    //   J2 khong dich (khop vai ngay tai vi tri J1)
    //   J3 a2=250mm theo X
    //   J4 a3=160mm theo X
    //   J5 a4= 72mm theo X
    auto* rot1 = makeJoint(root,   {0.0f,   0.148f, 0.0f}, 0);  // J1 Eo: len Y d1=148mm
    addLink(rot1, partsDir, LINK1);

    // J2 Vai: offset (0,0,0) so voi J1 — khop vai nam ngay tai J1
    // J3 Khuyu: dich a2=250mm doc canh tay (truc X local cua J2)
    // J4 Co tay: dich a3=160mm theo X
    // J5 Roll: dich a4=72mm theo X
    // Chi chinh tx/ty/tz trong bang LINK de can mesh, KHONG chinh DH offset nay
    auto* rot2 = makeJoint(rot1,   {0.0f,   0.148f,   0.0f}, 1);  // J2 Vai: world Y=0.148m
    addLink(rot2, partsDir, LINK2);

    auto* rot3 = makeJoint(rot2,   {0.250f, 0.0f,   0.0f}, 2);  // J3 Khuyu a2=250mm
    addLink(rot3, partsDir, LINK3);

    auto* rot4 = makeJoint(rot3,   {0.160f, 0.0f,   0.0f}, 3);  // J4 Co tay a3=160mm
    addLink(rot4, partsDir, LINK4);

    // Joint 5 (co tay roll) - hien chua co part rieng
    auto* rot5 = makeJoint(rot4,   {0.072f, 0.0f,   0.0f}, 4);

    // -------------------------------------------------------
    // TCP Marker — diem cuoi cua robot (Tool Center Point)
    //
    // Theo thong so DH: a5 = 107 mm, tuc la TCP cach khop 5
    // them 107 mm doc theo truc X cuc bo cua khop 5.
    // Khi cac khop quay, marker nay tu dong di chuyen theo
    // vi no la con cua rot5 trong cay phan cap scene.
    //
    // Bieu dien bang hinh cau do nho (ban kinh 12 mm).
    // -------------------------------------------------------
    auto* tcpEnt = new Qt3DCore::QEntity(rot5);

    // Hinh cau
    auto* tcpSphere = new Qt3DExtras::QSphereMesh(tcpEnt);
    tcpSphere->setRadius(0.003f);   // ban kinh 3 mm
    tcpSphere->setRings(20);
    tcpSphere->setSlices(20);
    tcpEnt->addComponent(tcpSphere);

    // Mau do tuoi, phat sang de de nhan ra
    auto* tcpMat = new Qt3DExtras::QPhongMaterial(tcpEnt);
    tcpMat->setDiffuse(QColor(255, 50,  50));
    tcpMat->setAmbient(QColor(200, 10,  10));
    tcpMat->setSpecular(QColor(255, 255, 255));
    tcpMat->setShininess(120.0f);
    tcpEnt->addComponent(tcpMat);

    // Dich chuyen 107 mm doc theo truc X (a5 = 107 mm)
    auto* tcpT = new Qt3DCore::QTransform(tcpEnt);
    tcpT->setTranslation(QVector3D(0.107f, 0.0f, 0.0f));
    tcpEnt->addComponent(tcpT);

    return root;
}

// -----------------------------------------------------------------------
void RobotRenderer::updateJointAngles(double q1, double q2, double q3,
                                       double q4, double q5)
{
    m_pivotTransforms[0]->setRotation(
        QQuaternion::fromAxisAndAngle(0,1,0, float(qRadiansToDegrees(q1))));
    m_pivotTransforms[1]->setRotation(
        QQuaternion::fromAxisAndAngle(0,0,1, float(qRadiansToDegrees(q2))));
    m_pivotTransforms[2]->setRotation(
        QQuaternion::fromAxisAndAngle(0,0,1, float(qRadiansToDegrees(q3))));
    m_pivotTransforms[3]->setRotation(
        QQuaternion::fromAxisAndAngle(0,0,1, float(qRadiansToDegrees(q4))));
    m_pivotTransforms[4]->setRotation(
        QQuaternion::fromAxisAndAngle(1,0,0, float(qRadiansToDegrees(q5))));
}
