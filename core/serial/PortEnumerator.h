#pragma once

#include <QStringList>

namespace smi::dnc {

class PortEnumerator {
public:
    static QStringList listPortNames();
};

} // namespace smi::dnc
