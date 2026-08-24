#pragma once

#include <QObject>
#include <QSerialPort>
#include "../common/Result.h"
#include "../common/DncTypes.h"
#include "../config/MachineConfig.h"

namespace smi::dnc {

class SerialPortFacade : public QObject {
    Q_OBJECT
public:
    explicit SerialPortFacade(QObject* parent = nullptr);

    Result<void> open(const MachineConfig& config);
    void close();
    bool isOpen() const;

    qint64 writeBytes(const QByteArray& data);
    QByteArray readAvailable();
    qint64 bytesToWrite() const;
    qint64 bytesAvailable() const;
    bool waitForBytesWritten(int timeoutMs);
    bool clearOutputBuffer();
    SerialSignalSnapshot readSignals();
    QString errorString() const;

    void setDtr(bool enabled);
    void setRts(bool enabled);

signals:
    void readyRead();
    void bytesWritten(qint64 bytes);
    void portErrorOccurred(const QString& message);

private slots:
    void onReadyRead();
    void onBytesWritten(qint64 bytes);
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    QSerialPort m_port;
};

} // namespace smi::dnc
