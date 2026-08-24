#include "TxScheduler.h"
#include <algorithm>

namespace smi::dnc {

TxScheduler::TxScheduler(QObject* parent) : QObject(parent) {}

void TxScheduler::setConfig(const MachineConfig& config) {
    m_config = config;
    reset();
}

namespace {
static bool manualFlowMode(const MachineConfig& c) {
    return (c.flowControl != QSerialPort::SoftwareControl && c.interpretXonXoff)
        || (c.flowControl != QSerialPort::HardwareControl && c.requireCtsHighToSend);
}

static bool strictRateMode(const MachineConfig& c) {
    return c.manualSendRateLimitBps > 0;
}

static int bytesPerSecondOnWire(const MachineConfig& c) {
    const int parityBits = (c.parity == QSerialPort::NoParity) ? 0 : 1;
    const int stopBits = (c.stopBits == QSerialPort::TwoStop) ? 2 : 1;
    const int bitsPerFrame = 1 + static_cast<int>(c.dataBits) + parityBits + stopBits;
    return std::max(1, static_cast<int>(c.baudRate / std::max(1, bitsPerFrame)));
}

static int manualInitialChunkCap(const MachineConfig& c) {
    const int wireBytesPer10ms = std::max(1, (bytesPerSecondOnWire(c) * 10) / 1000);
    return std::clamp(wireBytesPer10ms, 12, 48);
}

static int manualMaxChunkCap(const MachineConfig& c) {
    const int wireBytesPer60ms = std::max(1, (bytesPerSecondOnWire(c) * 60) / 1000);
    const int configuredCap = std::max(64, std::min(c.writeBufferLimit, 512));
    return std::clamp(std::max(wireBytesPer60ms, configuredCap / 2), 64, configuredCap);
}

static int strictRateChunkCap(const MachineConfig& c) {
    if (c.manualSendRateLimitBps <= 0) return 0;
    const int byTime = std::max(8, (c.manualSendRateLimitBps * 12) / 1000);
    const int wireCap = std::max(16, bytesPerSecondOnWire(c) / 6);
    return std::clamp(byTime, 8, std::min(128, wireCap));
}

}

void TxScheduler::noteChunkObservation() {
    if (m_minChunkObserved == 0 || m_currentChunk < m_minChunkObserved) m_minChunkObserved = m_currentChunk;
    if (m_currentChunk > m_maxChunkObserved) m_maxChunkObserved = m_currentChunk;
}

void TxScheduler::reset() {
    m_stableProgressCounter = 0;
    m_bytesSinceResume = 0;
    m_stableAcceptedBytes = 0;
    m_upshiftCount = 0;
    m_downshiftCount = 0;
    if (strictRateMode(m_config)) {
        m_currentChunk = std::min(16, strictRateChunkCap(m_config));
    } else if (manualFlowMode(m_config)) {
        m_currentChunk = manualInitialChunkCap(m_config);
    } else {
        m_currentChunk = std::clamp(m_config.initialChunkBytes, m_config.minChunkBytes, m_config.maxChunkBytes);
    }
    m_minChunkObserved = m_currentChunk;
    m_maxChunkObserved = m_currentChunk;
}

void TxScheduler::onHoldEntered() {
    m_stableProgressCounter = 0;
    const int before = m_currentChunk;
    if (strictRateMode(m_config)) {
        m_currentChunk = std::max(1, m_currentChunk / 2);
    } else if (manualFlowMode(m_config)) {
        const int floor = std::max(4, manualInitialChunkCap(m_config));
        if (m_bytesSinceResume < (before / 2)) {
            m_currentChunk = std::max(floor, m_currentChunk / 2);
        } else if (m_bytesSinceResume < (before * 2)) {
            m_currentChunk = std::max(floor, (m_currentChunk * 2) / 3);
        } else {
            m_currentChunk = std::max(floor, (m_currentChunk * 3) / 4);
        }
    } else {
        m_currentChunk = std::max(m_config.minChunkBytes, m_currentChunk / 2);
    }
    if (m_currentChunk < before) ++m_downshiftCount;
    m_bytesSinceResume = 0;
    m_stableAcceptedBytes = 0;
    noteChunkObservation();
}

void TxScheduler::onResumeWindow() {
    m_stableProgressCounter = 0;
    m_bytesSinceResume = 0;
    m_stableAcceptedBytes = 0;
}

void TxScheduler::onWriteAccepted(qint64 bytes) {
    if (bytes > 0) {
        m_bytesSinceResume += bytes;
        m_stableAcceptedBytes += bytes;
    }
}

void TxScheduler::onStableProgress() {
    const int before = m_currentChunk;
    if (strictRateMode(m_config)) {
        ++m_stableProgressCounter;
        const int cap = strictRateChunkCap(m_config);
        m_currentChunk = std::min(cap, m_currentChunk + std::max(2, cap / 8));
    } else if (manualFlowMode(m_config)) {
        const qint64 growthThreshold = std::max<qint64>(8, static_cast<qint64>(m_currentChunk) * 3);
        if (m_stableAcceptedBytes < growthThreshold) {
            noteChunkObservation();
            return;
        }

        ++m_stableProgressCounter;
        if (m_stableProgressCounter >= 2) {
            const int cap = manualMaxChunkCap(m_config);
            const int step = (m_currentChunk < 12) ? 1 : ((m_currentChunk < 24) ? 2 : 3);
            m_currentChunk = std::min(cap, m_currentChunk + step);
            m_stableProgressCounter = 0;
            m_stableAcceptedBytes = 0;
        }
    } else {
        ++m_stableProgressCounter;
        m_currentChunk = std::min(m_config.maxChunkBytes, m_currentChunk + std::max(1, m_config.minChunkBytes / 2));
    }
    if (m_currentChunk > before) ++m_upshiftCount;
    noteChunkObservation();
}

int TxScheduler::currentChunkSize() const {
    return m_currentChunk;
}

int TxScheduler::minChunkObserved() const { return m_minChunkObserved; }
int TxScheduler::maxChunkObserved() const { return m_maxChunkObserved; }
int TxScheduler::upshiftCount() const { return m_upshiftCount; }
int TxScheduler::downshiftCount() const { return m_downshiftCount; }
qint64 TxScheduler::bytesSinceResume() const { return m_bytesSinceResume; }

int TxScheduler::nextChunkSize(qint64 remainingBytes, qint64 bytesToWrite) const {
    if (remainingBytes <= 0) return 0;

    if (strictRateMode(m_config)) {
        const int highWaterCap = std::max(strictRateChunkCap(m_config), m_currentChunk * 2);
        const int dynamicHighWater = std::min(std::max(16, m_config.writeBufferLimit), highWaterCap);
        const int freeMargin = std::max(0, dynamicHighWater - static_cast<int>(bytesToWrite));
        if (freeMargin <= 0) return 0;
        const int desired = std::min(m_currentChunk, freeMargin);
        const int cap = strictRateChunkCap(m_config);
        const int bounded = std::max(1, std::min(desired, cap));
        return static_cast<int>(std::min<qint64>(bounded, remainingBytes));
    }

    if (manualFlowMode(m_config)) {
        const int dynamicHighWater = std::clamp(std::max(m_currentChunk * 3, manualInitialChunkCap(m_config) * 5), 48, std::max(64, std::min(m_config.writeBufferLimit, 512)));
        const int freeMargin = std::max(0, dynamicHighWater - static_cast<int>(bytesToWrite));
        if (freeMargin <= 0) return 0;
        const int desired = std::min(m_currentChunk, freeMargin);
        const int bounded = std::max(1, std::min(desired, manualMaxChunkCap(m_config)));
        return static_cast<int>(std::min<qint64>(bounded, remainingBytes));
    }

    const int freeMargin = std::max(0, m_config.writeBufferLimit - static_cast<int>(bytesToWrite));
    if (freeMargin <= 0) return 0;
    const int desired = std::min(m_currentChunk, freeMargin);
    const int bounded = std::max(m_config.minChunkBytes, std::min(desired, m_config.maxChunkBytes));
    return static_cast<int>(std::min<qint64>(bounded, remainingBytes));
}

} // namespace smi::dnc
