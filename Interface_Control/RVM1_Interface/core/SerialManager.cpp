#include "SerialManager.h"
#include <QSerialPortInfo>

SerialManager::SerialManager(QObject* parent)
    : QObject(parent)
    , m_serial(new QSerialPort(this))
{
    connect(m_serial, &QSerialPort::readyRead,
            this,     &SerialManager::onDataReceived);
}

SerialManager::~SerialManager()
{
    closePort();
}

bool SerialManager::openPort(const QString& portName, int baudRate)
{
    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serial->open(QIODevice::ReadWrite)) {
        emit connectionChanged(true);
        return true;
    }
    return false;
}

void SerialManager::closePort()
{
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

void SerialManager::sendStatus(const double angles[5])
{
    if (!m_serial->isOpen()) return;
    QString msg = QString("STATUS:%1,%2,%3,%4,%5\n")
                      .arg(angles[0]).arg(angles[1]).arg(angles[2])
                      .arg(angles[3]).arg(angles[4]);
    m_serial->write(msg.toUtf8());
}

void SerialManager::onDataReceived()
{
    m_buffer += m_serial->readAll();
    while (m_buffer.contains('\n')) {
        int idx = m_buffer.indexOf('\n');
        QString cmd = QString::fromUtf8(m_buffer.left(idx)).trimmed();
        m_buffer.remove(0, idx + 1);
        if (!cmd.isEmpty())
            parseCommand(cmd);
    }
}

void SerialManager::parseCommand(const QString& cmd)
{
    if (cmd.startsWith("JOINT:")) {
        QStringList v = cmd.mid(6).split(',');
        if (v.size() >= 5)
            emit jointAnglesReceived(v[0].toDouble(), v[1].toDouble(),
                                     v[2].toDouble(), v[3].toDouble(),
                                     v[4].toDouble());
    }
    else if (cmd.startsWith("POSE:")) {
        QStringList v = cmd.mid(5).split(',');
        if (v.size() >= 5)
            emit poseReceived(v[0].toDouble(), v[1].toDouble(),
                              v[2].toDouble(), v[3].toDouble(),
                              v[4].toDouble());
    }
    else if (cmd.startsWith("ESTOP")) {
        emit emergencyStop();
    }
}
