#include <algorithm>
#include "SerialPortFacade.h"

namespace smi::dnc {

namespace {
static int serialBitsPerFrame(const MachineConfig& c) {
    const int parityBits = (c.parity == QSerialPort::NoParity) ? 0 : 1;
    const int stopBits = (c.stopBits == QSerialPort::TwoStop) ? 2 : 1;
    return 1 + static_cast<int>(c.dataBits) + parityBits + stopBits;
}

static qint64 adaptiveWriteBufferForManualFlow(const MachineConfig& c) {
    const qint64 bytesPerSecond = std::max<qint64>(1, static_cast<qint64>(c.baudRate) / std::max(1, serialBitsPerFrame(c)));
    const qint64 bytesFor20ms = std::max<qint64>(1, (bytesPerSecond * 20) / 1000);
    return std::clamp<qint64>(bytesFor20ms, 64, 512);
}

static qint64 adaptiveReadBufferForManualFlow(const MachineConfig& c) {
    const qint64 bytesPerSecond = std::max<qint64>(1, static_cast<qint64>(c.baudRate) / std::max(1, serialBitsPerFrame(c)));
    const qint64 bytesFor80ms = std::max<qint64>(1, (bytesPerSecond * 80) / 1000);
    return std::clamp<qint64>(bytesFor80ms, 128, 2048);
}
}

SerialPortFacade::SerialPortFacade(QObject* parent) : QObject(parent) {
    connect(&m_port, &QSerialPort::readyRead, this, &SerialPortFacade::onReadyRead);
    connect(&m_port, &QSerialPort::bytesWritten, this, &SerialPortFacade::onBytesWritten);
    connect(&m_port, &QSerialPort::errorOccurred, this, &SerialPortFacade::onErrorOccurred);
}

Result<void> SerialPortFacade::open(const MachineConfig& config) {
    if (m_port.isOpen()) m_port.close();

    QSerialPort::FlowControl effectiveFlowControl = config.flowControl;
    const bool appManagedXonXoff = (effectiveFlowControl != QSerialPort::SoftwareControl) && config.interpretXonXoff;
    const bool appManagedCts = (effectiveFlowControl != QSerialPort::HardwareControl) && config.requireCtsHighToSend;
    if (appManagedCts || appManagedXonXoff) {
        effectiveFlowControl = QSerialPort::NoFlowControl;
    }

    m_port.setPortName(config.portName);
    m_port.setBaudRate(config.baudRate);
    m_port.setDataBits(config.dataBits);
    m_port.setParity(config.parity);
    m_port.setStopBits(config.stopBits);
    m_port.setFlowControl(effectiveFlowControl);
    qint64 desiredReadBuffer = std::max<qint64>(64, config.readBufferLimit);
    if (appManagedXonXoff || appManagedCts) {
        desiredReadBuffer = std::clamp<qint64>(desiredReadBuffer, adaptiveReadBufferForManualFlow(config), 4096);
    }
    m_port.setReadBufferSize(desiredReadBuffer);
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    // QSerialPort write buffering (Qt >= 6.10):
    // - In *manual* flow modes (XON/XOFF interpretado no app ou CTS obrigatorio),
    //   manter o "prefetch" extremamente curto evita overshoot apos XOFF/CTS-low.
    //   Se o stack Qt/OS tiver muitos bytes enfileirados, eles ainda podem drenar
    //   para a CNC mesmo depois de um HOLD, causando buffer overflow.
    // - Fora do modo manual, e seguro manter um buffer local maior para nao
    //   estrangular a UART.
    qint64 desiredWriteBuffer = std::max<qint64>(16, config.writeBufferLimit);
    if (config.manualSendRateLimitBps > 0) {
        const qint64 pacedCap = std::clamp<qint64>((std::max(1, config.manualSendRateLimitBps) * 20) / 1000, 16, 192);
        desiredWriteBuffer = std::max(desiredWriteBuffer, pacedCap);
    } else if (appManagedXonXoff || appManagedCts) {
        desiredWriteBuffer = std::clamp<qint64>(desiredWriteBuffer, adaptiveWriteBufferForManualFlow(config), 512);
    }
    m_port.setWriteBufferSize(desiredWriteBuffer);
#endif
    if (!m_port.open(QIODevice::ReadWrite)) return Result<void>::fail(m_port.errorString());

    m_port.setDataTerminalReady(config.dtrEnabled);
    if (effectiveFlowControl != QSerialPort::HardwareControl) {
        const bool effectiveRts = config.rtsEnabled || config.requireCtsHighToSend;
        m_port.setRequestToSend(effectiveRts);
    }
    return Result<void>::ok();
}

void SerialPortFacade::close() { if (m_port.isOpen()) m_port.close(); }
bool SerialPortFacade::isOpen() const { return m_port.isOpen(); }
qint64 SerialPortFacade::writeBytes(const QByteArray& data) { return m_port.write(data); }
QByteArray SerialPortFacade::readAvailable() { return m_port.readAll(); }
qint64 SerialPortFacade::bytesToWrite() const { return m_port.bytesToWrite(); }
qint64 SerialPortFacade::bytesAvailable() const { return m_port.bytesAvailable(); }
bool SerialPortFacade::waitForBytesWritten(int timeoutMs) { return m_port.waitForBytesWritten(timeoutMs); }
bool SerialPortFacade::clearOutputBuffer() { return m_port.clear(QSerialPort::Output); }
QString SerialPortFacade::errorString() const { return m_port.errorString(); }
void SerialPortFacade::setDtr(bool enabled) { m_port.setDataTerminalReady(enabled); }
void SerialPortFacade::setRts(bool enabled) { m_port.setRequestToSend(enabled); }

SerialSignalSnapshot SerialPortFacade::readSignals() {
    SerialSignalSnapshot s;
    const auto pinSignals = m_port.pinoutSignals();
    s.cts = pinSignals.testFlag(QSerialPort::ClearToSendSignal);
    s.dsr = pinSignals.testFlag(QSerialPort::DataSetReadySignal);
    s.dcd = pinSignals.testFlag(QSerialPort::DataCarrierDetectSignal);
    s.ri  = pinSignals.testFlag(QSerialPort::RingIndicatorSignal);
    return s;
}
void SerialPortFacade::onReadyRead() { emit readyRead(); }
void SerialPortFacade::onBytesWritten(qint64 bytes) { emit bytesWritten(bytes); }
void SerialPortFacade::onErrorOccurred(QSerialPort::SerialPortError error) {
    if (error == QSerialPort::NoError || error == QSerialPort::TimeoutError) return;
    emit portErrorOccurred(m_port.errorString());
}

} // namespace smi::dnc
