#include <algorithm>
#include "HandshakeMonitor.h"

namespace smi::dnc {

HandshakeMonitor::HandshakeMonitor(SerialPortFacade* serial, QObject* parent)
    : QObject(parent), m_serial(serial) {
    connect(&m_timer, &QTimer::timeout, this, &HandshakeMonitor::poll);
    m_timer.setTimerType(Qt::PreciseTimer);
    if (m_serial) {
        connect(m_serial, &SerialPortFacade::readyRead, this, &HandshakeMonitor::requestImmediatePoll);
        connect(m_serial, &SerialPortFacade::bytesWritten, this, &HandshakeMonitor::requestImmediatePoll);
    }
}

void HandshakeMonitor::start(int intervalMs) {
    m_initialized = false;
    m_pollQueued = false;
    poll();
    m_timer.start(std::max(1, intervalMs));
}
void HandshakeMonitor::stop() { m_timer.stop(); m_initialized = false; m_pollQueued = false; }

void HandshakeMonitor::requestImmediatePoll() {
    if (m_pollQueued) return;
    m_pollQueued = true;
    QMetaObject::invokeMethod(this, [this]() {
        m_pollQueued = false;
        poll();
    }, Qt::QueuedConnection);
}

void HandshakeMonitor::poll() {
    if (!m_serial || !m_serial->isOpen()) return;
    const auto snap = m_serial->readSignals();
    if (!m_initialized) {
        m_last = snap;
        m_initialized = true;
        emit ctsChanged(snap.cts);
        emit dsrChanged(snap.dsr);
        emit signalsChanged(snap);
        return;
    }
    if (snap.cts != m_last.cts) emit ctsChanged(snap.cts);
    if (snap.dsr != m_last.dsr) emit dsrChanged(snap.dsr);
    if (snap.cts != m_last.cts || snap.dsr != m_last.dsr || snap.dcd != m_last.dcd || snap.ri != m_last.ri) emit signalsChanged(snap);
    m_last = snap;
}

} // namespace smi::dnc
