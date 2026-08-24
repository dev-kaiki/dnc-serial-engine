#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include "../common/Result.h"

namespace smi::dnc {

struct SessionTraceEvent {
    QDateTime timestampUtc;
    QString category;
    QString message;
    QJsonObject context;
};

class SessionTraceRepository {
public:
    explicit SessionTraceRepository(const QString& baseDirectory = QString());

    QString defaultBaseDirectory() const;
    Result<QString> writeSessionTrace(const QString& sessionId, const QList<SessionTraceEvent>& events) const;
    Result<QString> exportTraceText(const QString& sessionId, const QList<SessionTraceEvent>& events) const;

private:
    QString m_baseDirectory;
};

} // namespace smi::dnc
