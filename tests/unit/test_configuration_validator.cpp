#include <QtTest/QtTest>
#include "core/config/ConfigurationValidator.h"

using namespace smi::dnc;

class TestConfigurationValidator : public QObject {
    Q_OBJECT
private slots:
    void rejects_xon_requirement_without_xon();
    void accepts_basic_config();
};

void TestConfigurationValidator::rejects_xon_requirement_without_xon() {
    MachineConfig c;
    c.portName = "COM1";
    c.requireXonToSend = true;
    c.interpretXonXoff = false;
    QVERIFY(!ConfigurationValidator::validate(c).isOk());
}

void TestConfigurationValidator::accepts_basic_config() {
    MachineConfig c;
    c.portName = "COM1";
    QVERIFY(ConfigurationValidator::validate(c).isOk());
}

QTEST_MAIN(TestConfigurationValidator)
#include "test_configuration_validator.moc"
