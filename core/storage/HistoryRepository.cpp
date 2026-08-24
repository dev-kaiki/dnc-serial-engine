#include "HistoryRepository.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace smi::dnc {

static QJsonObject statsToJson(const SessionStats& s) {
    QJsonObject o;
    o["fileBytesTotal"] = QString::number(s.fileBytesTotal);
    o["bytesQueued"] = QString::number(s.bytesQueued);
    o["bytesAcceptedByPort"] = QString::number(s.bytesAcceptedByPort);
    o["bytesReceived"] = QString::number(s.bytesReceived);
    o["pendingBytes"] = QString::number(s.pendingBytes);
    o["receivedFramedBytes"] = QString::number(s.receivedFramedBytes);
    o["receiveFramesCompleted"] = s.receiveFramesCompleted;
    o["xonCount"] = s.xonCount;
    o["xoffCount"] = s.xoffCount;
    o["ctsDropCount"] = s.ctsDropCount;
    o["ctsRiseCount"] = s.ctsRiseCount;
    o["holdTimeMs"] = QString::number(s.holdTimeMs);
    o["sendingTimeMs"] = QString::number(s.sendingTimeMs);
    o["drainingTimeMs"] = QString::number(s.drainingTimeMs);
    o["sessionElapsedMs"] = QString::number(s.sessionElapsedMs);
    o["avgThroughputBps"] = s.avgThroughputBps;
    o["effectiveThroughputBps"] = s.effectiveThroughputBps;
    o["estimatedConsumeBps"] = s.estimatedConsumeBps;
    o["congestionRatio"] = s.congestionRatio;
    o["lastFileName"] = s.lastFileName;
    o["lastPortName"] = s.lastPortName;
    o["traceFilePath"] = s.traceFilePath;
    return o;
}

static SessionStats statsFromJson(const QJsonObject& o) {
    SessionStats s;
    s.fileBytesTotal = o["fileBytesTotal"].toString().toLongLong();
    s.bytesQueued = o["bytesQueued"].toString().toLongLong();
    s.bytesAcceptedByPort = o["bytesAcceptedByPort"].toString().toLongLong();
    s.bytesReceived = o["bytesReceived"].toString().toLongLong();
    s.pendingBytes = o["pendingBytes"].toString().toLongLong();
    s.receivedFramedBytes = o["receivedFramedBytes"].toString().toLongLong();
    s.receiveFramesCompleted = o["receiveFramesCompleted"].toInt();
    s.xonCount = o["xonCount"].toInt();
    s.xoffCount = o["xoffCount"].toInt();
    s.ctsDropCount = o["ctsDropCount"].toInt();
    s.ctsRiseCount = o["ctsRiseCount"].toInt();
    s.holdTimeMs = o["holdTimeMs"].toString().toLongLong();
    s.sendingTimeMs = o["sendingTimeMs"].toString().toLongLong();
    s.drainingTimeMs = o["drainingTimeMs"].toString().toLongLong();
    s.sessionElapsedMs = o["sessionElapsedMs"].toString().toLongLong();
    s.avgThroughputBps = o["avgThroughputBps"].toDouble();
    s.effectiveThroughputBps = o["effectiveThroughputBps"].toDouble();
    s.estimatedConsumeBps = o["estimatedConsumeBps"].toDouble();
    s.congestionRatio = o["congestionRatio"].toDouble();
    s.lastFileName = o["lastFileName"].toString();
    s.lastPortName = o["lastPortName"].toString();
    s.traceFilePath = o["traceFilePath"].toString();
    return s;
}

static QJsonObject entryToJson(const SessionHistoryEntry& e) {
    QJsonObject o;
    o["startedAtUtc"] = e.startedAtUtc.toString(Qt::ISODateWithMs);
    o["finishedAtUtc"] = e.finishedAtUtc.toString(Qt::ISODateWithMs);
    o["fileName"] = e.fileName;
    o["portName"] = e.portName;
    o["endReason"] = toString(e.endReason);
    o["detail"] = e.detail;
    o["stats"] = statsToJson(e.stats);
    return o;
}

static SessionEndReason endReasonFromString(const QString& s) {
    for (SessionEndReason r : {SessionEndReason::None, SessionEndReason::Success, SessionEndReason::OperatorAbort,
                               SessionEndReason::PortOpenFailed, SessionEndReason::PortError, SessionEndReason::ConfigurationInvalid,
                               SessionEndReason::ReadyTimeout, SessionEndReason::DrainTimeout, SessionEndReason::OverallTimeout,
                               SessionEndReason::SourceError, SessionEndReason::UnknownFault}) {
        if (toString(r) == s) return r;
    }
    return SessionEndReason::UnknownFault;
}

static SessionHistoryEntry entryFromJson(const QJsonObject& o) {
    SessionHistoryEntry e;
    e.startedAtUtc = QDateTime::fromString(o["startedAtUtc"].toString(), Qt::ISODateWithMs);
    e.finishedAtUtc = QDateTime::fromString(o["finishedAtUtc"].toString(), Qt::ISODateWithMs);
    e.fileName = o["fileName"].toString();
    e.portName = o["portName"].toString();
    e.endReason = endReasonFromString(o["endReason"].toString());
    e.detail = o["detail"].toString();
    e.stats = statsFromJson(o["stats"].toObject());
    return e;
}

QString HistoryRepository::defaultFilePath() {
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(baseDir);
    return QDir(baseDir).filePath("history.json");
}

HistoryRepository::HistoryRepository(const QString& filePath)
    : m_filePath(filePath.isEmpty() ? defaultFilePath() : filePath) {}

Result<void> HistoryRepository::append(const SessionHistoryEntry& entry) const {
    QFileInfo fi(m_filePath);
    QDir().mkpath(fi.absolutePath());
    QJsonArray arr;
    QFile rf(m_filePath);
    if (rf.exists() && rf.open(QIODevice::ReadOnly)) {
        const auto doc = QJsonDocument::fromJson(rf.readAll());
        if (doc.isArray()) arr = doc.array();
    }
    arr.append(entryToJson(entry));
    QFile wf(m_filePath);
    if (!wf.open(QIODevice::WriteOnly | QIODevice::Truncate)) return Result<void>::fail("Falha ao gravar histórico.");
    wf.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    return Result<void>::ok();
}

Result<QList<SessionHistoryEntry>> HistoryRepository::loadAll() const {
    QList<SessionHistoryEntry> out;
    QFile f(m_filePath);
    if (!f.exists()) return Result<QList<SessionHistoryEntry>>::ok(out);
    if (!f.open(QIODevice::ReadOnly)) return Result<QList<SessionHistoryEntry>>::fail("Falha ao abrir histórico.");
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) return Result<QList<SessionHistoryEntry>>::fail("Histórico inválido.");
    for (const auto& v : doc.array()) if (v.isObject()) out.append(entryFromJson(v.toObject()));
    return Result<QList<SessionHistoryEntry>>::ok(out);
}

QString HistoryRepository::filePath() const { return m_filePath; }

} // namespace smi::dnc
