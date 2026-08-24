#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QDateTime>
#include <QFile>
#include <QList>
#include <QByteArray>
#include <QJsonObject>
#include "../common/Result.h"
#include "../common/DncTypes.h"
#include "../config/MachineConfig.h"
#include "../storage/SessionTraceRepository.h"
#include "SessionStats.h"

namespace smi::dnc {

class SerialPortFacade;
class ProgramSource;
class RxInterpreter;
class HandshakeMonitor;
class FlowController;
class TxScheduler;
class Watchdog;
class DrainController;
class StateMachine;
class Logger;
class HistoryRepository;

class DncEngine : public QObject {
    Q_OBJECT
public:
    explicit DncEngine(QObject* parent = nullptr);
    ~DncEngine() override;

    Result<void> startSend(const QString& filePath, const MachineConfig& config);
    Result<void> startReceive(const QString& outputFilePath, const MachineConfig& config);
    void pause();
    void resume();
    void abort();
    void stopReceive();
    void finalizeSend();

    EngineState state() const;
    SessionStats stats() const;

signals:
    void stateChanged(const smi::dnc::StateTransition& transition);
    void statsUpdated(const smi::dnc::SessionStats& stats);
    void progressUpdated(qint64 sent, qint64 total);
    void rxTextReceived(const QString& text);
    void signalSnapshotUpdated(const smi::dnc::SerialSignalSnapshot& snapshot);
    void logLine(const QString& line);
    void sessionFinished(const smi::dnc::SessionResult& result);
    void sendReadyToFinalize(const QString& detail);
    void inactivityWarning(const QString& detail);
    // Emitido quando a recepcao fica em silencio: a porta NAO e fechada
    // automaticamente; quem decide e o operador.
    void receiveIdleWarning(const QString& detail);

private slots:
    void onSerialReadyRead();
    void onSerialBytesWritten(qint64 bytes);
    void onSerialError(const QString& message);
    void onRxEvent(const smi::dnc::RxEvent& event);
    void onSignalsChanged(const smi::dnc::SerialSignalSnapshot& snapshot);
    void onCtsChanged(bool high);
    void onSendPermissionChanged(bool allowed, smi::dnc::HoldReason reason);
    void onWatchdogHoldTriggered();
    void onWatchdogHoldReleased();
    void onResumeDebounceElapsed();
    void pumpTx();
    void onReadyTimeout();
    void onOverallTimeout();
    void onReceiveIdleTimeout();

public slots:
    void continueAfterInactivityWarning();
    // Decisao do operador apos o alerta de silencio na recepcao.
    void continueReceiveAfterIdleWarning();
    void finalizeReceiveAfterIdleWarning();

private:
    int configuredInactivityTimeoutMs() const;
    int inactivityGraceTimeoutMs() const;
    void restartInactivityTimer();
    bool isActiveSendSessionState(EngineState state) const;

private:
    Result<void> prepareSession(const QString& filePath, const MachineConfig& config, bool receiveMode);
    Result<void> openPortAndWire();
    void finishSession(SessionEndReason reason, const QString& detail);
    void refreshStateFromHoldReason(HoldReason reason);
    void tryEnterReadyOrSending();
    void beginDrainingIfNeeded();
    void updateRuntimeStats();
    void saveHistory(SessionEndReason reason, const QString& detail);
    void trace(const QString& category, const QString& message, const QJsonObject& context = {});
    void resetReceiveBuffers();
    void processReceivePayload(const QByteArray& data);
    void appendReceivedChunk(const QByteArray& data);
    bool isSendHoldState(EngineState state) const;
    void purgeManualTxBacklog(const QString& reason);
    bool drainSerialRxImmediate();

    MachineConfig m_config;
    SessionStats m_stats;
    QString m_filePath;
    QString m_receiveOutputPath;
    bool m_receiveMode = false;
    QDateTime m_sessionStartUtc;
    QString m_sessionId;

    SerialPortFacade* m_serial = nullptr;
    ProgramSource* m_source = nullptr;
    RxInterpreter* m_rx = nullptr;
    HandshakeMonitor* m_handshake = nullptr;
    FlowController* m_flow = nullptr;
    TxScheduler* m_scheduler = nullptr;
    Watchdog* m_watchdog = nullptr;
    DrainController* m_drain = nullptr;
    StateMachine* m_sm = nullptr;
    Logger* m_logger = nullptr;
    HistoryRepository* m_history = nullptr;
    SessionTraceRepository* m_traceRepo = nullptr;

    QTimer m_txPumpTimer;
    QTimer m_readyTimeoutTimer;
    QTimer m_overallTimeoutTimer;
    QTimer m_resumeDebounceTimer;
    QTimer m_receiveIdleTimer;
    QElapsedTimer m_sessionTimer;
    bool m_started = false;
    bool m_receiveSawPayload = false;
    bool m_sendAwaitingOperatorFinish = false;
    bool m_inactivityWarningActive = false;
    bool m_receiveIdleWarningActive = false;
    bool m_drainTimeoutWarningActive = false;
    qint64 m_holdStartedMs = -1;
    qint64 m_lastManualWriteMs = -1;
    qint64 m_manualStartGateUntilMs = -1;
    qint64 m_manualObserveUntilMs = -1;
    bool m_manualFirstByteSent = false;
    bool m_manualStartPrimed = false;
    qint64 m_firstXoffAtMs = -1;
    qint64 m_manualRecoveryUntilMs = -1;
    int m_manualRecoveryChunksRemaining = 0;
    int m_remoteWindowBlocks = 0;
    int m_remoteBlocksSinceResume = 0;
    qint64 m_remoteCooldownUntilMs = -1;
    double m_rateBucketTokens = 0.0;
    qint64 m_rateLastRefillMs = -1;
    qint64 m_rateLimitLogUntilMs = -1;
    qint64 m_lastResumeSignalMs = -1;
    qint64 m_lastThroughputSampleMs = -1;
    qint64 m_lastThroughputSampleBytes = 0;
    qint64 m_resumeLatencyTotalMs = 0;
    qint64 m_lastRxProcessedMs = -1;
    qint64 m_lastRxProcessedBytes = 0;

    QFile m_receiveFile;
    QByteArray m_receiveScanBuffer;
    bool m_receiveCapturing = false;
    QList<SessionTraceEvent> m_traceEvents;
};

} // namespace smi::dnc
