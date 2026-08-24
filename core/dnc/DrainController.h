#pragma once

#include <QObject>
#include <QElapsedTimer>
#include "../config/MachineConfig.h"

namespace smi::dnc {

class DrainController : public QObject {
    Q_OBJECT
public:
    explicit DrainController(QObject* parent = nullptr);
    void setConfig(const MachineConfig& config);
    void begin(qint64 pendingBytes, double effectiveBps, double wireBps);
    void reset();
    bool isActive() const;
    bool isTimedOut(qint64 bytesToWrite, bool sendAllowed) const;
    bool canFinish(qint64 bytesToWrite, bool sendAllowed) const;
    qint64 timeoutBudgetMs() const;
    qint64 finishThresholdBytes() const;
    qint64 quietFinishWindowMs() const;

private:
    void resetStableWindow();
    void noteDrainProgress(qint64 bytesToWrite) const;
    qint64 computeAdaptiveTimeoutMs(qint64 pendingBytes, double effectiveBps, double wireBps) const;
    qint64 computeFinishThresholdBytes(qint64 pendingBytes, double effectiveBps, double wireBps) const;
    qint64 computeQuietFinishWindowMs(double effectiveBps, double wireBps) const;

private:
    MachineConfig m_config;
    QElapsedTimer m_timer;
    QElapsedTimer m_stableTimer;
    mutable QElapsedTimer m_progressTimer;
    bool m_active = false;
    qint64 m_timeoutBudgetMs = 0;
    qint64 m_finishThresholdBytes = 0;
    qint64 m_quietFinishWindowMs = 0;
    mutable qint64 m_lastBytesToWrite = -1;
    mutable qint64 m_lowestBytesToWrite = -1;
};

} // namespace smi::dnc
