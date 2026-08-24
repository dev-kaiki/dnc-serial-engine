#pragma once

#include <QString>
#include <QByteArray>
#include "../common/Result.h"
#include "../config/MachineConfig.h"

namespace smi::dnc {

class ProgramSource {
public:
    Result<void> loadFromFile(const QString& filePath, const MachineConfig& config);
    bool hasRemaining() const;
    qint64 remaining() const;
    qint64 totalSize() const;
    QByteArray peekChunk(int maxLen) const;
    QByteArray peekNextBlock(int maxBytes = 512) const;
    void commit(qint64 acceptedBytes);
    void rewind(qint64 byteCount);

private:
    QByteArray normalize(const QByteArray& raw, const MachineConfig& config) const;

    QByteArray m_content;
    qint64 m_cursor = 0;
};

} // namespace smi::dnc
