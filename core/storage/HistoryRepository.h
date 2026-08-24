#pragma once

#include <QString>
#include <QDateTime>
#include <QList>
#include "../common/Result.h"
#include "../common/DncTypes.h"
#include "../dnc/SessionStats.h"

namespace smi::dnc {

struct SessionHistoryEntry {
    QDateTime startedAtUtc;
    QDateTime finishedAtUtc;
    QString fileName;
    QString portName;
    SessionEndReason endReason = SessionEndReason::None;
    QString detail;
    SessionStats stats;
};

class HistoryRepository {
public:
    explicit HistoryRepository(const QString& filePath = QString());

    Result<void> append(const SessionHistoryEntry& entry) const;
    Result<QList<SessionHistoryEntry>> loadAll() const;
    QString filePath() const;
    static QString defaultFilePath();

private:
    QString m_filePath;
};

} // namespace smi::dnc
