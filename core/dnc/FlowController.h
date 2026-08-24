#pragma once

#include <QObject>
#include "../common/DncTypes.h"
#include "../config/MachineConfig.h"

namespace smi::dnc {

class FlowController : public QObject {
    Q_OBJECT
public:
    explicit FlowController(QObject* parent = nullptr);
    void setConfig(const MachineConfig& config);
    void setOperatorPaused(bool paused);
    void onXon();
    void onXoff();
    void onCtsChanged(bool high);
    void onWatchdogHold(bool hold);
    void reset();

    bool sendAllowed() const;
    HoldReason currentHoldReason() const;

signals:
    void sendPermissionChanged(bool allowed, smi::dnc::HoldReason reason);

private:
    void recalc();

    MachineConfig m_config;
    bool m_operatorPaused = false;
    bool m_xoffActive = false;
    bool m_ctsHigh = true;
    bool m_watchdogHold = false;
    HoldReason m_reason = HoldReason::None;
    bool m_lastAllowed = false;
};

} // namespace smi::dnc
