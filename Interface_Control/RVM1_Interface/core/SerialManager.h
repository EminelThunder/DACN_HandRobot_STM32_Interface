#pragma once
#include <QObject>
#include <QSerialPort>
#include <QStringList>

// Giao thức nhận qua COM:
//   "JOINT:q1,q2,q3,q4,q5\n"    — góc khớp (radian)
//   "POSE:x,y,z,pitch,roll\n"   — vị trí (mm, radian)
//   "ESTOP\n"                   — dừng khẩn cấp

class SerialManager : public QObject {
    Q_OBJECT
public:
    explicit SerialManager(QObject* parent = nullptr);
    ~SerialManager();

    bool         openPort(const QString& portName, int baudRate = 115200);
    void         closePort();
    bool         isOpen() const;
    QStringList  availablePorts() const;
    void         sendStatus(const double angles[5]);

signals:
    void jointAnglesReceived(double q1, double q2, double q3,
                             double q4, double q5);
    void poseReceived(double x, double y, double z,
                     double pitch, double roll);
    void emergencyStop();
    void connectionChanged(bool connected);

private slots:
    void onDataReceived();

private:
    QSerialPort* m_serial;
    QByteArray   m_buffer;
    void parseCommand(const QString& cmd);
};
