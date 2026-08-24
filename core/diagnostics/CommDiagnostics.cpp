#include "CommDiagnostics.h"

namespace smi::dnc {

CommDiagnostics::CommDiagnostics(QObject* parent) : QObject(parent) {}

QString CommDiagnostics::summarize(const SessionStats& stats) const {
    return QString("TX aceitos: %1 | RX bruto: %2 | RX útil: %3 | Pendente: %4 | XON: %5 | XOFF: %6 | Congestão: %7% | Efetivo: %8 B/s | Consumo estimado: %9 B/s")
        .arg(stats.bytesAcceptedByPort)
        .arg(stats.bytesReceived)
        .arg(stats.receivedFramedBytes)
        .arg(stats.pendingBytes)
        .arg(stats.xonCount)
        .arg(stats.xoffCount)
        .arg(QString::number(stats.congestionRatio * 100.0, 'f', 1))
        .arg(QString::number(stats.effectiveThroughputBps, 'f', 1))
        .arg(QString::number(stats.estimatedConsumeBps, 'f', 1));
}

QString CommDiagnostics::summarizeSignals(const SerialSignalSnapshot& snapshot) const {
    return QString("CTS=%1 | DSR=%2 | DCD=%3 | RI=%4")
        .arg(snapshot.cts ? "ON" : "OFF")
        .arg(snapshot.dsr ? "ON" : "OFF")
        .arg(snapshot.dcd ? "ON" : "OFF")
        .arg(snapshot.ri ? "ON" : "OFF");
}

QString CommDiagnostics::healthGrade(const SessionStats& stats, EngineState state) const {
    if (state == EngineState::Fault) return "Crítico";
    if (stats.congestionRatio > 0.35 || stats.holdTimeMs > 0) return "Atenção";
    if (stats.bytesAcceptedByPort > 0 || stats.receivedFramedBytes > 0) return "Estável";
    return "Sem atividade";
}

} // namespace smi::dnc
