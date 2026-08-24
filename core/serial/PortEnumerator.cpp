#include "PortEnumerator.h"
#include <QSerialPortInfo>

namespace smi::dnc {

QStringList PortEnumerator::listPortNames() {
    QStringList ports;
    const auto list = QSerialPortInfo::availablePorts();
    for (const auto& p : list) ports << p.portName();
    return ports;
}

} // namespace smi::dnc
