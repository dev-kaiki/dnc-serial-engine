#pragma once

#include <QObject>
#include "../dnc/SessionStats.h"
#include "../common/DncTypes.h"

namespace smi::dnc {

class CommDiagnostics : public QObject {
    Q_OBJECT
public:
    explicit CommDiagnostics(QObject* parent = nullptr);
    QString summarize(const SessionStats& stats) const;
    QString summarizeSignals(const SerialSignalSnapshot& snapshot) const;
    QString healthGrade(const SessionStats& stats, EngineState state) const;
};

} // namespace smi::dnc
