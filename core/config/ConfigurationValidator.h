#pragma once

#include "../common/Result.h"
#include "MachineConfig.h"

namespace smi::dnc {

class ConfigurationValidator {
public:
    static Result<void> validate(const MachineConfig& config);
};

} // namespace smi::dnc
