#include "StateMachine.h"
#include <QDateTime>

namespace smi::dnc {

StateMachine::StateMachine(QObject* parent) : QObject(parent) {}
EngineState StateMachine::state() const { return m_state; }

void StateMachine::transitionTo(EngineState next, const QString& reason) {
    if (m_state == next) return;
    StateTransition tr;
    tr.from = m_state;
    tr.to = next;
    tr.reason = reason;
    tr.timestampUtc = QDateTime::currentDateTimeUtc();
    m_state = next;
    emit stateChanged(tr);
}

} // namespace smi::dnc
