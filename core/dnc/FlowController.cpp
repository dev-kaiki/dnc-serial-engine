#include "FlowController.h"

namespace smi::dnc {

FlowController::FlowController(QObject* parent) : QObject(parent) {}
void FlowController::setConfig(const MachineConfig& config) { m_config = config; reset(); }
void FlowController::setOperatorPaused(bool paused) { m_operatorPaused = paused; recalc(); }
void FlowController::onXon() { m_xoffActive = false; recalc(); }
void FlowController::onXoff() { m_xoffActive = true; recalc(); }
void FlowController::onCtsChanged(bool high) { m_ctsHigh = high; recalc(); }
void FlowController::onWatchdogHold(bool hold) { m_watchdogHold = hold; recalc(); }

void FlowController::reset() {
    m_operatorPaused = false;
        // Many CNCs using XON/XOFF do not emit an initial XON at session start.
    // Preserve permissive startup by default, but honor explicit requireXonToSend.
    m_xoffActive = m_config.requireXonToSend;
    m_ctsHigh = !m_config.requireCtsHighToSend;
    m_watchdogHold = false;
    m_reason = HoldReason::None;
    m_lastAllowed = false;
    recalc();
}

bool FlowController::sendAllowed() const { return m_reason == HoldReason::None; }
HoldReason FlowController::currentHoldReason() const { return m_reason; }

void FlowController::recalc() {
    HoldReason newReason = HoldReason::None;
    if (m_operatorPaused) newReason = HoldReason::OperatorPause;
    else if (m_config.interpretXonXoff && m_xoffActive) newReason = HoldReason::Xoff;
    else if (m_config.requireCtsHighToSend && !m_ctsHigh) newReason = HoldReason::CtsLow;
    else if (m_watchdogHold) newReason = HoldReason::NoConsume;

    const bool allowed = (newReason == HoldReason::None);
    const bool changed = (m_reason != newReason) || (m_lastAllowed != allowed);
    m_reason = newReason;
    m_lastAllowed = allowed;
    if (changed) emit sendPermissionChanged(allowed, m_reason);
}

} // namespace smi::dnc
