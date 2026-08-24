#pragma once

#include <QtGlobal>
#include <QString>

namespace smi::dnc {

struct SessionStats {
    qint64 fileBytesTotal = 0;
    qint64 bytesQueued = 0;
    qint64 bytesAcceptedByPort = 0;
    qint64 bytesReceived = 0;
    qint64 rxDataBytes = 0;
    qint64 pendingBytes = 0;
    qint64 receivedFramedBytes = 0;
    int receiveFramesCompleted = 0;

    int xonCount = 0;
    int xoffCount = 0;
    int ctsDropCount = 0;
    int ctsRiseCount = 0;
    int holdXoffCount = 0;
    int holdCtsCount = 0;
    int holdNoConsumeCount = 0;
    int resumeCount = 0;

    qint64 holdTimeMs = 0;
    qint64 sendingTimeMs = 0;
    qint64 drainingTimeMs = 0;
    qint64 sessionElapsedMs = 0;
    qint64 resumeLatencyAvgMs = 0;
    qint64 resumeLatencyMaxMs = 0;
    qint64 bytesSinceResume = 0;

    int currentChunkBytes = 0;
    int minChunkBytesObserved = 0;
    int maxChunkBytesObserved = 0;
    int chunkUpshiftCount = 0;
    int chunkDownshiftCount = 0;

    double wireTheoreticalBps = 0.0;
    double avgThroughputBps = 0.0;
    double effectiveThroughputBps = 0.0;
    double instantaneousThroughputBps = 0.0;
    double peakThroughputBps = 0.0;
    double estimatedConsumeBps = 0.0;
    double congestionRatio = 0.0;
    double lineEfficiency = 0.0;

    QString lastFileName;
    QString lastPortName;
    QString traceFilePath;
};

} // namespace smi::dnc
