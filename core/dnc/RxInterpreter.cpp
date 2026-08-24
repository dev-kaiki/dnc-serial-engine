#include "RxInterpreter.h"
#include <QDateTime>

namespace smi::dnc {

RxInterpreter::RxInterpreter(QObject* parent) : QObject(parent) {}
void RxInterpreter::setConfig(const MachineConfig& config) { m_config = config; }

void RxInterpreter::process(const QByteArray& data) {
    for (char ch : data) {
        RxEvent ev;
        ev.raw = QByteArray(1, ch);
        ev.timestampUtc = QDateTime::currentDateTimeUtc();
        const unsigned char b = static_cast<unsigned char>(ch);

        if (m_config.interpretXonXoff && b == 0x11) {
            ev.type = RxEventType::Xon;
            emit rxEvent(ev);
            continue;
        }
        if (m_config.interpretXonXoff && b == 0x13) {
            ev.type = RxEventType::Xoff;
            emit rxEvent(ev);
            continue;
        }
        if (!m_config.binarySafeMode && b >= 32 && b <= 126) {
            ev.type = RxEventType::Text;
            ev.text = QString(QChar(static_cast<char>(b)));
        } else {
            ev.type = RxEventType::Data;
        }
        emit rxEvent(ev);
    }
}

} // namespace smi::dnc
