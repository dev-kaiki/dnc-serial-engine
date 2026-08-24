#pragma once

#include <QObject>
#include <QTimer>
#include "../common/DncTypes.h"
#include "../serial/SerialPortFacade.h"

namespace smi::dnc {

class HandshakeMonitor : public QObject {
    Q_OBJECT
public:
    explicit HandshakeMonitor(SerialPortFacade* serial, QObject* parent = nullptr);
    void start(int intervalMs = 20);
    void stop();

signals:
    void signalsChanged(const smi::dnc::SerialSignalSnapshot& snapshot);
    void ctsChanged(bool high);
    void dsrChanged(bool high);

private slots:
    void poll();
    void requestImmediatePoll();

private:
    SerialPortFacade* m_serial = nullptr;
    QTimer m_timer;
    SerialSignalSnapshot m_last;
    bool m_initialized = false;
    bool m_pollQueued = false;
};

} // namespace smi::dnc
