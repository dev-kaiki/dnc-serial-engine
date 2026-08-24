#pragma once

#include <QDateTime>
#include <QByteArray>
#include <QString>
#include <QMetaType>

namespace smi::dnc {

enum class EngineState {
    Idle,
    Preparing,
    OpeningPort,
    WaitMachineReady,
    Ready,
    Sending,
    Receiving,
    HoldXoff,
    HoldCtsLow,
    HoldNoConsume,
    HoldOperator,
    Draining,
    AwaitOperatorFinish,
    Completed,
    Aborted,
    Fault
};

enum class HoldReason {
    None,
    OperatorPause,
    Xoff,
    CtsLow,
    NoConsume,
    ReadyGate,
    Drain,
    Fault
};

enum class SessionMode {
    Send,
    Receive
};

enum class SessionEndReason {
    None,
    Success,
    OperatorAbort,
    PortOpenFailed,
    PortError,
    ConfigurationInvalid,
    ReadyTimeout,
    DrainTimeout,
    OverallTimeout,
    SourceError,
    UnknownFault
};

enum class RxEventType {
    Data,
    Xon,
    Xoff,
    Text,
    Binary,
    Unknown
};

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error
};

struct SerialSignalSnapshot {
    bool cts = false;
    bool dsr = false;
    bool dcd = false;
    bool ri = false;
};

struct RxEvent {
    RxEventType type = RxEventType::Unknown;
    QByteArray raw;
    QString text;
    QDateTime timestampUtc;
};


struct SessionResult {
    SessionEndReason reason = SessionEndReason::None;
    SessionMode mode = SessionMode::Send;
    QString detail;
    QString filePath;
    QString traceFilePath;
};

inline QString toString(SessionMode mode) {
    switch (mode) {
    case SessionMode::Send: return "Send";
    case SessionMode::Receive: return "Receive";
    }
    return "Unknown";
}

struct StateTransition {
    EngineState from = EngineState::Idle;
    EngineState to = EngineState::Idle;
    QString reason;
    QDateTime timestampUtc;
};

inline QString toString(EngineState state) {
    switch (state) {
    case EngineState::Idle: return "Idle";
    case EngineState::Preparing: return "Preparing";
    case EngineState::OpeningPort: return "OpeningPort";
    case EngineState::WaitMachineReady: return "WaitMachineReady";
    case EngineState::Ready: return "Ready";
    case EngineState::Sending: return "Sending";
    case EngineState::Receiving: return "Receiving";
    case EngineState::HoldXoff: return "HoldXoff";
    case EngineState::HoldCtsLow: return "HoldCtsLow";
    case EngineState::HoldNoConsume: return "HoldNoConsume";
    case EngineState::HoldOperator: return "HoldOperator";
    case EngineState::Draining: return "Draining";
    case EngineState::AwaitOperatorFinish: return "AwaitOperatorFinish";
    case EngineState::Completed: return "Completed";
    case EngineState::Aborted: return "Aborted";
    case EngineState::Fault: return "Fault";
    }
    return "Unknown";
}

inline QString toString(HoldReason reason) {
    switch (reason) {
    case HoldReason::None: return "None";
    case HoldReason::OperatorPause: return "OperatorPause";
    case HoldReason::Xoff: return "Xoff";
    case HoldReason::CtsLow: return "CtsLow";
    case HoldReason::NoConsume: return "NoConsume";
    case HoldReason::ReadyGate: return "ReadyGate";
    case HoldReason::Drain: return "Drain";
    case HoldReason::Fault: return "Fault";
    }
    return "Unknown";
}

inline QString toString(SessionEndReason reason) {
    switch (reason) {
    case SessionEndReason::None: return "None";
    case SessionEndReason::Success: return "Success";
    case SessionEndReason::OperatorAbort: return "OperatorAbort";
    case SessionEndReason::PortOpenFailed: return "PortOpenFailed";
    case SessionEndReason::PortError: return "PortError";
    case SessionEndReason::ConfigurationInvalid: return "ConfigurationInvalid";
    case SessionEndReason::ReadyTimeout: return "ReadyTimeout";
    case SessionEndReason::DrainTimeout: return "DrainTimeout";
    case SessionEndReason::OverallTimeout: return "OverallTimeout";
    case SessionEndReason::SourceError: return "SourceError";
    case SessionEndReason::UnknownFault: return "UnknownFault";
    }
    return "Unknown";
}

} // namespace smi::dnc

Q_DECLARE_METATYPE(smi::dnc::SessionResult)
