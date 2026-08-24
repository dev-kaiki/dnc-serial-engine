#pragma once

#include <QString>

namespace smi::dnc {

template<typename T>
class Result {
public:
    static Result<T> ok(const T& value) { return Result<T>(true, value, {}); }
    static Result<T> fail(const QString& error) { return Result<T>(false, T{}, error); }

    bool isOk() const { return m_ok; }
    const T& value() const { return m_value; }
    const QString& error() const { return m_error; }

private:
    Result(bool ok, const T& value, const QString& error) : m_ok(ok), m_value(value), m_error(error) {}
    bool m_ok = false;
    T m_value{};
    QString m_error;
};

template<>
class Result<void> {
public:
    static Result<void> ok() { return Result<void>(true, {}); }
    static Result<void> fail(const QString& error) { return Result<void>(false, error); }

    bool isOk() const { return m_ok; }
    const QString& error() const { return m_error; }

private:
    Result(bool ok, const QString& error) : m_ok(ok), m_error(error) {}
    bool m_ok = false;
    QString m_error;
};

} // namespace smi::dnc
