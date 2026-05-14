#pragma once
#include <QObject>
#include <QSerialPort>
#include <QStringList>
#include <QTimer>

// ============================================================
// SerialManager — Giao tiếp với STM32 qua Modbus RTU over USB CDC
//
// Giao thức: Modbus RTU binary (không phải text)
//   Slave ID : 0x01
//   Baud     : tuỳ cấu hình (thường 115200)
//   Cấu trúc : mỗi board điều khiển 2 trục (axis 0, axis 1)
//
// Ánh xạ mặc định:
//   Axis 0  ↔  Joint index 0 (J1 - Eo)
//   Axis 1  ↔  Joint index 1 (J2 - Vai)
//
// Đơn vị vị trí STM32: 0.01° (góc_độ × 100 → int32, chia H/L uint16)
//
// Thanh ghi (MODBUS_AXIS_OFFSET = 32):
//   Địa chỉ tuyệt đối = axis * 32 + reg_index
//   REG_POSITION_TARGET_H  = 10  (ghi đích)
//   REG_POSITION_TARGET_L  = 11
//   REG_POSITION_PRESENT_H = 18  (đọc hiện tại)
//   REG_POSITION_PRESENT_L = 19
// ============================================================

class SerialManager : public QObject {
    Q_OBJECT
public:
    explicit SerialManager(QObject* parent = nullptr);
    ~SerialManager();

    // Mở cổng (baud only - các tham số còn lại mặc định 8N1)
    bool openPort(const QString& portName, int baudRate = 115200);

    // Mở cổng với đầy đủ tham số
    bool openPort(const QString&              portName,
                  int                         baudRate,
                  QSerialPort::DataBits        dataBits,
                  QSerialPort::StopBits        stopBits,
                  QSerialPort::Parity          parity,
                  QSerialPort::FlowControl     flowControl);

    void        closePort();
    bool        isOpen() const;
    QStringList availablePorts() const;

    // Gửi góc đích cho các khớp được ánh xạ đến board này
    // angles[5] đơn vị: radian
    void sendStatus(const double angles[5]);

signals:
    // Phát ra khi đọc được vị trí hiện tại từ STM32 (radian)
    void jointAnglesReceived(double q1, double q2, double q3,
                             double q4, double q5);
    void poseReceived(double x, double y, double z,
                     double pitch, double roll);
    void emergencyStop();
    void connectionChanged(bool connected);

private slots:
    void onDataReceived();
    void onPollTimer();     // Định kỳ đọc vị trí hiện tại từ STM32

private:
    // ── Modbus RTU helpers ──────────────────────────────────
    static uint16_t   crc16(const uint8_t* data, int len);
    static QByteArray buildWritePositionFrame(uint8_t axis, double angleDeg);
    static QByteArray buildReadPositionFrame(uint8_t axis);

    // Phân tích response READ_HOLDING (9 byte) → góc hiện tại (độ)
    // Trả về true nếu frame hợp lệ
    bool parseReadResponse(const QByteArray& buf, double& outDeg);

    // ── Trạng thái polling ──────────────────────────────────
    enum class PollState { IDLE, WAIT_AXIS0, WAIT_AXIS1 };
    PollState m_pollState = PollState::IDLE;

    // ── Thành viên ──────────────────────────────────────────
    QSerialPort* m_serial;
    QTimer*      m_pollTimer;
    QByteArray   m_rxBuf;

    // Lưu góc hiện tại của tất cả 5 khớp (radian); cập nhật dần khi đọc từ STM32
    double       m_presentAngles[5]{};
};

