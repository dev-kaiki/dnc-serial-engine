#pragma once

#include <QObject>
#include <QTimer>
#include <QString>

namespace smi::dnc {

struct SimulationReport {
    qint64 bytesFed = 0;
    int xoffCount = 0;
    int xonCount = 0;
    int configuredCycleBytes = 0;
    int resumeDelayMs = 0;
};

class CncSimulator : public QObject {
    Q_OBJECT
public:
    explicit CncSimulator(QObject* parent = nullptr);

    void setXoffCycleBytes(int bytes);
    void setResumeDelayMs(int delayMs);
    void feedBytes(const QByteArray& data);
    void reset();
    SimulationReport simulatePayload(const QByteArray& data);

signals:
    void generatedRx(const QByteArray& data);
    void reportReady(const smi::dnc::SimulationReport& report);

private slots:
    void releaseXon();

private:
    int m_buffered = 0;
    int m_xoffCycleBytes = 256;
    int m_resumeDelayMs = 120;
    bool m_holding = false;
    QTimer m_resumeTimer;
    SimulationReport m_report;
};

} // namespace smi::dnc
