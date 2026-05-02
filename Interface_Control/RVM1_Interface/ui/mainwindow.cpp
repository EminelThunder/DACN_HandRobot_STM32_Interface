#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "SerialManager.h"
#include "RobotRenderer.h"
#include "Kinematics.h"

#include <QWidget>
#include <QSerialPort>
#include <Qt3DExtras/Qt3DWindow>
#include <QtMath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_serial(new SerialManager(this))
    , m_renderer(new RobotRenderer(this))
{
    ui->setupUi(this);

    // ── Nhúng Qt3DWindow vào widget viewport3D ──────────────────────────────
    QWidget* container = QWidget::createWindowContainer(m_renderer->view(), this);
    container->setMinimumSize(400, 400);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->viewport3D->layout()->addWidget(container);

    // ── Khởi tạo danh sách port và chọn giá trị mặc định ────────────────────────────────
    onBtnRefreshClicked();
    ui->comboBaud->setCurrentIndex(4);       // 115200
    ui->comboDataBits->setCurrentIndex(3);   // 8
    ui->comboStopBits->setCurrentIndex(0);   // 1
    ui->comboParity->setCurrentIndex(0);     // None
    ui->comboFlow->setCurrentIndex(0);       // None

    // Màu nút mặc định (chưa kết nối = xanh lá)
    ui->btnConnect->setStyleSheet(
        "QPushButton { background-color: #27ae60; color: white; font-weight: bold; border-radius: 4px; }"
        "QPushButton:hover { background-color: #2ecc71; }"
        "QPushButton:pressed { background-color: #1e8449; }");

    // ── Kết nối nút bấm & slider ────────────────────────────────────────────
    connect(ui->btnConnect,  &QPushButton::clicked, this, &MainWindow::onBtnConnectClicked);
    connect(ui->btnRefresh,  &QPushButton::clicked, this, &MainWindow::onBtnRefreshClicked);
    connect(ui->btnHome,     &QPushButton::clicked, this, &MainWindow::onBtnHomeClicked);

    connect(ui->sliderJ1, &QSlider::valueChanged, this, &MainWindow::onSliderJ1Changed);
    connect(ui->sliderJ2, &QSlider::valueChanged, this, &MainWindow::onSliderJ2Changed);
    connect(ui->sliderJ3, &QSlider::valueChanged, this, &MainWindow::onSliderJ3Changed);
    connect(ui->sliderJ4, &QSlider::valueChanged, this, &MainWindow::onSliderJ4Changed);
    connect(ui->sliderJ5, &QSlider::valueChanged, this, &MainWindow::onSliderJ5Changed);

    // ── Kết nối SerialManager signals ───────────────────────────────────────
    connect(m_serial, &SerialManager::connectionChanged,
            this, &MainWindow::onConnectionChanged);

    connect(m_serial, &SerialManager::jointAnglesReceived,
            this, &MainWindow::onJointAnglesReceived);

    connect(m_serial, &SerialManager::poseReceived,
            [this](double x, double y, double z, double pitch, double roll) {
                double angles[5]{};
                if (Kinematics::inverseKinematics(x, y, z, pitch, roll, angles)) {
                    for (int i = 0; i < 5; ++i) m_angles[i] = angles[i];
                    updateSlidersFromAngles();
                    m_renderer->updateJointAngles(angles[0], angles[1],
                                                  angles[2], angles[3], angles[4]);
                    sendCurrentAngles();
                }
            });

    connect(m_serial, &SerialManager::emergencyStop,
            [this]() {
                ui->sliderJ1->setValue(0);
                ui->sliderJ2->setValue(0);
                ui->sliderJ3->setValue(0);
                ui->sliderJ4->setValue(0);
                ui->sliderJ5->setValue(0);
                ui->labelStatus->setText("⚠ EMERGENCY STOP");
                ui->labelStatus->setStyleSheet("color: red; font-weight: bold;");
            });

    // ── Cập nhật 3D ban đầu ──────────────────────────────────────────────────
    // m_angles[1] = qDegreesToRadians(35.0);
    // m_angles[2] = qDegreesToRadians(-65.0);
    // m_renderer->updateJointAngles(m_angles[0], m_angles[1], m_angles[2],
    //                               m_angles[3], m_angles[4]);
    m_renderer->updateJointAngles(0, 0, 0, 0, 0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ────────────────────────────────────────────────────────────────────────────
// Slot: nút Kết nối / Ngắt kết nối
// ────────────────────────────────────────────────────────────────────────────
void MainWindow::onBtnConnectClicked()
{
    if (m_serial->isOpen()) {
        m_serial->closePort();
        ui->btnConnect->setText("Kết nối");
    } else {
        QString port = ui->comboPort->currentText();
        int     baud = ui->comboBaud->currentText().toInt();

        // Data Bits
        const QSerialPort::DataBits dataBitsMap[] = {
            QSerialPort::Data5, QSerialPort::Data6,
            QSerialPort::Data7, QSerialPort::Data8
        };
        auto dataBits = dataBitsMap[qBound(0, ui->comboDataBits->currentIndex(), 3)];

        // Stop Bits
        const QSerialPort::StopBits stopBitsMap[] = {
            QSerialPort::OneStop, QSerialPort::OneAndHalfStop, QSerialPort::TwoStop
        };
        auto stopBits = stopBitsMap[qBound(0, ui->comboStopBits->currentIndex(), 2)];

        // Parity
        const QSerialPort::Parity parityMap[] = {
            QSerialPort::NoParity, QSerialPort::EvenParity,
            QSerialPort::OddParity, QSerialPort::SpaceParity, QSerialPort::MarkParity
        };
        auto parity = parityMap[qBound(0, ui->comboParity->currentIndex(), 4)];

        // Flow Control
        const QSerialPort::FlowControl flowMap[] = {
            QSerialPort::NoFlowControl,
            QSerialPort::HardwareControl,
            QSerialPort::SoftwareControl
        };
        auto flow = flowMap[qBound(0, ui->comboFlow->currentIndex(), 2)];

        if (!m_serial->openPort(port, baud, dataBits, stopBits, parity, flow)) {
            ui->labelStatus->setText("✗ Lỗi mở " + port);
            ui->labelStatus->setStyleSheet("color: red; font-weight: bold;");
        }
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Slot: nút Refresh danh sách port
// ────────────────────────────────────────────────────────────────────────────
void MainWindow::onBtnRefreshClicked()
{
    ui->comboPort->clear();
    for (const QString& p : m_serial->availablePorts())
        ui->comboPort->addItem(p);
}

// ────────────────────────────────────────────────────────────────────────────
// Slot: nút Về vị trí nghỉ → reset tất cả khớp về 0°
// ────────────────────────────────────────────────────────────────────────────
void MainWindow::onBtnHomeClicked()
{
    for (int i = 0; i < 5; ++i) m_angles[i] = 0.0;
    updateSlidersFromAngles();
    m_renderer->updateJointAngles(0, 0, 0, 0, 0);
    sendCurrentAngles();
}

// ────────────────────────────────────────────────────────────────────────────
// Slot: slider thay đổi → cập nhật nhãn + 3D + gửi COM
// ────────────────────────────────────────────────────────────────────────────
void MainWindow::onSliderJ1Changed(int val)
{
    ui->labelJ1->setText(QString("%1°").arg(val));
    m_angles[0] = qDegreesToRadians(double(val));
    m_renderer->updateJointAngles(m_angles[0], m_angles[1], m_angles[2],
                                  m_angles[3], m_angles[4]);
    sendCurrentAngles();
}

void MainWindow::onSliderJ2Changed(int val)
{
    ui->labelJ2->setText(QString("%1°").arg(val));
    m_angles[1] = qDegreesToRadians(double(val));
    m_renderer->updateJointAngles(m_angles[0], m_angles[1], m_angles[2],
                                  m_angles[3], m_angles[4]);
    sendCurrentAngles();
}

void MainWindow::onSliderJ3Changed(int val)
{
    ui->labelJ3->setText(QString("%1°").arg(val));
    m_angles[2] = qDegreesToRadians(double(val));
    m_renderer->updateJointAngles(m_angles[0], m_angles[1], m_angles[2],
                                  m_angles[3], m_angles[4]);
    sendCurrentAngles();
}

void MainWindow::onSliderJ4Changed(int val)
{
    ui->labelJ4->setText(QString("%1°").arg(val));
    m_angles[3] = qDegreesToRadians(double(val));
    m_renderer->updateJointAngles(m_angles[0], m_angles[1], m_angles[2],
                                  m_angles[3], m_angles[4]);
    sendCurrentAngles();
}

void MainWindow::onSliderJ5Changed(int val)
{
    ui->labelJ5->setText(QString("%1°").arg(val));
    m_angles[4] = qDegreesToRadians(double(val));
    m_renderer->updateJointAngles(m_angles[0], m_angles[1], m_angles[2],
                                  m_angles[3], m_angles[4]);
    sendCurrentAngles();
}

// ────────────────────────────────────────────────────────────────────────────
// Slot: SerialManager báo thay đổi trạng thái kết nối
// ────────────────────────────────────────────────────────────────────────────
void MainWindow::onConnectionChanged(bool connected)
{
    // Enable/disable các combo khi kết nối/ngắt
    const bool canEdit = !connected;
    ui->comboPort->setEnabled(canEdit);
    ui->comboBaud->setEnabled(canEdit);
    ui->comboDataBits->setEnabled(canEdit);
    ui->comboStopBits->setEnabled(canEdit);
    ui->comboParity->setEnabled(canEdit);
    ui->comboFlow->setEnabled(canEdit);
    ui->btnRefresh->setEnabled(canEdit);

    if (connected) {
        ui->labelStatus->setText("● Đã kết nối: " + ui->comboPort->currentText());
        ui->labelStatus->setStyleSheet("color: #00CC44; font-weight: bold;");
        ui->btnConnect->setText("Ngắt kết nối");
        ui->btnConnect->setStyleSheet(
            "QPushButton { background-color: #c0392b; color: white; font-weight: bold; border-radius: 4px; }"
            "QPushButton:hover { background-color: #e74c3c; }"
            "QPushButton:pressed { background-color: #922b21; }");
    } else {
        ui->labelStatus->setText("● Chưa kết nối");
        ui->labelStatus->setStyleSheet("color: #FFA500; font-weight: bold;");
        ui->btnConnect->setText("Kết nối");
        ui->btnConnect->setStyleSheet(
            "QPushButton { background-color: #27ae60; color: white; font-weight: bold; border-radius: 4px; }"
            "QPushButton:hover { background-color: #2ecc71; }"
            "QPushButton:pressed { background-color: #1e8449; }");
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Slot: nhận góc khớp từ COM ("JOINT:q1,q2,q3,q4,q5\n")
// ────────────────────────────────────────────────────────────────────────────
void MainWindow::onJointAnglesReceived(double q1, double q2, double q3,
                                       double q4, double q5)
{
    m_angles[0] = q1; m_angles[1] = q2; m_angles[2] = q3;
    m_angles[3] = q4; m_angles[4] = q5;
    updateSlidersFromAngles();
    m_renderer->updateJointAngles(q1, q2, q3, q4, q5);
    sendCurrentAngles();
}

// ────────────────────────────────────────────────────────────────────────────
// Private: đồng bộ slider + nhãn từ m_angles[]  (blockSignals để tránh loop)
// ────────────────────────────────────────────────────────────────────────────
void MainWindow::updateSlidersFromAngles()
{
    auto block = [&](bool b) {
        ui->sliderJ1->blockSignals(b); ui->sliderJ2->blockSignals(b);
        ui->sliderJ3->blockSignals(b); ui->sliderJ4->blockSignals(b);
        ui->sliderJ5->blockSignals(b);
    };
    block(true);

    auto setSlider = [](QSlider* s, QLabel* l, double rad) {
        int deg = int(qRadiansToDegrees(rad));
        s->setValue(deg);
        l->setText(QString("%1°").arg(deg));
    };

    setSlider(ui->sliderJ1, ui->labelJ1, m_angles[0]);
    setSlider(ui->sliderJ2, ui->labelJ2, m_angles[1]);
    setSlider(ui->sliderJ3, ui->labelJ3, m_angles[2]);
    setSlider(ui->sliderJ4, ui->labelJ4, m_angles[3]);
    setSlider(ui->sliderJ5, ui->labelJ5, m_angles[4]);

    block(false);
}

// ────────────────────────────────────────────────────────────────────────────
// Private: gửi "STATUS:q1,q2,q3,q4,q5\n" ra COM nếu đang kết nối
// ────────────────────────────────────────────────────────────────────────────
void MainWindow::sendCurrentAngles()
{
    m_serial->sendStatus(m_angles);
}

