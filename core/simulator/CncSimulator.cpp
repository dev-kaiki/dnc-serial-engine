#include "CncSimulator.h"

namespace smi::dnc {

CncSimulator::CncSimulator(QObject* parent) : QObject(parent) {
    m_resumeTimer.setSingleShot(true);
    connect(&m_resumeTimer, &QTimer::timeout, this, &CncSimulator::releaseXon);
}

void CncSimulator::setXoffCycleBytes(int bytes) { m_xoffCycleBytes = bytes; }
void CncSimulator::setResumeDelayMs(int delayMs) { m_resumeDelayMs = delayMs; }

void CncSimulator::feedBytes(const QByteArray& data) {
    m_report.bytesFed += data.size();
    m_report.configuredCycleBytes = m_xoffCycleBytes;
    m_report.resumeDelayMs = m_resumeDelayMs;
    m_buffered += data.size();
    if (!m_holding && m_buffered >= m_xoffCycleBytes) {
        m_holding = true;
        m_report.xoffCount++;
        emit generatedRx(QByteArray(1, char(0x13)));
        m_resumeTimer.start(m_resumeDelayMs);
        m_buffered = 0;
    }
}

SimulationReport CncSimulator::simulatePayload(const QByteArray& data) {
    reset();
    const int chunk = qMax(1, m_xoffCycleBytes / 4);
    for (int i = 0; i < data.size(); i += chunk) {
        feedBytes(data.mid(i, chunk));
        if (m_holding) {
            releaseXon();
        }
    }
    emit reportReady(m_report);
    return m_report;
}

void CncSimulator::reset() {
    m_buffered = 0;
    m_holding = false;
    m_resumeTimer.stop();
    m_report = SimulationReport{};
}

void CncSimulator::releaseXon() {
    if (m_holding) {
        m_holding = false;
        m_report.xonCount++;
        emit generatedRx(QByteArray(1, char(0x11)));
    }
}

} // namespace smi::dnc
