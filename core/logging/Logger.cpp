#include "Logger.h"
#include <QDateTime>

namespace smi::dnc {

Logger::Logger(QObject* parent) : QObject(parent) {}

void Logger::log(LogLevel, const QString& category, const QString& message) {
    const QString line = QString("%1 | %2 | %3")
                             .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs))
                             .arg(category).arg(message);
    emit logLineReady(line);
}

} // namespace smi::dnc
