#pragma once

#include <QObject>
#include "../common/DncTypes.h"

namespace smi::dnc {

class Logger : public QObject {
    Q_OBJECT
public:
    explicit Logger(QObject* parent = nullptr);
    void log(LogLevel level, const QString& category, const QString& message);

signals:
    void logLineReady(const QString& line);
};

} // namespace smi::dnc
