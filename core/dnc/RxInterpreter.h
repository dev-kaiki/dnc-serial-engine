#pragma once

#include <QObject>
#include "../common/DncTypes.h"
#include "../config/MachineConfig.h"

namespace smi::dnc {

class RxInterpreter : public QObject {
    Q_OBJECT
public:
    explicit RxInterpreter(QObject* parent = nullptr);
    void setConfig(const MachineConfig& config);
    void process(const QByteArray& data);

signals:
    void rxEvent(const smi::dnc::RxEvent& event);

private:
    MachineConfig m_config;
};

} // namespace smi::dnc
