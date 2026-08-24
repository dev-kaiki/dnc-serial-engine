#include "ConfigurationValidator.h"

namespace smi::dnc {

Result<void> ConfigurationValidator::validate(const MachineConfig& config) {
    if (config.portName.trimmed().isEmpty() && !config.simulatorEnabled) {
        return Result<void>::fail("Porta serial não informada.");
    }
    if (config.minChunkBytes <= 0 || config.initialChunkBytes <= 0 || config.maxChunkBytes <= 0) {
        return Result<void>::fail("Chunk bytes deve ser maior que zero.");
    }
    if (config.minChunkBytes > config.initialChunkBytes) {
        return Result<void>::fail("minChunkBytes não pode ser maior que initialChunkBytes.");
    }
    if (config.initialChunkBytes > config.maxChunkBytes) {
        return Result<void>::fail("initialChunkBytes não pode ser maior que maxChunkBytes.");
    }
    if (config.requireXonToSend && !config.interpretXonXoff) {
        return Result<void>::fail("requireXonToSend exige interpretXonXoff habilitado.");
    }
    if (config.binarySafeMode && config.interpretXonXoff) {
        return Result<void>::fail("binarySafeMode não pode coexistir com interpretação de XON/XOFF.");
    }
    if (config.forceAscii7 && config.binarySafeMode) {
        return Result<void>::fail("forceAscii7 não pode coexistir com binarySafeMode.");
    }
    if (config.manualSendRateLimitBps < 0) {
        return Result<void>::fail("manualSendRateLimitBps não pode ser negativo.");
    }
    if (config.signalPollIntervalMs <= 0) {
        return Result<void>::fail("signalPollIntervalMs deve ser maior que zero.");
    }
    if (config.drainTimeoutMs <= 0) {
        return Result<void>::fail("drainTimeoutMs deve ser maior que zero.");
    }
    if (config.readyTimeoutMs < 0 || config.overallTimeoutMs < 0) {
        return Result<void>::fail("Timeouts não podem ser negativos.");
    }
    if (config.remoteBlockWindowMode) {
        if (config.remoteStartWindowBlocks < 1 || config.remoteTargetWindowBlocks < 1 || config.remoteMaxWindowBlocks < 1) {
            return Result<void>::fail("Janela remota de blocos inválida.");
        }
        if (!(config.remoteStartWindowBlocks <= config.remoteTargetWindowBlocks && config.remoteTargetWindowBlocks <= config.remoteMaxWindowBlocks)) {
            return Result<void>::fail("Janela remota deve obedecer start <= target <= max.");
        }
        if (config.remoteBlockGuardMs < 0 || config.remoteWindowCooldownMs < 0) {
            return Result<void>::fail("Tempos da janela remota inválidos.");
        }
    }
    if (config.normalizeLineEndings && config.lineEnding.isEmpty() && !config.binarySafeMode) {
        return Result<void>::fail("lineEnding não pode ser vazio no modo texto.");
    }
    if (config.receiveRequireStartDelimiter && config.receiveStartDelimiter.isEmpty()) {
        return Result<void>::fail("Recepção exige delimitador inicial, mas ele não foi configurado.");
    }
    if (config.receiveStopOnEndDelimiter && config.receiveEndDelimiter.isEmpty()) {
        return Result<void>::fail("Recepção exige delimitador final, mas ele não foi configurado.");
    }
    if (config.receiveAutoFinishOnSilence && config.receiveIdleFinishMs <= 0) {
        return Result<void>::fail("receiveIdleFinishMs deve ser maior que zero quando o auto-fim por silêncio estiver ativo.");
    }
    return Result<void>::ok();
}

} // namespace smi::dnc
