#pragma once

#include <QObject>
#include "../config/MachineConfig.h"

namespace smi::dnc {

class TxScheduler : public QObject {
    Q_OBJECT
public:
    explicit TxScheduler(QObject* parent = nullptr);

    void setConfig(const MachineConfig& config);
    int nextChunkSize(qint64 remainingBytes, qint64 bytesToWrite) const;
    void onHoldEntered();
    void onResumeWindow();
    void onWriteAccepted(qint64 bytes);
    void onStableProgress();
    void reset();
    int currentChunkSize() const;
    int minChunkObserved() const;
    int maxChunkObserved() const;
    int upshiftCount() const;
    int downshiftCount() const;
    qint64 bytesSinceResume() const;

private:
    void noteChunkObservation();

    MachineConfig m_config;
    int m_currentChunk = 32;
    int m_minChunkObserved = 0;
    int m_maxChunkObserved = 0;
    int m_upshiftCount = 0;
    int m_downshiftCount = 0;
    int m_stableProgressCounter = 0;
    qint64 m_bytesSinceResume = 0;
    qint64 m_stableAcceptedBytes = 0;
};

} // namespace smi::dnc
