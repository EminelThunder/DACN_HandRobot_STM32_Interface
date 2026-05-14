#include "SerialManager.h"
#include <QSerialPortInfo>
#include <QtMath>

// ============================================================
// Hằng số Modbus RTU — phải khớp với firmware STM32
// ============================================================
static constexpr uint8_t  MB_SLAVE_ID        = 0x01;
static constexpr uint8_t  MB_FUNC_READ       = 0x03;  // Read Holding Registers
static constexpr uint8_t  MB_FUNC_WRITE_MULTI= 0x10;  // Write Multiple Registers
static constexpr uint16_t MB_AXIS_OFFSET     = 32;    // Mỗi axis chiếm 32 register

// Register index (tương đối so với axis base)
static constexpr uint16_t REG_POS_TARGET_H   = 10;    // REG_POSITION_TARGET_H
static constexpr uint16_t REG_POS_TARGET_L   = 11;    // REG_POSITION_TARGET_L
static constexpr uint16_t REG_POS_PRESENT_H  = 18;    // REG_POSITION_PRESENT_H
static constexpr uint16_t REG_POS_PRESENT_L  = 19;    // REG_POSITION_PRESENT_L

static constexpr double   POS_SCALE          = 100.0; // 0.01° / unit
static constexpr int      POLL_INTERVAL_MS   = 100;   // Đọc vị trí mỗi 100 ms

// Kích thước frame phản hồi đọc 2 register: [id][0x03][4][H_H][H_L][L_H][L_L][crc_l][crc_h]
static constexpr int READ_RESP_LEN = 9;
// Kích thước frame phản hồi ghi nhiều: [id][0x10][addr_H][addr_L][qty_H][qty_L][crc_l][crc_h]
static constexpr int WRITE_RESP_LEN = 8;

// ============================================================
// Constructor / Destructor
// ============================================================
SerialManager::SerialManager(QObject* parent)
    : QObject(parent)
    , m_serial(new QSerialPort(this))
    , m_pollTimer(new QTimer(this))
{
    connect(m_serial,    &QSerialPort::readyRead, this, &SerialManager::onDataReceived);
    connect(m_pollTimer, &QTimer::timeout,        this, &SerialManager::onPollTimer);
}

SerialManager::~SerialManager()
{
    closePort();
}

// ============================================================
// openPort / closePort / isOpen / availablePorts
// ============================================================
bool SerialManager::openPort(const QString& portName, int baudRate)
{
    return openPort(portName, baudRate,
                    QSerialPort::Data8,
                    QSerialPort::OneStop,
                    QSerialPort::NoParity,
                    QSerialPort::NoFlowControl);
}

bool SerialManager::openPort(const QString&           portName,
                              int                      baudRate,
                              QSerialPort::DataBits    dataBits,
                              QSerialPort::StopBits    stopBits,
                              QSerialPort::Parity      parity,
                              QSerialPort::FlowControl flowControl)
{
    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(dataBits);
    m_serial->setParity(parity);
    m_serial->setStopBits(stopBits);
    m_serial->setFlowControl(flowControl);

    if (m_serial->open(QIODevice::ReadWrite)) {
        m_rxBuf.clear();
        m_pollState = PollState::IDLE;
        m_pollTimer->start(POLL_INTERVAL_MS);
        emit connectionChanged(true);
        return true;
    }
    return false;
}

void SerialManager::closePort()
{
    m_pollTimer->stop();
    if (m_serial->isOpen()) {
        m_serial->close();
        emit connectionChanged(false);
    }
}

bool SerialManager::isOpen() const
{
    return m_serial->isOpen();
}

QStringList SerialManager::availablePorts() const
{
    QStringList result;
    for (const auto& info : QSerialPortInfo::availablePorts())
        result << info.portName();
    return result;
}

// ============================================================
// CRC16 Modbus chuẩn (poly 0xA001, init 0xFFFF)
// ============================================================
uint16_t SerialManager::crc16(const uint8_t* data, int len)
{
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

// ============================================================
// buildWritePositionFrame
//
// Tạo frame Modbus WRITE_MULTIPLE để ghi vị trí đích cho 1 axis.
//
// Frame (13 byte):
//   [slave_id][0x10][addr_H][addr_L][0x00][0x02][0x04]
//   [val_H_high][val_H_low][val_L_high][val_L_low][crc_l][crc_h]
//
// angleDeg: góc đích đơn vị độ
// ============================================================
QByteArray SerialManager::buildWritePositionFrame(uint8_t axis, double angleDeg)
{
    // Chuyển đổi: độ → đơn vị 0.01° → int32 → chia H/L uint16
    auto     raw  = (int32_t)(angleDeg * POS_SCALE);
    auto     uraw = (uint32_t)raw;
    uint16_t valH = (uint16_t)(uraw >> 16);
    uint16_t valL = (uint16_t)(uraw & 0xFFFF);

    uint16_t addr = axis * MB_AXIS_OFFSET + REG_POS_TARGET_H;

    uint8_t buf[11];
    buf[0]  = MB_SLAVE_ID;
    buf[1]  = MB_FUNC_WRITE_MULTI;
    buf[2]  = (uint8_t)(addr >> 8);
    buf[3]  = (uint8_t)(addr & 0xFF);
    buf[4]  = 0x00;
    buf[5]  = 0x02;             // quantity = 2 registers
    buf[6]  = 0x04;             // byte count = 4
    buf[7]  = (uint8_t)(valH >> 8);
    buf[8]  = (uint8_t)(valH & 0xFF);
    buf[9]  = (uint8_t)(valL >> 8);
    buf[10] = (uint8_t)(valL & 0xFF);

    uint16_t crc = crc16(buf, 11);
    QByteArray frame(reinterpret_cast<const char*>(buf), 11);
    frame.append((char)(crc & 0xFF));
    frame.append((char)(crc >> 8));
    return frame;
}

// ============================================================
// buildReadPositionFrame
//
// Tạo frame Modbus READ_HOLDING để đọc vị trí hiện tại (2 register H+L).
//
// Frame (8 byte):
//   [slave_id][0x03][addr_H][addr_L][0x00][0x02][crc_l][crc_h]
// ============================================================
QByteArray SerialManager::buildReadPositionFrame(uint8_t axis)
{
    uint16_t addr = axis * MB_AXIS_OFFSET + REG_POS_PRESENT_H;

    uint8_t buf[6];
    buf[0] = MB_SLAVE_ID;
    buf[1] = MB_FUNC_READ;
    buf[2] = (uint8_t)(addr >> 8);
    buf[3] = (uint8_t)(addr & 0xFF);
    buf[4] = 0x00;
    buf[5] = 0x02;  // đọc 2 register

    uint16_t crc = crc16(buf, 6);
    QByteArray frame(reinterpret_cast<const char*>(buf), 6);
    frame.append((char)(crc & 0xFF));
    frame.append((char)(crc >> 8));
    return frame;
}

// ============================================================
// parseReadResponse
//
// Phân tích frame phản hồi READ_HOLDING 2 register (9 byte):
//   [id][0x03][0x04][H_H][H_L][L_H][L_L][crc_l][crc_h]
//
// Trả về true + outDeg nếu frame hợp lệ.
// ============================================================
bool SerialManager::parseReadResponse(const QByteArray& buf, double& outDeg)
{
    if (buf.size() < READ_RESP_LEN) return false;

    const auto* b = reinterpret_cast<const uint8_t*>(buf.constData());

    // Kiểm tra header
    if (b[0] != MB_SLAVE_ID) return false;
    if (b[1] != MB_FUNC_READ) return false;
    if (b[2] != 0x04) return false;  // byte_count phải = 4

    // Kiểm tra CRC
    uint16_t calcCrc = crc16(b, READ_RESP_LEN - 2);
    uint16_t recvCrc = (uint16_t)(b[READ_RESP_LEN - 2]) |
                       ((uint16_t)(b[READ_RESP_LEN - 1]) << 8);
    if (calcCrc != recvCrc) return false;

    // Ghép H và L thành int32
    uint16_t valH = ((uint16_t)b[3] << 8) | b[4];
    uint16_t valL = ((uint16_t)b[5] << 8) | b[6];
    auto     raw  = (int32_t)(((uint32_t)valH << 16) | (uint32_t)valL);

    // Chuyển về độ
    outDeg = (double)raw / POS_SCALE;
    return true;
}

// ============================================================
// sendStatus
//
// Ghi vị trí đích xuống STM32 cho 2 axis:
//   Axis 0 ← angles[0] (J1 - Eo)
//   Axis 1 ← angles[1] (J2 - Vai)
//
// angles[] đơn vị: radian → tự động đổi sang độ trong hàm này.
// Fire-and-forget: không chờ ACK — phản hồi ghi sẽ bị loại bỏ.
// ============================================================
void SerialManager::sendStatus(const double angles[5])
{
    if (!m_serial->isOpen()) return;

    for (uint8_t axis = 0; axis < 2; ++axis) {
        double deg = qRadiansToDegrees(angles[axis]);
        m_serial->write(buildWritePositionFrame(axis, deg));
    }
}

// ============================================================
// onPollTimer — định kỳ khởi động chu kỳ đọc vị trí
//
// State machine:
//   IDLE       → gửi READ axis 0 → WAIT_AXIS0
//   WAIT_AXIS0 → nhận đủ 9 byte → phân tích → gửi READ axis 1 → WAIT_AXIS1
//   WAIT_AXIS1 → nhận đủ 9 byte → phân tích → emit signal → IDLE
//
// Nếu timer tích tiếp mà vẫn đang chờ → reset về IDLE (timeout)
// ============================================================
void SerialManager::onPollTimer()
{
    if (!m_serial->isOpen()) return;

    if (m_pollState == PollState::IDLE) {
        // Bắt đầu chu kỳ: đọc axis 0
        m_rxBuf.clear();
        m_serial->write(buildReadPositionFrame(0));
        m_pollState = PollState::WAIT_AXIS0;
    }
    else {
        // Timeout: đang chờ response nhưng timer đã tích lần nữa → reset
        m_rxBuf.clear();
        m_pollState = PollState::IDLE;
    }
}

// ============================================================
// onDataReceived — tích lũy bytes, phân tích khi đủ frame
// ============================================================
void SerialManager::onDataReceived()
{
    m_rxBuf += m_serial->readAll();

    // Khi đang chờ response READ (9 byte)
    if (m_pollState == PollState::WAIT_AXIS0 ||
        m_pollState == PollState::WAIT_AXIS1)
    {
        // Bỏ qua các byte rác ở đầu (response write = 8 byte có func 0x10)
        // Tìm byte bắt đầu hợp lệ: slave_id=0x01, func=0x03
        while (m_rxBuf.size() >= 2) {
            auto b0 = (uint8_t)m_rxBuf[0];
            auto b1 = (uint8_t)m_rxBuf[1];
            if (b0 == MB_SLAVE_ID && b1 == MB_FUNC_READ) break;
            m_rxBuf.remove(0, 1);  // bỏ 1 byte rác
        }

        if (m_rxBuf.size() < READ_RESP_LEN) return;  // chưa đủ

        double deg = 0.0;
        bool ok = parseReadResponse(m_rxBuf.left(READ_RESP_LEN), deg);
        m_rxBuf.remove(0, READ_RESP_LEN);

        if (ok) {
            if (m_pollState == PollState::WAIT_AXIS0) {
                // Lưu góc axis 0 → gửi yêu cầu đọc axis 1
                m_presentAngles[0] = qDegreesToRadians(deg);
                m_rxBuf.clear();
                m_serial->write(buildReadPositionFrame(1));
                m_pollState = PollState::WAIT_AXIS1;
            }
            else {  // WAIT_AXIS1
                // Lưu góc axis 1 → phát tín hiệu cập nhật giao diện
                m_presentAngles[1] = qDegreesToRadians(deg);
                emit jointAnglesReceived(m_presentAngles[0], m_presentAngles[1],
                                         m_presentAngles[2], m_presentAngles[3],
                                         m_presentAngles[4]);
                m_pollState = PollState::IDLE;
            }
        }
        else {
            // Frame không hợp lệ → reset
            m_pollState = PollState::IDLE;
        }
    }
    // Khi không trong trạng thái polling: xả buffer tránh tràn
    else {
        if (m_rxBuf.size() > 256) m_rxBuf.clear();
    }
}

