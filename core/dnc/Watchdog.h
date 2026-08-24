#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include "../config/MachineConfig.h"

namespace smi::dnc {

class Watchdog : public QObject {
    Q_OBJECT
public:
    explicit Watchdog(QObject* parent = nullptr);
    void setConfig(const MachineConfig& config);
    void start();
    void stop();
    void markForwardProgress();
    void clearHold();

signals:
    void holdTriggered();
    void holdReleased();

private slots:
    void check();

private:
    MachineConfig m_config;
    QTimer m_timer;
    QElapsedTimer m_progressTimer;
    bool m_hold = false;
};

} // namespace smi::dnc
