#include "Watchdog.h"

namespace smi::dnc {

Watchdog::Watchdog(QObject* parent) : QObject(parent) {
    connect(&m_timer, &QTimer::timeout, this, &Watchdog::check);
}

void Watchdog::setConfig(const MachineConfig& config) { m_config = config; }
void Watchdog::start() { m_progressTimer.start(); m_hold = false; m_timer.start(50); }
void Watchdog::stop() { m_timer.stop(); m_hold = false; }

void Watchdog::markForwardProgress() {
    m_progressTimer.restart();
    if (m_hold) { m_hold = false; emit holdReleased(); }
}

void Watchdog::clearHold() {
    if (m_hold) { m_hold = false; emit holdReleased(); }
    m_progressTimer.restart();
}

void Watchdog::check() {
    if (m_config.noConsumeHoldMs <= 0) return;
    if (!m_hold && m_progressTimer.isValid() && m_progressTimer.elapsed() >= m_config.noConsumeHoldMs) {
        m_hold = true;
        emit holdTriggered();
    }
}

} // namespace smi::dnc
