#include "ProgramSource.h"

#include <QFile>
#include <algorithm>

namespace smi::dnc {

namespace {
QByteArray stripUtf8BomIfNeeded(const QByteArray& raw, bool enabled) {
    if (!enabled) return raw;
    static const QByteArray bom("\xEF\xBB\xBF", 3);
    if (raw.startsWith(bom)) return raw.mid(bom.size());
    return raw;
}

QByteArray forceAscii7IfNeeded(QByteArray data, bool enabled) {
    if (!enabled) return data;
    for (char& ch : data) {
        const unsigned char b = static_cast<unsigned char>(ch);
        if (b == 0x09 || b == 0x0A || b == 0x0D || b == 0x1A) continue;
        if (b > 0x7F) ch = '?';
    }
    return data;
}

bool alreadyWrappedWithPercent(const QByteArray& data) {
    const QByteArray trimmed = data.trimmed();
    return trimmed.startsWith('%') && trimmed.endsWith('%');
}

QByteArray defaultPrefix(const MachineConfig& config) {
    return QByteArray("%") + config.lineEnding;
}

QByteArray defaultSuffix(const MachineConfig& config) {
    return config.lineEnding + QByteArray("%") + config.lineEnding;
}
} // namespace

Result<void> ProgramSource::loadFromFile(const QString& filePath, const MachineConfig& config) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return Result<void>::fail("Não foi possível abrir o arquivo do programa.");
    }

    const QByteArray raw = file.readAll();
    m_content = normalize(raw, config);
    m_cursor = 0;
    if (config.appendEof && !config.eofSequence.isEmpty()) {
        m_content.append(config.eofSequence);
    }
    return Result<void>::ok();
}

bool ProgramSource::hasRemaining() const { return m_cursor < m_content.size(); }
qint64 ProgramSource::remaining() const { return m_content.size() - m_cursor; }
qint64 ProgramSource::totalSize() const { return m_content.size(); }
QByteArray ProgramSource::peekChunk(int maxLen) const { return m_content.mid(m_cursor, maxLen); }

QByteArray ProgramSource::peekNextBlock(int maxBytes) const {
    if (!hasRemaining()) return {};

    const int remainingBytes = static_cast<int>(std::min<qint64>(remaining(), std::max(1, maxBytes)));
    QByteArray chunk = m_content.mid(m_cursor, remainingBytes);

    const int lf = chunk.indexOf('\n');
    const int cr = chunk.indexOf('\r');
    int idx = -1;
    if (lf >= 0 && cr >= 0) idx = std::min(lf, cr);
    else idx = std::max(lf, cr);

    if (idx < 0) return chunk;

    int end = idx + 1;
    if (chunk[idx] == '\r' && end < chunk.size() && chunk[end] == '\n') {
        ++end;
    }
    return chunk.left(end);
}

void ProgramSource::commit(qint64 acceptedBytes) {
    m_cursor += acceptedBytes;
    if (m_cursor < 0) m_cursor = 0;
    if (m_cursor > m_content.size()) m_cursor = m_content.size();
}

void ProgramSource::rewind(qint64 byteCount) {
    if (byteCount <= 0) return;
    m_cursor -= byteCount;
    if (m_cursor < 0) m_cursor = 0;
}

QByteArray ProgramSource::normalize(const QByteArray& raw, const MachineConfig& config) const {
    if (config.binarySafeMode) return raw;

    QByteArray data = stripUtf8BomIfNeeded(raw, config.stripUtf8Bom);

    if (config.normalizeLineEndings) {
        data.replace("\r\n", "\n");
        data.replace('\r', '\n');

        QList<QByteArray> lines = data.split('\n');
        if (config.trimTrailingSpaces) {
            for (QByteArray& line : lines) {
                while (!line.isEmpty() && (line.endsWith(' ') || line.endsWith('\t') || line.endsWith('\r'))) {
                    line.chop(1);
                }
            }
        }
        data = lines.join(config.lineEnding);
    }

    data = forceAscii7IfNeeded(data, config.forceAscii7 || config.dataBits == QSerialPort::Data7);

    if (config.ensureTrailingLineEnding && !config.lineEnding.isEmpty() && !data.endsWith(config.lineEnding)) {
        data.append(config.lineEnding);
    }

    QByteArray prefix = config.sendPrefix;
    QByteArray suffix = config.sendSuffix;
    if (config.autoWrapPercent && !alreadyWrappedWithPercent(data)) {
        if (prefix.isEmpty()) prefix = defaultPrefix(config);
        if (suffix.isEmpty()) suffix = defaultSuffix(config);
    }

    if (!prefix.isEmpty() && !data.startsWith(prefix)) data.prepend(prefix);
    if (!suffix.isEmpty() && !data.endsWith(suffix)) data.append(suffix);

    return data;
}

} // namespace smi::dnc
