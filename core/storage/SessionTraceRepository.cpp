#include "SessionTraceRepository.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

namespace smi::dnc {

static QJsonObject eventToJson(const SessionTraceEvent& e) {
    QJsonObject o;
    o["timestampUtc"] = e.timestampUtc.toString(Qt::ISODateWithMs);
    o["category"] = e.category;
    o["message"] = e.message;
    o["context"] = e.context;
    return o;
}

SessionTraceRepository::SessionTraceRepository(const QString& baseDirectory)
    : m_baseDirectory(baseDirectory) {}

QString SessionTraceRepository::defaultBaseDirectory() const {
    const auto base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base);
    return QDir(base).filePath("session_traces");
}

Result<QString> SessionTraceRepository::writeSessionTrace(const QString& sessionId, const QList<SessionTraceEvent>& events) const {
    const QString base = m_baseDirectory.isEmpty() ? defaultBaseDirectory() : m_baseDirectory;
    QDir().mkpath(base);
    const QString path = QDir(base).filePath(sessionId + ".json");
    QJsonArray arr;
    for (const auto& e : events) arr.append(eventToJson(e));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return Result<QString>::fail("Falha ao gravar trace da sessão.");
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    return Result<QString>::ok(path);
}

Result<QString> SessionTraceRepository::exportTraceText(const QString& sessionId, const QList<SessionTraceEvent>& events) const {
    const QString base = m_baseDirectory.isEmpty() ? defaultBaseDirectory() : m_baseDirectory;
    QDir().mkpath(base);
    const QString path = QDir(base).filePath(sessionId + ".log");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return Result<QString>::fail("Falha ao exportar trace em texto.");
    for (const auto& e : events) {
        const auto ctx = QJsonDocument(e.context).toJson(QJsonDocument::Compact);
        f.write(QString("%1 | %2 | %3 | %4\n")
                    .arg(e.timestampUtc.toString(Qt::ISODateWithMs), e.category, e.message, QString::fromUtf8(ctx))
                    .toUtf8());
    }
    return Result<QString>::ok(path);
}

} // namespace smi::dnc
