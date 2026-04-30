#pragma once
#include <QObject>
#include <QSerialPort>
#include <QStringList>

class SerialManager : public QObject {
    Q_OBJECT
public:
    explicit SerialManager(QObject* parent = nullptr);
    ~SerialManager();

    // openPort cơ bản (baud only)
    bool        openPort(const QString& portName, int baudRate = 115200);

    // openPort đầy đủ tham số như QModMaster
    bool        openPort(const QString&              portName,
                         int                         baudRate,
                         QSerialPort::DataBits        dataBits,
                         QSerialPort::StopBits        stopBits,
                         QSerialPort::Parity          parity,
                         QSerialPort::FlowControl     flowControl);

    void        closePort();
    bool        isOpen() const;
    QStringList availablePorts() const;
    void        sendStatus(const double angles[5]);

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
