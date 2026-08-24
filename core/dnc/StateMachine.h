#pragma once

#include <QObject>
#include "../common/DncTypes.h"

namespace smi::dnc {

class StateMachine : public QObject {
    Q_OBJECT
public:
    explicit StateMachine(QObject* parent = nullptr);
    EngineState state() const;
    void transitionTo(EngineState next, const QString& reason);

signals:
    void stateChanged(const smi::dnc::StateTransition& transition);

private:
    EngineState m_state = EngineState::Idle;
};

} // namespace smi::dnc
