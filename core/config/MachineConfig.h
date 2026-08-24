#pragma once

#include <QString>
#include <QByteArray>
#include <QSerialPort>

namespace smi::dnc {

struct MachineConfig {
    QString machineName;
    QString portName;
    qint32 baudRate = 9600;
    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    QSerialPort::Parity parity = QSerialPort::NoParity;
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl;

    bool dtrEnabled = false;
    bool rtsEnabled = false;
    bool monitorDsr = false;
    int signalPollIntervalMs = 2;
    bool requireCtsHighToSend = false;
    bool requireXonToSend = false;
    bool interpretXonXoff = false;
    bool binarySafeMode = false;

    int readBufferLimit = 256;
    int writeBufferLimit = 256;

    // Optional manual cap for outgoing application-side throughput.
    // 0 = automatic pacing by the engine/flow-control logic.
    int manualSendRateLimitBps = 0;

    int initialChunkBytes = 32;
    int minChunkBytes = 8;
    int maxChunkBytes = 96;

    int noConsumeHoldMs = 250;
    int resumeDebounceMs = 40;
    int drainTimeoutMs = 3000;
    int readyTimeoutMs = 4000;
    int overallTimeoutMs = 0;

    // Remote drip-feed pacing by parsed NC blocks
    bool remoteBlockWindowMode = true;
    int remoteStartWindowBlocks = 2;
    int remoteTargetWindowBlocks = 6;
    int remoteMaxWindowBlocks = 8;
    int remoteBlockGuardMs = 3;
    int remoteWindowCooldownMs = 12;

    bool normalizeLineEndings = true;
    bool trimTrailingSpaces = false;
    QByteArray lineEnding = "\r";
    bool stripUtf8Bom = true;
    bool forceAscii7 = false;
    bool ensureTrailingLineEnding = true;
    bool autoWrapPercent = false;
    QByteArray sendPrefix;
    QByteArray sendSuffix;

    bool appendEof = false;
    QByteArray eofSequence;

    bool enableReceiveMode = true;
    QString defaultSendDirectory;
    QString defaultReceiveDirectory;

    // Receive protocol framing
    QByteArray receiveStartDelimiter;
    QByteArray receiveEndDelimiter;
    bool receiveRequireStartDelimiter = false;
    bool receiveStopOnEndDelimiter = false;
    bool receiveIncludeDelimitersInFile = false;
    bool receiveAutoFinishOnSilence = true;
    int receiveIdleFinishMs = 1500;

    // Logging / trace export
    QString sessionTraceDirectory;
    QString exportDirectory;

    // Integrated simulator defaults
    bool simulatorEnabled = false;
    int simulatorXoffCycleBytes = 256;
    int simulatorResumeDelayMs = 120;
};

} // namespace smi::dnc
