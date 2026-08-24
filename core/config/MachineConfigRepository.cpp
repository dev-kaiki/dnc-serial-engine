#include "MachineConfigRepository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace smi::dnc {

static QString hexString(const QByteArray& data) { return QString::fromLatin1(data.toHex()); }
static QByteArray fromHexString(const QJsonValue& v, const QByteArray& fallback = {}) {
    const auto s = v.toString();
    return s.isEmpty() ? fallback : QByteArray::fromHex(s.toLatin1());
}

static QJsonObject toJson(const MachineConfig& c) {
    QJsonObject o;
    o["machineName"] = c.machineName;
    o["portName"] = c.portName;
    o["baudRate"] = c.baudRate;
    o["dataBits"] = static_cast<int>(c.dataBits);
    o["parity"] = static_cast<int>(c.parity);
    o["stopBits"] = static_cast<int>(c.stopBits);
    o["flowControl"] = static_cast<int>(c.flowControl);
    o["dtrEnabled"] = c.dtrEnabled;
    o["rtsEnabled"] = c.rtsEnabled;
    o["monitorDsr"] = c.monitorDsr;
    o["signalPollIntervalMs"] = c.signalPollIntervalMs;
    o["requireCtsHighToSend"] = c.requireCtsHighToSend;
    o["requireXonToSend"] = c.requireXonToSend;
    o["interpretXonXoff"] = c.interpretXonXoff;
    o["binarySafeMode"] = c.binarySafeMode;
    o["readBufferLimit"] = c.readBufferLimit;
    o["writeBufferLimit"] = c.writeBufferLimit;
    o["manualSendRateLimitBps"] = c.manualSendRateLimitBps;
    o["initialChunkBytes"] = c.initialChunkBytes;
    o["minChunkBytes"] = c.minChunkBytes;
    o["maxChunkBytes"] = c.maxChunkBytes;
    o["noConsumeHoldMs"] = c.noConsumeHoldMs;
    o["resumeDebounceMs"] = c.resumeDebounceMs;
    o["drainTimeoutMs"] = c.drainTimeoutMs;
    o["readyTimeoutMs"] = c.readyTimeoutMs;
    o["overallTimeoutMs"] = c.overallTimeoutMs;
    o["remoteBlockWindowMode"] = c.remoteBlockWindowMode;
    o["remoteStartWindowBlocks"] = c.remoteStartWindowBlocks;
    o["remoteTargetWindowBlocks"] = c.remoteTargetWindowBlocks;
    o["remoteMaxWindowBlocks"] = c.remoteMaxWindowBlocks;
    o["remoteBlockGuardMs"] = c.remoteBlockGuardMs;
    o["remoteWindowCooldownMs"] = c.remoteWindowCooldownMs;
    o["normalizeLineEndings"] = c.normalizeLineEndings;
    o["trimTrailingSpaces"] = c.trimTrailingSpaces;
    o["lineEnding"] = hexString(c.lineEnding);
    o["stripUtf8Bom"] = c.stripUtf8Bom;
    o["forceAscii7"] = c.forceAscii7;
    o["ensureTrailingLineEnding"] = c.ensureTrailingLineEnding;
    o["autoWrapPercent"] = c.autoWrapPercent;
    o["sendPrefix"] = hexString(c.sendPrefix);
    o["sendSuffix"] = hexString(c.sendSuffix);
    o["appendEof"] = c.appendEof;
    o["eofSequence"] = hexString(c.eofSequence);
    o["enableReceiveMode"] = c.enableReceiveMode;
    o["defaultSendDirectory"] = c.defaultSendDirectory;
    o["defaultReceiveDirectory"] = c.defaultReceiveDirectory;
    o["receiveStartDelimiter"] = hexString(c.receiveStartDelimiter);
    o["receiveEndDelimiter"] = hexString(c.receiveEndDelimiter);
    o["receiveRequireStartDelimiter"] = c.receiveRequireStartDelimiter;
    o["receiveStopOnEndDelimiter"] = c.receiveStopOnEndDelimiter;
    o["receiveIncludeDelimitersInFile"] = c.receiveIncludeDelimitersInFile;
    o["receiveAutoFinishOnSilence"] = c.receiveAutoFinishOnSilence;
    o["receiveIdleFinishMs"] = c.receiveIdleFinishMs;
    o["sessionTraceDirectory"] = c.sessionTraceDirectory;
    o["exportDirectory"] = c.exportDirectory;
    o["simulatorEnabled"] = c.simulatorEnabled;
    o["simulatorXoffCycleBytes"] = c.simulatorXoffCycleBytes;
    o["simulatorResumeDelayMs"] = c.simulatorResumeDelayMs;
    return o;
}

static MachineConfig fromJson(const QJsonObject& o) {
    MachineConfig c;
    c.machineName = o["machineName"].toString();
    c.portName = o["portName"].toString();
    c.baudRate = o["baudRate"].toInt(9600);
    c.dataBits = static_cast<QSerialPort::DataBits>(o["dataBits"].toInt(static_cast<int>(QSerialPort::Data8)));
    c.parity = static_cast<QSerialPort::Parity>(o["parity"].toInt(static_cast<int>(QSerialPort::NoParity)));
    c.stopBits = static_cast<QSerialPort::StopBits>(o["stopBits"].toInt(static_cast<int>(QSerialPort::OneStop)));
    const auto savedFlowControl = static_cast<QSerialPort::FlowControl>(o["flowControl"].toInt(static_cast<int>(QSerialPort::NoFlowControl)));
    c.flowControl = savedFlowControl;
    c.dtrEnabled = o["dtrEnabled"].toBool(false);
    c.rtsEnabled = o["rtsEnabled"].toBool(false);
    c.monitorDsr = o["monitorDsr"].toBool(false);
    c.signalPollIntervalMs = o.contains("signalPollIntervalMs") ? o["signalPollIntervalMs"].toInt(2) : 2;
    c.requireCtsHighToSend = o["requireCtsHighToSend"].toBool(false);
    c.requireXonToSend = o["requireXonToSend"].toBool(false);
    c.interpretXonXoff = o["interpretXonXoff"].toBool(false);
    if (savedFlowControl == QSerialPort::SoftwareControl && !o.contains("interpretXonXoff")) {
        c.interpretXonXoff = false;
        c.requireCtsHighToSend = false;
    } else if (savedFlowControl == QSerialPort::HardwareControl && !o.contains("requireCtsHighToSend")) {
        c.requireCtsHighToSend = false;
        c.interpretXonXoff = false;
    }
    c.binarySafeMode = o["binarySafeMode"].toBool(false);
    c.readBufferLimit = o["readBufferLimit"].toInt(256);
    c.writeBufferLimit = o["writeBufferLimit"].toInt(256);
    c.manualSendRateLimitBps = o.contains("manualSendRateLimitBps") ? o["manualSendRateLimitBps"].toInt(0) : 0;
    c.initialChunkBytes = o["initialChunkBytes"].toInt(32);
    c.minChunkBytes = o["minChunkBytes"].toInt(8);
    c.maxChunkBytes = o["maxChunkBytes"].toInt(96);
    c.noConsumeHoldMs = o["noConsumeHoldMs"].toInt(250);
    c.resumeDebounceMs = o["resumeDebounceMs"].toInt(40);
    c.drainTimeoutMs = o["drainTimeoutMs"].toInt(3000);
    c.readyTimeoutMs = o["readyTimeoutMs"].toInt(4000);
    c.overallTimeoutMs = o["overallTimeoutMs"].toInt(0);
    c.remoteBlockWindowMode = o.contains("remoteBlockWindowMode") ? o["remoteBlockWindowMode"].toBool(false) : false;
    c.remoteStartWindowBlocks = o["remoteStartWindowBlocks"].toInt(2);
    c.remoteTargetWindowBlocks = o["remoteTargetWindowBlocks"].toInt(6);
    c.remoteMaxWindowBlocks = o["remoteMaxWindowBlocks"].toInt(8);
    c.remoteBlockGuardMs = o["remoteBlockGuardMs"].toInt(3);
    c.remoteWindowCooldownMs = o["remoteWindowCooldownMs"].toInt(12);
    c.normalizeLineEndings = o["normalizeLineEndings"].toBool(true);
    c.trimTrailingSpaces = o["trimTrailingSpaces"].toBool(false);
    c.lineEnding = fromHexString(o["lineEnding"], QByteArray("\r"));
    c.appendEof = o["appendEof"].toBool(false);
    c.eofSequence = fromHexString(o["eofSequence"]);
    c.enableReceiveMode = o["enableReceiveMode"].toBool(true);
    c.defaultSendDirectory = o["defaultSendDirectory"].toString();
    c.defaultReceiveDirectory = o["defaultReceiveDirectory"].toString();
    c.receiveStartDelimiter = fromHexString(o["receiveStartDelimiter"]);
    c.receiveEndDelimiter = fromHexString(o["receiveEndDelimiter"]);
    c.receiveRequireStartDelimiter = o["receiveRequireStartDelimiter"].toBool(false);
    c.receiveStopOnEndDelimiter = o["receiveStopOnEndDelimiter"].toBool(false);
    c.receiveIncludeDelimitersInFile = o["receiveIncludeDelimitersInFile"].toBool(false);
    c.receiveAutoFinishOnSilence = o.contains("receiveAutoFinishOnSilence") ? o["receiveAutoFinishOnSilence"].toBool(true) : true;
    c.receiveIdleFinishMs = o["receiveIdleFinishMs"].toInt(1500);
    c.sessionTraceDirectory = o["sessionTraceDirectory"].toString();
    c.exportDirectory = o["exportDirectory"].toString();
    c.simulatorEnabled = o["simulatorEnabled"].toBool(false);
    c.simulatorXoffCycleBytes = o["simulatorXoffCycleBytes"].toInt(256);
    c.simulatorResumeDelayMs = o["simulatorResumeDelayMs"].toInt(120);
    return c;
}

QString MachineConfigRepository::defaultFilePath() {
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(baseDir);
    return QDir(baseDir).filePath("machine_config.json");
}

MachineConfigRepository::MachineConfigRepository(const QString& filePath)
    : m_filePath(filePath.isEmpty() ? defaultFilePath() : filePath) {}

Result<MachineConfig> MachineConfigRepository::load() const {
    QFile f(m_filePath);
    if (!f.exists()) return Result<MachineConfig>::fail("Arquivo de configuração não encontrado.");
    if (!f.open(QIODevice::ReadOnly)) return Result<MachineConfig>::fail("Falha ao abrir configuração.");
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return Result<MachineConfig>::fail("Configuração inválida.");
    return Result<MachineConfig>::ok(fromJson(doc.object()));
}

Result<void> MachineConfigRepository::save(const MachineConfig& config) const {
    QFileInfo fi(m_filePath);
    QDir().mkpath(fi.absolutePath());
    QFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return Result<void>::fail("Falha ao salvar configuração.");
    f.write(QJsonDocument(toJson(config)).toJson(QJsonDocument::Indented));
    return Result<void>::ok();
}

QString MachineConfigRepository::filePath() const { return m_filePath; }

} // namespace smi::dnc
