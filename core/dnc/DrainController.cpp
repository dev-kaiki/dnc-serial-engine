#include "DrainController.h"

#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace smi::dnc {

DrainController::DrainController(QObject* parent) : QObject(parent) {}
void DrainController::setConfig(const MachineConfig& config) { m_config = config; }

qint64 DrainController::computeAdaptiveTimeoutMs(qint64 pendingBytes, double effectiveBps, double wireBps) const {
    const qint64 baseMs = std::max<qint64>(3000, m_config.drainTimeoutMs);
    const double safeEffectiveBps = (effectiveBps > 1.0) ? effectiveBps : 0.0;
    const double safeWireBps = (wireBps > 1.0) ? wireBps : 0.0;

    double referenceBps = 0.0;
    if (safeEffectiveBps > 0.0 && safeWireBps > 0.0) {
        referenceBps = std::min(safeWireBps, std::max(16.0, safeEffectiveBps));
    } else if (safeEffectiveBps > 0.0) {
        referenceBps = std::max(16.0, safeEffectiveBps);
    } else if (safeWireBps > 0.0) {
        referenceBps = std::max(16.0, safeWireBps * 0.25);
    } else {
        referenceBps = 32.0;
    }

    const qint64 estimatedDrainMs = static_cast<qint64>(std::ceil((std::max<qint64>(0, pendingBytes) * 1000.0) / referenceBps));
    const qint64 guardMs = std::clamp<qint64>(1500 + (pendingBytes * 2), 5000, 45000);
    return std::clamp<qint64>(std::max(baseMs, estimatedDrainMs + guardMs), 8000, 180000);
}

qint64 DrainController::computeFinishThresholdBytes(qint64 pendingBytes, double effectiveBps, double wireBps) const {
    Q_UNUSED(effectiveBps);
    Q_UNUSED(wireBps);
    const qint64 clampedPending = std::max<qint64>(0, pendingBytes);
    const qint64 proportional = clampedPending / 3;
    const qint64 guarded = qBound<qint64>(96LL, proportional, 768LL);
    if (clampedPending <= 0) return 0;
    return std::min(clampedPending, guarded);
}

qint64 DrainController::computeQuietFinishWindowMs(double effectiveBps, double wireBps) const {
    const double referenceBps = std::max({16.0, effectiveBps, wireBps * 0.20});
    const qint64 adaptive = static_cast<qint64>(std::ceil(9000.0 / referenceBps * 1000.0 / 1000.0));
    return qBound<qint64>(600LL, std::max<qint64>(adaptive, qint64(m_config.resumeDebounceMs) * 6LL), 2500LL);
}

void DrainController::begin(qint64 pendingBytes, double effectiveBps, double wireBps) {
    m_active = true;
    m_timeoutBudgetMs = computeAdaptiveTimeoutMs(pendingBytes, effectiveBps, wireBps);
    m_finishThresholdBytes = computeFinishThresholdBytes(pendingBytes, effectiveBps, wireBps);
    m_quietFinishWindowMs = computeQuietFinishWindowMs(effectiveBps, wireBps);
    m_lastBytesToWrite = pendingBytes;
    m_lowestBytesToWrite = pendingBytes;
    m_timer.start();
    m_progressTimer.start();
    resetStableWindow();
}

void DrainController::reset() {
    m_active = false;
    m_timeoutBudgetMs = 0;
    m_finishThresholdBytes = 0;
    m_quietFinishWindowMs = 0;
    m_lastBytesToWrite = -1;
    m_lowestBytesToWrite = -1;
}

bool DrainController::isActive() const { return m_active; }
qint64 DrainController::timeoutBudgetMs() const { return m_timeoutBudgetMs; }
qint64 DrainController::finishThresholdBytes() const { return m_finishThresholdBytes; }
qint64 DrainController::quietFinishWindowMs() const { return m_quietFinishWindowMs; }

void DrainController::resetStableWindow() {
    m_stableTimer.invalidate();
}

void DrainController::noteDrainProgress(qint64 bytesToWrite) const {
    if (!m_active) return;
    auto* self = const_cast<DrainController*>(this);
    const bool progressed = (self->m_lastBytesToWrite < 0 || bytesToWrite < self->m_lastBytesToWrite);
    if (!self->m_progressTimer.isValid() || progressed) {
        self->m_progressTimer.start();
        self->resetStableWindow();
    }
    if (self->m_lowestBytesToWrite < 0 || bytesToWrite < self->m_lowestBytesToWrite) {
        self->m_lowestBytesToWrite = bytesToWrite;
    }
    self->m_lastBytesToWrite = bytesToWrite;
}

bool DrainController::isTimedOut(qint64 bytesToWrite, bool sendAllowed) const {
    if (!m_active || !m_timer.isValid()) return false;

    noteDrainProgress(bytesToWrite);

    if (!sendAllowed) return false;

    if (canFinish(bytesToWrite, sendAllowed)) {
        return false;
    }

    const qint64 budgetMs = std::max<qint64>(3000, m_timeoutBudgetMs);
    if (bytesToWrite <= 0) {
        return m_timer.elapsed() > budgetMs;
    }

    const qint64 hardResidual = std::max<qint64>(m_finishThresholdBytes * 2, 1024);
    const qint64 stallBudgetMs = std::clamp<qint64>(budgetMs / 2, 5000, 45000);
    if (bytesToWrite > hardResidual && m_progressTimer.isValid() && m_progressTimer.elapsed() > stallBudgetMs) {
        return true;
    }

    return m_timer.elapsed() > budgetMs;
}

bool DrainController::canFinish(qint64 bytesToWrite, bool sendAllowed) const {
    if (!m_active) return false;

    noteDrainProgress(bytesToWrite);

    auto* self = const_cast<DrainController*>(this);

    if (!sendAllowed && bytesToWrite > 0) {
        self->resetStableWindow();
        return false;
    }

    const qint64 stableMs = qBound<qint64>(180LL, std::max<qint64>(m_quietFinishWindowMs, qint64(m_config.resumeDebounceMs) * 4LL), 3000LL);

    if (bytesToWrite <= 0) {
        if (!self->m_stableTimer.isValid()) {
            self->m_stableTimer.start();
            return false;
        }
        return self->m_stableTimer.elapsed() >= stableMs;
    }

    const bool nearFinished = (m_finishThresholdBytes > 0 && bytesToWrite <= m_finishThresholdBytes);
    const bool noProgressLately = self->m_progressTimer.isValid() && self->m_progressTimer.elapsed() >= stableMs;
    if (!nearFinished || !noProgressLately) {
        self->resetStableWindow();
        return false;
    }

    if (!self->m_stableTimer.isValid()) {
        self->m_stableTimer.start();
        return false;
    }

    return self->m_stableTimer.elapsed() >= stableMs;
}

} // namespace smi::dnc
