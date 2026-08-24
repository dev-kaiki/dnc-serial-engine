#include "DncEngine.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QDir>
#include <algorithm>
#include "../serial/SerialPortFacade.h"
#include "../storage/HistoryRepository.h"
#include "ProgramSource.h"
#include "RxInterpreter.h"
#include "HandshakeMonitor.h"
#include "FlowController.h"
#include "TxScheduler.h"
#include "Watchdog.h"
#include "DrainController.h"
#include "StateMachine.h"
#include "../logging/Logger.h"
#include "../config/ConfigurationValidator.h"

namespace smi::dnc {

namespace {
static bool manualFlowModeFor(const MachineConfig& c) {
    return (c.flowControl != QSerialPort::SoftwareControl && c.interpretXonXoff)
    || (c.flowControl != QSerialPort::HardwareControl && c.requireCtsHighToSend);
}

static bool strictRateModeFor(const MachineConfig& c) {
    return c.manualSendRateLimitBps > 0;
}

static int serialBitsPerFrameFor(const MachineConfig& c) {
    const int parityBits = (c.parity == QSerialPort::NoParity) ? 0 : 1;
    const int stopBits = (c.stopBits == QSerialPort::TwoStop) ? 2 : 1;
    return 1 + static_cast<int>(c.dataBits) + parityBits + stopBits;
}

static int bytesPerSecondOnWireFor(const MachineConfig& c) {
    return std::max(1, static_cast<int>(c.baudRate / std::max(1, serialBitsPerFrameFor(c))));
}

static int bytesForWireTimeMs(const MachineConfig& c, int ms) {
    return std::max(1, (bytesPerSecondOnWireFor(c) * std::max(1, ms)) / 1000);
}

static int manualStartGateMsFor(const MachineConfig& c) {
    const int gate = (serialBitsPerFrameFor(c) * 2 * 1000) / std::max(1, static_cast<int>(c.baudRate));
    return std::clamp(gate, c.interpretXonXoff ? 3 : 2, c.interpretXonXoff ? 8 : 6);
}

static int manualFirstByteObserveMsFor(const MachineConfig& c) {
    const int observe = (serialBitsPerFrameFor(c) * 2 * 1000) / std::max(1, static_cast<int>(c.baudRate));
    return std::clamp(observe, 1, 4);
}

static bool nativeFlowModeFor(const MachineConfig& c) {
    return c.flowControl == QSerialPort::SoftwareControl || c.flowControl == QSerialPort::HardwareControl;
}

static int manualResumeGuardMsFor(const MachineConfig& c) {
    const int wireMs = (serialBitsPerFrameFor(c) * 3 * 1000) / std::max(1, static_cast<int>(c.baudRate));
    if (c.interpretXonXoff) return std::clamp(wireMs, 6, 12);
    if (c.requireCtsHighToSend) return std::clamp(wireMs, 3, 8);
    return std::clamp(c.resumeDebounceMs, 0, 20);
}

static int manualInterByteGapMsFor(const MachineConfig& c, bool firstByteAlreadySent) {
    if (!firstByteAlreadySent) return 0;
    if (c.interpretXonXoff) return 0;
    if (c.requireCtsHighToSend) return 0;
    return 0;
}

static bool remoteBlockModeFor(const MachineConfig&) {
    // Byte-paced manual flow proved more reliable than parsed-block windows for this CNC.
    return false;
}

static bool useNoConsumeWatchdogFor(const MachineConfig& c) {
    // In manual flow modes (XON/XOFF or CTS/RTS), lack of immediate local progress
    // does not mean the machine is stalled; the sender may be intentionally waiting
    // for the next software flow event. Using the generic no-consume watchdog here
    // causes false HoldNoConsume right after startup.
    return !(manualFlowModeFor(c) || nativeFlowModeFor(c));
}

static int manualLocalHighWaterFor(const MachineConfig& c, int currentChunk) {
    const int wireBudget = bytesForWireTimeMs(c, 20);
    const int desired = std::max(currentChunk * 4, currentChunk + wireBudget);
    const int bounded = std::clamp(desired, 64, 512);
    return std::min(std::max(64, bounded), std::max(64, c.writeBufferLimit));
}

static int strictRateBurstBytesFor(const MachineConfig& c) {
    if (c.manualSendRateLimitBps <= 0) return 0;
    const int msWindow = 12;
    const int byTime = std::max(8, (c.manualSendRateLimitBps * msWindow) / 1000);
    const int wireCap = std::max(16, bytesPerSecondOnWireFor(c) / 6);
    return std::clamp(byTime, 8, std::min(128, wireCap));
}
}

DncEngine::DncEngine(QObject* parent)
    : QObject(parent),
    m_serial(new SerialPortFacade(this)),
    m_source(new ProgramSource()),
    m_rx(new RxInterpreter(this)),
    m_handshake(new HandshakeMonitor(m_serial, this)),
    m_flow(new FlowController(this)),
    m_scheduler(new TxScheduler(this)),
    m_watchdog(new Watchdog(this)),
    m_drain(new DrainController(this)),
    m_sm(new StateMachine(this)),
    m_logger(new Logger(this)),
    m_history(new HistoryRepository()),
    m_traceRepo(new SessionTraceRepository()) {
    connect(m_serial, &SerialPortFacade::readyRead, this, &DncEngine::onSerialReadyRead);
    connect(m_serial, &SerialPortFacade::bytesWritten, this, &DncEngine::onSerialBytesWritten);
    connect(m_serial, &SerialPortFacade::portErrorOccurred, this, &DncEngine::onSerialError);
    connect(m_rx, &RxInterpreter::rxEvent, this, &DncEngine::onRxEvent);
    connect(m_handshake, &HandshakeMonitor::signalsChanged, this, &DncEngine::onSignalsChanged);
    connect(m_handshake, &HandshakeMonitor::ctsChanged, this, &DncEngine::onCtsChanged);
    connect(m_flow, &FlowController::sendPermissionChanged, this, &DncEngine::onSendPermissionChanged);
    connect(m_watchdog, &Watchdog::holdTriggered, this, &DncEngine::onWatchdogHoldTriggered);
    connect(m_watchdog, &Watchdog::holdReleased, this, &DncEngine::onWatchdogHoldReleased);
    connect(m_sm, &StateMachine::stateChanged, this, &DncEngine::stateChanged);
    connect(m_sm, &StateMachine::stateChanged, this, [this](const StateTransition& tr) {
        m_logger->log(LogLevel::Info, "STATE", QString("%1 -> %2 | %3").arg(toString(tr.from), toString(tr.to), tr.reason));
        trace("state", QString("%1 -> %2").arg(toString(tr.from), toString(tr.to)), {{"reason", tr.reason}});
    });
    connect(m_logger, &Logger::logLineReady, this, &DncEngine::logLine);
    connect(&m_txPumpTimer, &QTimer::timeout, this, &DncEngine::pumpTx);
    connect(&m_readyTimeoutTimer, &QTimer::timeout, this, &DncEngine::onReadyTimeout);
    connect(&m_overallTimeoutTimer, &QTimer::timeout, this, &DncEngine::onOverallTimeout);
    connect(&m_resumeDebounceTimer, &QTimer::timeout, this, &DncEngine::onResumeDebounceElapsed);
    connect(&m_receiveIdleTimer, &QTimer::timeout, this, &DncEngine::onReceiveIdleTimeout);
    m_receiveIdleTimer.setSingleShot(true);
    m_txPumpTimer.setTimerType(Qt::PreciseTimer);
    m_txPumpTimer.setInterval(1);
    m_readyTimeoutTimer.setSingleShot(true);
    m_overallTimeoutTimer.setSingleShot(true);
    m_resumeDebounceTimer.setSingleShot(true);
}

DncEngine::~DncEngine() {
    delete m_source;
    delete m_history;
    delete m_traceRepo;
}

Result<void> DncEngine::startSend(const QString& filePath, const MachineConfig& config) {
    return prepareSession(filePath, config, false);
}

Result<void> DncEngine::startReceive(const QString& outputFilePath, const MachineConfig& config) {
    return prepareSession(outputFilePath, config, true);
}

Result<void> DncEngine::prepareSession(const QString& filePath, const MachineConfig& config, bool receiveMode) {
    if (m_started) return Result<void>::fail("Já existe uma sessão em andamento.");

    auto valid = ConfigurationValidator::validate(config);
    if (!valid.isOk()) return valid;

    m_receiveMode = receiveMode;
    m_config = config;
    m_filePath = receiveMode ? QString() : filePath;
    m_receiveOutputPath = receiveMode ? filePath : QString();
    m_stats = SessionStats{};
    m_holdStartedMs = -1;
    m_lastManualWriteMs = -1;
    m_manualStartGateUntilMs = -1;
    m_manualObserveUntilMs = -1;
    m_manualFirstByteSent = false;
    m_manualStartPrimed = false;
    m_firstXoffAtMs = -1;
    m_manualRecoveryUntilMs = -1;
    m_manualRecoveryChunksRemaining = 0;
    m_remoteWindowBlocks = qMax(1, config.remoteStartWindowBlocks);
    m_remoteBlocksSinceResume = 0;
    m_remoteCooldownUntilMs = -1;
    m_rateBucketTokens = 0.0;
    m_rateLastRefillMs = -1;
    m_rateLimitLogUntilMs = -1;
    m_lastResumeSignalMs = -1;
    m_lastThroughputSampleMs = -1;
    m_lastThroughputSampleBytes = 0;
    m_resumeLatencyTotalMs = 0;
    m_lastRxProcessedMs = -1;
    m_lastRxProcessedBytes = 0;
    m_receiveSawPayload = false;
    m_sendAwaitingOperatorFinish = false;
    m_inactivityWarningActive = false;
    m_receiveIdleWarningActive = false;
    m_drainTimeoutWarningActive = false;
    m_traceEvents.clear();
    resetReceiveBuffers();
    m_sessionStartUtc = QDateTime::currentDateTimeUtc();
    m_sessionId = m_sessionStartUtc.toString("yyyyMMdd_hhmmss_zzz");
    m_stats.lastFileName = QFileInfo(filePath).fileName();
    m_stats.lastPortName = config.portName;

    m_sm->transitionTo(EngineState::Preparing, "Iniciando preparação da sessão.");
    trace("session", receiveMode ? "Preparando sessão de recepção" : "Preparando sessão de envio",
          {{"filePath", filePath}, {"portName", config.portName}, {"receiveMode", receiveMode}});

    if (receiveMode) {
        QFileInfo fi(m_receiveOutputPath);
        QDir().mkpath(fi.absolutePath());
        m_receiveFile.setFileName(m_receiveOutputPath);
        if (!m_receiveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return Result<void>::fail("Falha ao preparar arquivo de recepção.");
        }
        m_receiveFile.close();
    } else {
        auto loaded = m_source->loadFromFile(filePath, config);
        if (!loaded.isOk()) {
            finishSession(SessionEndReason::SourceError, loaded.error());
            return loaded;
        }
        m_stats.fileBytesTotal = m_source->totalSize();
    }

    m_rx->setConfig(config);
    m_flow->setConfig(config);
    m_scheduler->setConfig(config);
    m_drain->setConfig(config);

    m_sm->transitionTo(EngineState::OpeningPort, "Abrindo porta serial.");
    auto port = openPortAndWire();
    if (!port.isOk()) {
        finishSession(SessionEndReason::PortOpenFailed, port.error());
        return port;
    }

    m_sm->transitionTo(receiveMode ? EngineState::Receiving : EngineState::WaitMachineReady,
                       receiveMode ? "Modo recepção ativo." : "Aguardando máquina liberar envio.");

    if (!receiveMode) {
        // Monitorar sinais sempre durante envio melhora o diagnóstico e reduz
        // latência para refletir mudanças reais de CTS/DSR/DCD/RI na UI/log.
        m_handshake->start(std::max(1, m_config.signalPollIntervalMs));
    } else {
        m_handshake->stop();
    }
    m_sessionTimer.start();
    m_started = true;

    if (!receiveMode) {
        m_watchdog->setConfig(m_config);
        if (useNoConsumeWatchdogFor(m_config)) m_watchdog->start();
        else m_watchdog->stop();
        const bool manualFlowMode = manualFlowModeFor(m_config);
        const bool strictRateMode = strictRateModeFor(m_config);
        m_txPumpTimer.setInterval((manualFlowMode || strictRateMode) ? 1 : 5);
        m_txPumpTimer.start();
        if ((manualFlowMode || strictRateMode) && m_sessionTimer.isValid()) {
            const int gateMs = manualStartGateMsFor(m_config);
            m_manualStartGateUntilMs = gateMs;
            trace("flow", "Janela inicial de envio armada", {{"startGateMs", gateMs}, {"wireBps", bytesPerSecondOnWireFor(m_config)}});
        }
        if (useNoConsumeWatchdogFor(m_config)) {
            trace("watchdog", "Watchdog local de transporte habilitado", {{"holdMs", m_config.noConsumeHoldMs}});
        }
        refreshStateFromHoldReason(m_flow->currentHoldReason());
    } else if (m_config.receiveAutoFinishOnSilence && m_config.receiveIdleFinishMs > 0) {
        m_receiveIdleTimer.start(m_config.receiveIdleFinishMs);
    }

    if (m_config.readyTimeoutMs > 0 && !receiveMode) m_readyTimeoutTimer.start(m_config.readyTimeoutMs);
    if (receiveMode && m_config.overallTimeoutMs > 0) m_overallTimeoutTimer.start(m_config.overallTimeoutMs);

    if (!receiveMode) tryEnterReadyOrSending();
    emit statsUpdated(m_stats);
    return Result<void>::ok();
}

Result<void> DncEngine::openPortAndWire() {
    auto opened = m_serial->open(m_config);
    if (!opened.isOk()) return opened;

    const QString flowMode = m_config.requireCtsHighToSend
                                 ? QStringLiteral("RTS/CTS")
                                 : (m_config.interpretXonXoff ? QStringLiteral("XON/XOFF") : QStringLiteral("SEM_CONTROLE"));
    trace("session", "Porta serial aberta", {
                                                {"flowMode", flowMode},
                                                {"baudRate", m_config.baudRate},
                                                {"dataBits", static_cast<int>(m_config.dataBits)},
                                                {"parity", static_cast<int>(m_config.parity)},
                                                {"stopBits", static_cast<int>(m_config.stopBits)}
                                            });

    const auto initialSignals = m_serial->readSignals();
    emit signalSnapshotUpdated(initialSignals);
    m_flow->onCtsChanged(initialSignals.cts);
    return Result<void>::ok();
}
void DncEngine::pause() { m_flow->setOperatorPaused(true); }
void DncEngine::resume() { m_flow->setOperatorPaused(false); }
void DncEngine::abort() { if (m_started) finishSession(SessionEndReason::OperatorAbort, "Abortado pelo operador."); }
void DncEngine::stopReceive() { if (m_started && m_receiveMode) finishSession(SessionEndReason::Success, "Recepção finalizada pelo operador."); }
void DncEngine::finalizeSend() {
    if (m_started && !m_receiveMode && m_sendAwaitingOperatorFinish) {
        finishSession(SessionEndReason::Success, "Envio concluído com sucesso após confirmação do operador.");
    }
}
EngineState DncEngine::state() const { return m_sm->state(); }
SessionStats DncEngine::stats() const { return m_stats; }

int DncEngine::configuredInactivityTimeoutMs() const {
    if (m_config.overallTimeoutMs > 0) return m_config.overallTimeoutMs;
    if (m_config.readyTimeoutMs > 0) return m_config.readyTimeoutMs;
    return 0;
}

int DncEngine::inactivityGraceTimeoutMs() const {
    const int base = configuredInactivityTimeoutMs();
    if (base <= 0) return 0;
    return std::max(60000, base);
}

bool DncEngine::isActiveSendSessionState(EngineState state) const {
    return state == EngineState::Sending
           || state == EngineState::HoldOperator
           || state == EngineState::HoldXoff
           || state == EngineState::HoldCtsLow
           || state == EngineState::HoldNoConsume
           || state == EngineState::Draining;
}

void DncEngine::restartInactivityTimer() {
    if (!m_started || m_receiveMode || m_sendAwaitingOperatorFinish) return;
    if (!isActiveSendSessionState(m_sm->state())) return;
    const int timeoutMs = m_inactivityWarningActive ? inactivityGraceTimeoutMs() : configuredInactivityTimeoutMs();
    if (timeoutMs > 0) m_overallTimeoutTimer.start(timeoutMs);
}

void DncEngine::continueAfterInactivityWarning() {
    if (!m_started || m_receiveMode || !m_inactivityWarningActive) return;
    m_inactivityWarningActive = false;
    trace("timeout", "Operador optou por continuar aguardando após alerta de inatividade", {{"nextTimeoutMs", configuredInactivityTimeoutMs()}});
    emit logLine("Alerta de inatividade reconhecido. Mantendo porta aberta e retomando monitoramento.");

    // Se o alerta veio do timeout de drenagem, o orcamento ja estourou. Sem
    // reinicia-lo, isTimedOut() continuaria verdadeiro e o alerta voltaria em
    // laco a cada pumpTx. Reabrir o orcamento e o que da ao operador uma nova
    // janela real de observacao.
    if (m_drainTimeoutWarningActive && m_drain->isActive()) {
        m_drainTimeoutWarningActive = false;
        const qint64 pendingBytes = std::max<qint64>(0, m_stats.bytesQueued - m_stats.bytesAcceptedByPort);
        m_drain->begin(pendingBytes, m_stats.effectiveThroughputBps, m_stats.wireTheoreticalBps);
        trace("drain", "Orçamento de drenagem reiniciado após operador manter porta aberta",
              {{"pendingBytes", pendingBytes},
               {"newDrainBudgetMs", m_drain->timeoutBudgetMs()}});
        emit logLine(QString("Pendente local: %1 bytes. Novo orçamento de observação: %2 s.")
                         .arg(pendingBytes)
                         .arg(m_drain->timeoutBudgetMs() / 1000));
    }
    m_drainTimeoutWarningActive = false;

    restartInactivityTimer();
    updateRuntimeStats();
    emit statsUpdated(m_stats);
}

void DncEngine::continueReceiveAfterIdleWarning() {
    if (!m_started || !m_receiveMode || !m_receiveIdleWarningActive) return;
    m_receiveIdleWarningActive = false;
    trace("receive", "Operador optou por manter a porta aberta após silêncio na recepção",
          {{"idleMs", m_config.receiveIdleFinishMs}, {"bytesReceived", m_stats.bytesReceived}});
    emit logLine("Silêncio de recepção reconhecido. Mantendo porta aberta e aguardando novos dados da máquina.");
    if (m_config.receiveIdleFinishMs > 0) {
        m_receiveIdleTimer.start(m_config.receiveIdleFinishMs);
    }
    updateRuntimeStats();
    emit statsUpdated(m_stats);
}

void DncEngine::finalizeReceiveAfterIdleWarning() {
    if (!m_started || !m_receiveMode || !m_receiveIdleWarningActive) return;
    m_receiveIdleWarningActive = false;
    trace("receive", "Operador confirmou finalização da recepção após silêncio.",
          {{"bytesReceived", m_stats.bytesReceived}});
    finishSession(SessionEndReason::Success,
                  "Recepção finalizada por silêncio na linha RS232 após confirmação do operador.");
}

void DncEngine::trace(const QString& category, const QString& message, const QJsonObject& context) {
    SessionTraceEvent ev;
    ev.timestampUtc = QDateTime::currentDateTimeUtc();
    ev.category = category;
    ev.message = message;
    ev.context = context;
    m_traceEvents.append(ev);
}

void DncEngine::resetReceiveBuffers() {
    m_receiveScanBuffer.clear();
    m_receiveCapturing = !m_config.receiveRequireStartDelimiter;
}

void DncEngine::appendReceivedChunk(const QByteArray& data) {
    if (data.isEmpty()) return;
    QFile f(m_receiveOutputPath);
    if (f.open(QIODevice::Append)) {
        f.write(data);
        f.close();
        m_stats.receivedFramedBytes += data.size();
    }
}

void DncEngine::processReceivePayload(const QByteArray& data) {
    if (data.isEmpty()) return;
    m_receiveScanBuffer.append(data);

    while (!m_receiveScanBuffer.isEmpty()) {
        if (!m_receiveCapturing) {
            if (m_config.receiveStartDelimiter.isEmpty()) {
                m_receiveCapturing = true;
            } else {
                const int startPos = m_receiveScanBuffer.indexOf(m_config.receiveStartDelimiter);
                if (startPos < 0) {
                    const int keep = qMax(0, m_config.receiveStartDelimiter.size() - 1);
                    if (m_receiveScanBuffer.size() > keep)
                        m_receiveScanBuffer = m_receiveScanBuffer.right(keep);
                    return;
                }
                if (m_config.receiveIncludeDelimitersInFile) appendReceivedChunk(m_config.receiveStartDelimiter);
                m_receiveScanBuffer.remove(0, startPos + m_config.receiveStartDelimiter.size());
                m_receiveCapturing = true;
                trace("receive", "Delimitador inicial encontrado");
            }
        }

        if (m_receiveCapturing && m_config.receiveStopOnEndDelimiter && !m_config.receiveEndDelimiter.isEmpty()) {
            const int endPos = m_receiveScanBuffer.indexOf(m_config.receiveEndDelimiter);
            if (endPos < 0) {
                const int keep = qMax(0, m_config.receiveEndDelimiter.size() - 1);
                const int safeWrite = qMax(0, m_receiveScanBuffer.size() - keep);
                if (safeWrite > 0) {
                    appendReceivedChunk(m_receiveScanBuffer.left(safeWrite));
                    m_receiveScanBuffer.remove(0, safeWrite);
                }
                return;
            }
            if (endPos > 0) appendReceivedChunk(m_receiveScanBuffer.left(endPos));
            if (m_config.receiveIncludeDelimitersInFile) appendReceivedChunk(m_config.receiveEndDelimiter);
            m_receiveScanBuffer.remove(0, endPos + m_config.receiveEndDelimiter.size());
            m_stats.receiveFramesCompleted++;
            trace("receive", "Frame completo recebido", {{"frames", m_stats.receiveFramesCompleted}});
            if (m_config.receiveStopOnEndDelimiter) {
                finishSession(SessionEndReason::Success, "Recepção encerrada por delimitador final.");
                return;
            }
            m_receiveCapturing = !m_config.receiveRequireStartDelimiter;
        } else {
            appendReceivedChunk(m_receiveScanBuffer);
            m_receiveScanBuffer.clear();
            return;
        }
    }
}

bool DncEngine::drainSerialRxImmediate() {
    bool drainedAny = false;
    qint64 drainedBytes = 0;
    int loops = 0;
    while (m_serial && m_serial->isOpen() && loops < 16) {
        const QByteArray data = m_serial->readAvailable();
        if (data.isEmpty()) break;
        drainedAny = true;
        drainedBytes += data.size();
        m_stats.bytesReceived += data.size();
        trace("rx", "Dados recebidos", {{"bytes", static_cast<qint64>(data.size())}, {"sessionElapsedMs", m_sessionTimer.isValid() ? m_sessionTimer.elapsed() : 0}});
        if (m_receiveMode) {
            m_receiveSawPayload = true;
            if (m_config.receiveAutoFinishOnSilence && m_config.receiveIdleFinishMs > 0) {
                m_receiveIdleTimer.start(m_config.receiveIdleFinishMs);
            }
            processReceivePayload(data);
        }
        if (m_started) m_rx->process(data);
        ++loops;
    }
    if (drainedAny && m_sessionTimer.isValid()) {
        m_lastRxProcessedMs = m_sessionTimer.elapsed();
        m_lastRxProcessedBytes = drainedBytes;
        if (!m_receiveMode) restartInactivityTimer();
        emit statsUpdated(m_stats);
    }
    return drainedAny;
}

void DncEngine::onSerialReadyRead() {
    drainSerialRxImmediate();
}

void DncEngine::onSerialBytesWritten(qint64 bytes) {
    m_stats.bytesAcceptedByPort += bytes;
    m_watchdog->markForwardProgress();

    const bool manualFlowMode = manualFlowModeFor(m_config);
    if (!manualFlowMode) {
        m_scheduler->onStableProgress();
    } else if (bytes >= 16) {
        // Em modo manual, crescimento agressivo por callback gera oscilação.
        // Só consideramos progresso "estável" quando a drenagem local foi
        // relevante; o abastecimento fino vem do refill imediato abaixo.
        m_scheduler->onStableProgress();
    }

    trace("tx", "Bytes aceitos pela porta", {{"bytes", bytes}, {"chunkSize", m_scheduler->currentChunkSize()}, {"bytesToWrite", m_serial->bytesToWrite()}});
    restartInactivityTimer();
    updateRuntimeStats();
    emit statsUpdated(m_stats);
    emit progressUpdated(m_stats.bytesAcceptedByPort, m_stats.fileBytesTotal);
    if (!m_receiveMode && !m_source->hasRemaining()) beginDrainingIfNeeded();

    if (manualFlowMode && m_started && !m_receiveMode && m_flow->sendAllowed() && !m_drain->isActive()) {
        if (!drainSerialRxImmediate()) pumpTx();
    }
}

void DncEngine::purgeManualTxBacklog(const QString& reason) {
    Q_UNUSED(reason);
    // Nao limpar/rebobinar bytes ja entregues ao stack serial.
    // Em CNC real isso pode duplicar ou truncar bloco e gerar erro de formato.
    // A contencao e feita antes da escrita, por janela curta e chunk pequeno.
}

void DncEngine::onSerialError(const QString& message) { finishSession(SessionEndReason::PortError, message); }

void DncEngine::onRxEvent(const RxEvent& event) {
    switch (event.type) {
    case RxEventType::Xon:
        m_stats.xonCount++;
        trace("flow", "XON recebido", {{"pendingBytes", std::max<qint64>(0, m_stats.bytesQueued - m_stats.bytesAcceptedByPort)}});
        m_flow->onXon();
        restartInactivityTimer();
        m_watchdog->clearHold();
        if (remoteBlockModeFor(m_config)) {
            m_remoteBlocksSinceResume = 0;
            m_remoteWindowBlocks = qMin(m_config.remoteMaxWindowBlocks, qMax(m_remoteWindowBlocks + 1, m_config.remoteTargetWindowBlocks));
            trace("flow", "Janela de blocos ampliada após XON", {{"windowBlocks", m_remoteWindowBlocks}});
        }
        if (m_started && !m_receiveMode && m_sessionTimer.isValid()) {
            const qint64 now = m_sessionTimer.elapsed();
            m_lastResumeSignalMs = now;
            m_scheduler->onResumeWindow();
            if (m_config.interpretXonXoff) {
                const int guardMs = manualResumeGuardMsFor(m_config);
                m_manualObserveUntilMs = now + guardMs;
                m_manualRecoveryUntilMs = now + guardMs + 6;
                m_manualRecoveryChunksRemaining = strictRateModeFor(m_config) ? 2 : 3;
                m_remoteCooldownUntilMs = -1;
                trace("flow", "Retomada guardada após XON", {{"guardMs", guardMs}, {"bytesSinceResume", m_scheduler->bytesSinceResume()}});
            }
            pumpTx();
        }
        break;
    case RxEventType::Xoff:
        m_stats.xoffCount++;
        m_stats.holdXoffCount++;
        trace("flow", "XOFF recebido", {{"pendingBytes", std::max<qint64>(0, m_stats.bytesQueued - m_stats.bytesAcceptedByPort)}, {"bytesSinceResume", m_scheduler->bytesSinceResume()}, {"chunk", m_scheduler->currentChunkSize()}});
        m_flow->onXoff();
        purgeManualTxBacklog(QStringLiteral("XOFF"));
        if (remoteBlockModeFor(m_config)) {
            m_remoteWindowBlocks = qMax(1, m_remoteWindowBlocks / 2);
            m_remoteBlocksSinceResume = 0;
            trace("flow", "Janela de blocos reduzida após XOFF", {{"windowBlocks", m_remoteWindowBlocks}});
        }
        if (m_firstXoffAtMs < 0 && m_sessionTimer.isValid()) {
            m_firstXoffAtMs = m_sessionTimer.elapsed();
            trace("flow", "Primeiro XOFF observado", {{"elapsedMs", m_firstXoffAtMs}, {"bytesQueued", m_stats.bytesQueued}, {"bytesAcceptedByPort", m_stats.bytesAcceptedByPort}});
        }
        if (m_sessionTimer.isValid()) {
            const qint64 now = m_sessionTimer.elapsed();
            m_manualObserveUntilMs = now + manualResumeGuardMsFor(m_config);
            m_manualRecoveryUntilMs = now + manualResumeGuardMsFor(m_config) + 8;
            m_manualRecoveryChunksRemaining = strictRateModeFor(m_config) ? 2 : 4;
        }
        m_scheduler->onHoldEntered();
        break;
    case RxEventType::Text:
        m_stats.rxDataBytes += event.raw.size();
        emit rxTextReceived(event.text);
        break;
    case RxEventType::Data:
    case RxEventType::Binary:
        m_stats.rxDataBytes += event.raw.size();
        break;
    default:
        break;
    }
    updateRuntimeStats();
    emit statsUpdated(m_stats);
}

void DncEngine::onSignalsChanged(const SerialSignalSnapshot& snapshot) {
    trace("signals", "Snapshot RS232", {
                                           {"cts", snapshot.cts},
                                           {"dsr", snapshot.dsr},
                                           {"dcd", snapshot.dcd},
                                           {"ri", snapshot.ri}
                                       });
    emit signalSnapshotUpdated(snapshot);
    if (!m_receiveMode && snapshot.cts) restartInactivityTimer();
}

void DncEngine::onCtsChanged(bool high) {
    if (high) {
        m_stats.ctsRiseCount++;
        trace("flow", "CTS alto", {{"sessionElapsedMs", m_sessionTimer.isValid() ? m_sessionTimer.elapsed() : 0}});
    } else {
        m_stats.ctsDropCount++;
        m_stats.holdCtsCount++;
        trace("flow", "CTS baixo", {{"pendingBytes", std::max<qint64>(0, m_stats.bytesQueued - m_stats.bytesAcceptedByPort)}, {"bytesSinceResume", m_scheduler->bytesSinceResume()}});
        m_scheduler->onHoldEntered();
        purgeManualTxBacklog(QStringLiteral("CTS_LOW"));
    }
    m_flow->onCtsChanged(high);
    emit statsUpdated(m_stats);
    if (high && m_started && !m_receiveMode) {
        if (m_config.requireCtsHighToSend && m_sessionTimer.isValid()) {
            const qint64 now = m_sessionTimer.elapsed();
            const int guardMs = manualResumeGuardMsFor(m_config);
            m_lastResumeSignalMs = now;
            m_scheduler->onResumeWindow();
            m_manualObserveUntilMs = now + guardMs;
            m_manualRecoveryUntilMs = now + guardMs + 5;
            m_manualRecoveryChunksRemaining = strictRateModeFor(m_config) ? 2 : 3;
        }
        pumpTx();
    }
}

void DncEngine::onSendPermissionChanged(bool allowed, HoldReason reason) {
    if (!m_started || m_receiveMode) return;
    trace("flow", allowed ? "Permissão de envio liberada" : "Permissão de envio bloqueada", {{"reason", toString(reason)}, {"bytesToWrite", m_serial->bytesToWrite()}});
    refreshStateFromHoldReason(reason);
    if (reason == HoldReason::None) {
        restartInactivityTimer();
        if (m_holdStartedMs >= 0) {
            m_stats.holdTimeMs += (m_sessionTimer.elapsed() - m_holdStartedMs);
            m_holdStartedMs = -1;
        }
        const bool manualFlowMode = manualFlowModeFor(m_config);
        if (m_sessionTimer.isValid() && m_lastResumeSignalMs >= 0) {
            const qint64 latency = std::max<qint64>(0, m_sessionTimer.elapsed() - m_lastResumeSignalMs);
            ++m_stats.resumeCount;
            m_resumeLatencyTotalMs += latency;
            m_stats.resumeLatencyAvgMs = m_resumeLatencyTotalMs / std::max(1, m_stats.resumeCount);
            m_stats.resumeLatencyMaxMs = std::max(m_stats.resumeLatencyMaxMs, latency);
            m_lastResumeSignalMs = -1;
        }
        if (remoteBlockModeFor(m_config)) {
            m_remoteBlocksSinceResume = 0;
            if (m_sessionTimer.isValid()) m_remoteCooldownUntilMs = m_sessionTimer.elapsed() + qMax(0, m_config.remoteWindowCooldownMs);
        }
        const int debounceMs = manualFlowMode ? 0 : qMax(0, m_config.resumeDebounceMs);
        if (debounceMs > 0 && !m_receiveMode) {
            m_resumeDebounceTimer.start(debounceMs);
            trace("flow", "Debounce de retomada iniciado", {{"resumeDebounceMs", debounceMs}});
        } else {
            tryEnterReadyOrSending();
            if (manualFlowMode) pumpTx();
        }
    } else if (m_holdStartedMs < 0) {
        m_resumeDebounceTimer.stop();
        m_holdStartedMs = m_sessionTimer.elapsed();
    }
}

void DncEngine::onWatchdogHoldTriggered() {
    if (!m_started || m_receiveMode || m_drain->isActive() || !useNoConsumeWatchdogFor(m_config)) return;
    m_flow->onWatchdogHold(true);
    ++m_stats.holdNoConsumeCount;
    m_scheduler->onHoldEntered();
    trace("watchdog", "Hold por ausência de avanço local", {{"holdMs", m_config.noConsumeHoldMs}, {"bytesToWrite", m_serial->bytesToWrite()}});
}
void DncEngine::onWatchdogHoldReleased() {
    if (!m_started || m_receiveMode) return;
    m_flow->onWatchdogHold(false);
    trace("watchdog", "Hold liberado", {{"bytesToWrite", m_serial->bytesToWrite()}});
}
void DncEngine::onResumeDebounceElapsed() { tryEnterReadyOrSending(); }

void DncEngine::refreshStateFromHoldReason(HoldReason reason) {
    if (!m_started || m_receiveMode) return;
    if (m_drain->isActive() || m_sm->state() == EngineState::Draining ||
        m_sm->state() == EngineState::Completed || m_sm->state() == EngineState::Aborted ||
        m_sm->state() == EngineState::Fault) {
        return;
    }

    switch (reason) {
    case HoldReason::None:
        if (isSendHoldState(m_sm->state()) || m_sm->state() == EngineState::WaitMachineReady || m_sm->state() == EngineState::Ready) {
            m_sm->transitionTo(EngineState::Ready, "Envio liberado.");
        }
        break;
    case HoldReason::OperatorPause:
        if (m_sm->state() != EngineState::HoldOperator) m_sm->transitionTo(EngineState::HoldOperator, "Pausa manual.");
        break;
    case HoldReason::Xoff:
        if (m_sm->state() != EngineState::HoldXoff) m_sm->transitionTo(EngineState::HoldXoff, "Recebido XOFF.");
        break;
    case HoldReason::CtsLow:
        if (m_sm->state() != EngineState::HoldCtsLow) m_sm->transitionTo(EngineState::HoldCtsLow, "CTS em nível baixo.");
        break;
    case HoldReason::NoConsume:
        if (m_sm->state() != EngineState::HoldNoConsume) m_sm->transitionTo(EngineState::HoldNoConsume, "Watchdog sem avanço.");
        break;
    default:
        break;
    }
}

bool DncEngine::isSendHoldState(EngineState state) const {
    return state == EngineState::HoldXoff ||
           state == EngineState::HoldCtsLow ||
           state == EngineState::HoldNoConsume ||
           state == EngineState::HoldOperator;
}

void DncEngine::tryEnterReadyOrSending() {
    if (!m_started || !m_flow->sendAllowed() || m_receiveMode || m_drain->isActive()) return;
    if (m_sm->state() == EngineState::WaitMachineReady || m_sm->state() == EngineState::Ready || isSendHoldState(m_sm->state())) {
        m_readyTimeoutTimer.stop();
        m_inactivityWarningActive = false;
        m_sm->transitionTo(EngineState::Sending, "Início do envio.");
        restartInactivityTimer();
        trace("tx", "Bomba de envio armada", {{"remaining", m_source ? m_source->remaining() : 0}, {"bytesToWrite", m_serial->bytesToWrite()}});
    }
}

void DncEngine::pumpTx() {
    if (!m_started || m_receiveMode) return;
    if (drainSerialRxImmediate() && (!m_flow->sendAllowed())) return;
    if (!m_flow->sendAllowed()) {
        trace("flow", "Envio bloqueado", {{"reason", toString(m_flow->currentHoldReason())}, {"bytesToWrite", m_serial->bytesToWrite()}, {"chunk", m_scheduler->currentChunkSize()}});
        return;
    }

    const bool manualFlowMode = manualFlowModeFor(m_config);
    const bool strictRateMode = strictRateModeFor(m_config);
    const qint64 nowMs = m_sessionTimer.isValid() ? m_sessionTimer.elapsed() : 0;
    if ((manualFlowMode || strictRateMode) && m_lastRxProcessedMs >= 0) {
        const qint64 rxQuietMs = (m_config.interpretXonXoff ? 2 : 1);
        if ((nowMs - m_lastRxProcessedMs) < rxQuietMs) {
            updateRuntimeStats();
            return;
        }
    }
    if (strictRateMode && m_config.manualSendRateLimitBps > 0) {
        const qint64 refillBase = (m_rateLastRefillMs >= 0) ? m_rateLastRefillMs : nowMs;
        const qint64 elapsed = std::max<qint64>(0, nowMs - refillBase);
        const double refill = (static_cast<double>(m_config.manualSendRateLimitBps) * static_cast<double>(elapsed)) / 1000.0;
        const double bucketCap = std::max<double>(1.0, strictRateBurstBytesFor(m_config));
        m_rateBucketTokens = std::min(bucketCap, m_rateBucketTokens + refill);
        m_rateLastRefillMs = nowMs;
        if (m_rateBucketTokens < 1.0) {
            if (m_rateLimitLogUntilMs < 0 || nowMs >= m_rateLimitLogUntilMs) {
                trace("flow", "Limitador manual de B/s ativo", {{"limitBps", m_config.manualSendRateLimitBps}, {"tokens", m_rateBucketTokens}});
                m_rateLimitLogUntilMs = nowMs + 250;
            }
            updateRuntimeStats();
            return;
        }
    }
    if (manualFlowMode || strictRateMode) {
        if (!m_manualStartPrimed) {
            m_manualStartPrimed = true;
            if (m_manualStartGateUntilMs < 0) m_manualStartGateUntilMs = nowMs + manualStartGateMsFor(m_config);
            trace("flow", "Rampa inicial aguardando janela segura", {{"untilMs", m_manualStartGateUntilMs}});
            updateRuntimeStats();
            return;
        }
        if (m_manualStartGateUntilMs >= 0 && nowMs < m_manualStartGateUntilMs) {
            updateRuntimeStats();
            return;
        }
        if (m_manualObserveUntilMs >= 0 && nowMs < m_manualObserveUntilMs) {
            updateRuntimeStats();
            return;
        }
        if (m_manualRecoveryUntilMs >= 0 && nowMs < m_manualRecoveryUntilMs) {
            updateRuntimeStats();
            return;
        }
        const int localBuffered = static_cast<int>(m_serial->bytesToWrite());
        const int driverBacklog = static_cast<int>(std::max<qint64>(m_serial->bytesToWrite(), std::max<qint64>(0, m_stats.bytesQueued - m_stats.bytesAcceptedByPort)));
        const int localHighWater = (manualFlowMode || strictRateMode) ? manualLocalHighWaterFor(m_config, m_scheduler->currentChunkSize())
                                                                      : std::max(m_config.minChunkBytes,
                                                                                 std::min(m_config.writeBufferLimit,
                                                                                          std::max(24, std::min(m_scheduler->currentChunkSize() * 2,
                                                                                                                m_config.maxChunkBytes + m_config.minChunkBytes))));
        if (localBuffered >= localHighWater || driverBacklog >= localHighWater) {
            trace("flow", "Pausa por janela local cheia", {{"bytesToWrite", localBuffered}, {"driverBacklog", driverBacklog}, {"localHighWater", localHighWater}});
            updateRuntimeStats();
            return;
        }
        if (remoteBlockModeFor(m_config) && m_remoteCooldownUntilMs >= 0 && nowMs < m_remoteCooldownUntilMs) {
            trace("flow", "Cooldown de janela de blocos ativo", {{"untilMs", m_remoteCooldownUntilMs}, {"sessionElapsedMs", nowMs}});
            updateRuntimeStats();
            return;
        }
        if (m_lastManualWriteMs >= 0) {
            const int interChunkGapMs = manualInterByteGapMsFor(m_config, m_manualFirstByteSent);
            if ((nowMs - m_lastManualWriteMs) < interChunkGapMs) {
                updateRuntimeStats();
                return;
            }
        }
    }

    if (m_drain->isActive()) {
        updateRuntimeStats();
        if (m_sendAwaitingOperatorFinish) {
            return;
        }

        const qint64 bytesToWriteNow = m_serial->bytesToWrite();
        const bool sendAllowedNow = m_flow->sendAllowed();
        const bool allAcceptedByPort = (m_stats.bytesAcceptedByPort >= m_stats.fileBytesTotal);

        if (allAcceptedByPort && m_drain->canFinish(bytesToWriteNow, sendAllowedNow)) {
            if (!m_sendAwaitingOperatorFinish) {
                m_sendAwaitingOperatorFinish = true;
                m_overallTimeoutTimer.stop();
                m_sm->transitionTo(EngineState::AwaitOperatorFinish, "Programa transferido para a porta. Aguardando confirmação do operador para fechar a comunicação.");
                trace("drain", "Transferência para a porta concluída; aguardando confirmação do operador", {{"bytesAcceptedByPort", m_stats.bytesAcceptedByPort}, {"fileBytesTotal", m_stats.fileBytesTotal}, {"bytesToWrite", bytesToWriteNow}, {"finishThresholdBytes", m_drain->finishThresholdBytes()}, {"quietFinishWindowMs", m_drain->quietFinishWindowMs()}});
                emit logLine("Fim do programa atingido. Porta mantida aberta aguardando autorização do operador para fechar.");
                emit sendReadyToFinalize("Programa transferido por completo para a porta serial. Confirme o encerramento somente quando a máquina tiver concluído a execução.");
            }
            return;
        }

        if (m_drain->isTimedOut(bytesToWriteNow, sendAllowedNow)) {
            // A drenagem estourar o orcamento NAO prova que a CNC terminou.
            // Fechar a porta aqui pode cortar a execucao no meio. Vira alerta
            // operacional; quem decide fechar e o operador.
            if (!m_drainTimeoutWarningActive) {
                m_drainTimeoutWarningActive = true;
                m_inactivityWarningActive = true;
                m_overallTimeoutTimer.stop();

                trace("drain", "Timeout de drenagem convertido em alerta operacional sem fechamento automático",
                      {{"bytesToWrite", bytesToWriteNow},
                       {"drainBudgetMs", m_drain->timeoutBudgetMs()},
                       {"finishThresholdBytes", m_drain->finishThresholdBytes()},
                       {"sendAllowed", sendAllowedNow},
                       {"bytesAcceptedByPort", m_stats.bytesAcceptedByPort},
                       {"fileBytesTotal", m_stats.fileBytesTotal}});

                emit logLine(QString("Alerta: drenagem final demorou. Porta mantida aberta. Pendente local=%1 bytes.")
                                 .arg(bytesToWriteNow));

                emit inactivityWarning(
                    QString("A drenagem final passou do tempo previsto, mas isso NÃO confirma que o programa terminou na CNC.")
                    + "\n\n"
                    + "A porta continua aberta. Isso pode acontecer em usinagem lenta, CNC parado em M00/M01, buffer cheio, XOFF ativo ou CTS bloqueado.");
            }
            return;
        }
        return;
    }

    if (!m_source->hasRemaining()) {
        trace("file", "EOF atingido; iniciando drenagem", {{"bytesQueued", m_stats.bytesQueued}, {"bytesAcceptedByPort", m_stats.bytesAcceptedByPort}});
        beginDrainingIfNeeded();
        return;
    }

    QByteArray chunk;
    if (remoteBlockModeFor(m_config)) {
        if (m_remoteBlocksSinceResume >= m_remoteWindowBlocks) {
            if (m_sessionTimer.isValid()) m_remoteCooldownUntilMs = m_sessionTimer.elapsed() + qMax(0, m_config.remoteWindowCooldownMs);
            updateRuntimeStats();
            return;
        }
        chunk = m_source->peekNextBlock(512);
        if (!m_manualFirstByteSent && !chunk.isEmpty()) chunk = chunk.left(1);
    } else {
        int chunkSize = m_scheduler->nextChunkSize(m_source->remaining(), m_serial->bytesToWrite());
        if (strictRateMode) {
            chunkSize = std::min(chunkSize, std::max(1, static_cast<int>(m_rateBucketTokens)));
            chunkSize = std::min(chunkSize, strictRateBurstBytesFor(m_config));
        }
        if ((manualFlowMode || strictRateMode) && !m_manualFirstByteSent) {
            chunkSize = std::min(chunkSize, 16);
        } else if (strictRateMode && m_manualRecoveryChunksRemaining > 0) {
            chunkSize = std::min(chunkSize, std::max(16, strictRateBurstBytesFor(m_config) / 2));
        } else if (manualFlowMode && m_manualRecoveryChunksRemaining > 0) {
            chunkSize = std::min(chunkSize, std::max(16, m_scheduler->currentChunkSize() / 2));
        }
        if (chunkSize <= 0) return;
        chunk = m_source->peekChunk(chunkSize);
    }
    if (chunk.isEmpty()) {
        trace("tx", "Fila vazia sem chunk disponível", {{"remaining", m_source->remaining()}, {"bytesToWrite", m_serial->bytesToWrite()}, {"remoteBlockMode", remoteBlockModeFor(m_config)}});
        beginDrainingIfNeeded();
        return;
    }

    trace("tx", "Solicitação de escrita", {{"requested", static_cast<qint64>(chunk.size())}, {"bytesToWriteBefore", m_serial->bytesToWrite()}, {"manualFlowMode", manualFlowMode}});
    const qint64 accepted = m_serial->writeBytes(chunk);
    if (accepted < 0) {
        finishSession(SessionEndReason::PortError, m_serial->errorString());
        return;
    }

    if (accepted > 0) {
        const bool partialWrite = (accepted < chunk.size());
        if (partialWrite) {
            trace("tx", "Escrita parcial detectada", {{"accepted", accepted}, {"requested", chunk.size()}});
        }
        m_source->commit(accepted);
        m_stats.bytesQueued += accepted;
        if (m_config.manualSendRateLimitBps > 0) {
            m_rateBucketTokens = std::max(0.0, m_rateBucketTokens - static_cast<double>(accepted));
        }
        if ((manualFlowMode || strictRateMode) && m_sessionTimer.isValid()) {
            m_lastManualWriteMs = m_sessionTimer.elapsed();
            if (!m_manualFirstByteSent) {
                m_manualFirstByteSent = true;
                m_manualObserveUntilMs = m_lastManualWriteMs + manualFirstByteObserveMsFor(m_config);
                trace("flow", "Primeiro burst enviado", {{"observeUntilMs", m_manualObserveUntilMs}, {"nextChunkTarget", m_scheduler->currentChunkSize()}, {"accepted", accepted}});
                if (m_manualRecoveryChunksRemaining > 0) --m_manualRecoveryChunksRemaining;
                m_manualRecoveryUntilMs = -1;
            } else if (manualFlowMode && m_manualRecoveryChunksRemaining > 0) {
                --m_manualRecoveryChunksRemaining;
                if (m_manualRecoveryChunksRemaining <= 0) m_manualRecoveryUntilMs = -1;
            } else if (remoteBlockModeFor(m_config)) {
                if (!partialWrite) {
                    m_remoteBlocksSinceResume++;
                    trace("flow", "Bloco NC concluído na janela", {{"windowBlocks", m_remoteWindowBlocks}, {"blocksSinceResume", m_remoteBlocksSinceResume}, {"bytes", accepted}});
                } else {
                    trace("flow", "Bloco NC parcialmente escrito; janela preservada", {{"windowBlocks", m_remoteWindowBlocks}, {"blocksSinceResume", m_remoteBlocksSinceResume}, {"accepted", accepted}, {"requested", chunk.size()}});
                }
            }
        }
        m_scheduler->onWriteAccepted(accepted);
        updateRuntimeStats();
        emit statsUpdated(m_stats);
        emit progressUpdated(m_stats.bytesAcceptedByPort, m_stats.fileBytesTotal);
    } else {
        trace("tx", "Nenhum byte aceito pela porta", {{"requested", static_cast<qint64>(chunk.size())}, {"bytesToWrite", m_serial->bytesToWrite()}});
    }

    if (!m_source->hasRemaining()) beginDrainingIfNeeded();
}

void DncEngine::beginDrainingIfNeeded() {
    if (!m_drain->isActive()) {
        m_watchdog->stop();
        m_flow->onWatchdogHold(false);
        m_sm->transitionTo(EngineState::Draining, "Aguardando drenagem final.");
        const qint64 pendingBytes = std::max<qint64>(0, m_stats.bytesQueued - m_stats.bytesAcceptedByPort);
        m_drain->begin(pendingBytes, m_stats.effectiveThroughputBps, m_stats.wireTheoreticalBps);
        trace("drain", "Drenagem iniciada", {{"bytesAcceptedByPort", m_stats.bytesAcceptedByPort}, {"fileBytesTotal", m_stats.fileBytesTotal}, {"pendingBytes", pendingBytes}, {"drainBudgetMs", m_drain->timeoutBudgetMs()}});
    }
}

void DncEngine::updateRuntimeStats() {
    if (!m_sessionTimer.isValid()) return;
    m_stats.sessionElapsedMs = m_sessionTimer.elapsed();
    if (m_sm->state() == EngineState::Sending) {
        m_stats.sendingTimeMs = m_stats.sessionElapsedMs - m_stats.holdTimeMs - m_stats.drainingTimeMs;
    }
    if (m_sm->state() == EngineState::Draining) {
        m_stats.drainingTimeMs = m_drain->isActive() ? std::max<qint64>(0, m_stats.sessionElapsedMs - m_stats.sendingTimeMs - m_stats.holdTimeMs) : m_stats.drainingTimeMs;
    }
    m_stats.pendingBytes = std::max<qint64>(0, m_stats.bytesQueued - m_stats.bytesAcceptedByPort);
    m_stats.currentChunkBytes = m_scheduler->currentChunkSize();
    m_stats.minChunkBytesObserved = m_scheduler->minChunkObserved();
    m_stats.maxChunkBytesObserved = m_scheduler->maxChunkObserved();
    m_stats.chunkUpshiftCount = m_scheduler->upshiftCount();
    m_stats.chunkDownshiftCount = m_scheduler->downshiftCount();
    m_stats.bytesSinceResume = m_scheduler->bytesSinceResume();
    m_stats.wireTheoreticalBps = bytesPerSecondOnWireFor(m_config);
    if (m_lastThroughputSampleMs < 0) {
        m_lastThroughputSampleMs = m_stats.sessionElapsedMs;
        m_lastThroughputSampleBytes = m_stats.bytesAcceptedByPort;
    } else {
        const qint64 deltaMs = m_stats.sessionElapsedMs - m_lastThroughputSampleMs;
        if (deltaMs >= 200) {
            const qint64 deltaBytes = m_stats.bytesAcceptedByPort - m_lastThroughputSampleBytes;
            m_stats.instantaneousThroughputBps = (deltaBytes * 1000.0) / std::max<qint64>(1, deltaMs);
            m_stats.peakThroughputBps = std::max(m_stats.peakThroughputBps, m_stats.instantaneousThroughputBps);
            m_lastThroughputSampleMs = m_stats.sessionElapsedMs;
            m_lastThroughputSampleBytes = m_stats.bytesAcceptedByPort;
        }
    }
    if (m_stats.sessionElapsedMs > 0) {
        m_stats.avgThroughputBps = (m_stats.bytesAcceptedByPort * 1000.0) / m_stats.sessionElapsedMs;
        const auto effectiveDen = std::max<qint64>(1, m_stats.sessionElapsedMs - m_stats.holdTimeMs);
        m_stats.effectiveThroughputBps = (m_stats.bytesAcceptedByPort * 1000.0) / effectiveDen;
        m_stats.estimatedConsumeBps = (m_stats.bytesReceived * 1000.0) / m_stats.sessionElapsedMs;
        if (m_stats.wireTheoreticalBps > 0.0) {
            m_stats.lineEfficiency = m_stats.avgThroughputBps / m_stats.wireTheoreticalBps;
        }
    }
    m_stats.congestionRatio = m_stats.bytesQueued > 0 ? static_cast<double>(m_stats.pendingBytes) / static_cast<double>(m_stats.bytesQueued) : 0.0;
}

void DncEngine::onReadyTimeout() {
    if (!m_started || m_receiveMode) return;
    if (m_sm->state() != EngineState::WaitMachineReady && m_sm->state() != EngineState::Ready) return;

    // A maquina nao liberar condicao de envio nao e motivo para fechar a porta
    // sozinho: pode ser CNC em M00/M01, buffer cheio, XOFF ou CTS baixo.
    if (m_inactivityWarningActive) return;
    m_inactivityWarningActive = true;
    m_readyTimeoutTimer.stop();

    const int waitedMs = qMax(0, m_config.readyTimeoutMs);
    trace("timeout", "Alerta de prontidão sem fechamento automático",
          {{"readyTimeoutMs", waitedMs},
           {"state", toString(m_sm->state())},
           {"sendAllowed", m_flow->sendAllowed()}});

    emit logLine(QString("Alerta: sem condição de envio há %1 s. Porta mantida aberta aguardando decisão do operador.")
                     .arg(waitedMs / 1000));

    emit inactivityWarning(
        QString("A porta está aberta, mas a máquina ficou %1 s sem liberar condição para envio. "
                "A comunicação NÃO será fechada automaticamente. Escolha se deseja manter a porta "
                "aberta ou encerrar a comunicação.")
            .arg(waitedMs / 1000));
}

void DncEngine::onOverallTimeout() {
    if (!m_started) return;
    if (!m_receiveMode && isActiveSendSessionState(m_sm->state())) {
        // Um alerta ja esta na tela do operador. Nao fecha a porta e nao
        // empilha um segundo dialogo: o QMessageBox roda um event loop
        // aninhado, entao este timer continua disparando enquanto ele esta
        // aberto. Quem sai deste estado e o operador, via
        // continueAfterInactivityWarning() ou abort().
        if (m_inactivityWarningActive) {
            trace("timeout", "Bloqueado fechamento automático após fim do programa",
                  {{"state", toString(m_sm->state())},
                   {"pendingBytes", m_stats.pendingBytes},
                   {"bytesToWrite", m_serial->bytesToWrite()}});
            return;
        }

        m_inactivityWarningActive = true;
        const int graceMs = inactivityGraceTimeoutMs();
        const QString detail = QString("A máquina ficou %1 s sem avanço visível de comunicação. A porta permanecerá aberta por enquanto. Se a usinagem ainda estiver processando, escolha continuar aguardando.")
                                   .arg(configuredInactivityTimeoutMs() / 1000);
        trace("timeout", "Alerta de inatividade durante envio", {{"configuredTimeoutMs", configuredInactivityTimeoutMs()}, {"graceMs", graceMs}, {"state", toString(m_sm->state())}, {"bytesAcceptedByPort", m_stats.bytesAcceptedByPort}, {"pendingBytes", m_stats.pendingBytes}, {"bytesToWrite", m_serial->bytesToWrite()}});
        emit logLine(QString("Alerta de inatividade: sem avanço de comunicação há %1 s. Porta mantida aberta aguardando decisão do operador.").arg(configuredInactivityTimeoutMs() / 1000));
        emit inactivityWarning(detail);
        // O timer continua armado de proposito: se o operador nao responder, o
        // proximo disparo cai no ramo acima e apenas REGISTRA que o fechamento
        // automatico foi bloqueado. Nunca fecha a porta sozinho.
        if (graceMs > 0) m_overallTimeoutTimer.start(graceMs);
        return;
    }

    finishSession(SessionEndReason::OverallTimeout, "Timeout geral da sessão.");
}

void DncEngine::saveHistory(SessionEndReason reason, const QString& detail) {
    SessionHistoryEntry entry;
    entry.startedAtUtc = m_sessionStartUtc;
    entry.finishedAtUtc = QDateTime::currentDateTimeUtc();
    entry.fileName = m_receiveMode ? QFileInfo(m_receiveOutputPath).fileName() : QFileInfo(m_filePath).fileName();
    entry.portName = m_config.portName;
    entry.endReason = reason;
    entry.detail = detail;
    entry.stats = m_stats;
    m_history->append(entry);
}

void DncEngine::finishSession(SessionEndReason reason, const QString& detail) {
    updateRuntimeStats();
    if (m_holdStartedMs >= 0 && m_sessionTimer.isValid()) {
        m_stats.holdTimeMs += (m_sessionTimer.elapsed() - m_holdStartedMs);
        m_holdStartedMs = -1;
    }
    m_txPumpTimer.stop();
    m_readyTimeoutTimer.stop();
    m_overallTimeoutTimer.stop();
    m_resumeDebounceTimer.stop();
    m_receiveIdleTimer.stop();
    m_watchdog->stop();
    m_sendAwaitingOperatorFinish = false;
    m_inactivityWarningActive = false;
    m_receiveIdleWarningActive = false;
    m_drainTimeoutWarningActive = false;
    m_drain->reset();
    m_handshake->stop();
    m_serial->close();
    const bool wasStarted = m_started;
    m_started = false;

    switch (reason) {
    case SessionEndReason::Success:
        m_sm->transitionTo(EngineState::Completed, detail);
        break;
    case SessionEndReason::OperatorAbort:
        m_sm->transitionTo(EngineState::Aborted, detail);
        break;
    default:
        m_sm->transitionTo(EngineState::Fault, detail);
        break;
    }

    trace("session", "Sessão finalizada", {{"reason", toString(reason)}, {"detail", detail}});
    if (wasStarted) {
        const auto tracePath = m_traceRepo->writeSessionTrace(m_sessionId, m_traceEvents);
        if (tracePath.isOk()) {
            m_traceRepo->exportTraceText(m_sessionId, m_traceEvents);
            m_stats.traceFilePath = tracePath.value();
        }
        saveHistory(reason, detail);
    }
    emit statsUpdated(m_stats);
    SessionResult result;
    result.reason = reason;
    result.mode = m_receiveMode ? SessionMode::Receive : SessionMode::Send;
    result.detail = detail;
    result.filePath = m_receiveMode ? m_receiveOutputPath : m_filePath;
    result.traceFilePath = m_stats.traceFilePath;
    emit sessionFinished(result);
}

void DncEngine::onReceiveIdleTimeout() {
    if (!m_started || !m_receiveMode || !m_config.receiveAutoFinishOnSilence) return;
    if (!m_receiveSawPayload) return;

    // Silencio na linha NAO prova que a maquina terminou de transmitir: em
    // usinagem lenta ela pode voltar a enviar. Fechar aqui truncaria o arquivo
    // recebido. Vira alerta; quem finaliza e o operador.
    if (m_receiveIdleWarningActive) return;
    m_receiveIdleWarningActive = true;
    m_receiveIdleTimer.stop();

    const int idleSec = qMax(0, m_config.receiveIdleFinishMs) / 1000;
    trace("receive", "Timeout de silêncio atingido; aguardando decisão do operador",
          {{"idleMs", m_config.receiveIdleFinishMs}, {"bytesReceived", m_stats.bytesReceived}});

    emit logLine(QString("Alerta: recepção sem novos dados há %1 s. Porta mantida aberta aguardando decisão do operador.")
                     .arg(idleSec));

    emit receiveIdleWarning(
        QString("A máquina ficou %1 s sem enviar novos dados. A porta ainda está aberta. "
                "Se a máquina ainda estiver transmitindo lentamente, mantenha a porta aberta; "
                "se a recepção terminou, finalize para fechar a comunicação.")
            .arg(idleSec));
}

} // namespace smi::dnc
