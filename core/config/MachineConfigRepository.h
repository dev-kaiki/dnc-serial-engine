#pragma once

#include "../common/Result.h"
#include "MachineConfig.h"

namespace smi::dnc {

class MachineConfigRepository {
public:
    explicit MachineConfigRepository(const QString& filePath = QString());

    Result<MachineConfig> load() const;
    Result<void> save(const MachineConfig& config) const;
    QString filePath() const;
    static QString defaultFilePath();

private:
    QString m_filePath;
};

} // namespace smi::dnc
