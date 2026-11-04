// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace pfx {

enum class ErrorCode
{
    InvalidArgument,
    NotFound,
    UnsupportedFormat,
    DecodeFailed,
    EncodeFailed,
    ColorManagement,
    ParseError,
    NewerVersion,
    Unsupported,
    Io,
};

struct Error
{
    ErrorCode code = ErrorCode::InvalidArgument;
    std::string message;
};

// A value or an error. No exceptions cross the library's public API.
template <typename T>
class [[nodiscard]] Result
{
public:
    Result(T value) // NOLINT(google-explicit-constructor): implicit by design, like std::expected
        : m_state(std::move(value))
    {}
    Result(Error error) // NOLINT(google-explicit-constructor)
        : m_state(std::move(error))
    {}

    [[nodiscard]] bool ok() const { return std::holds_alternative<T>(m_state); }
    explicit operator bool() const { return ok(); }

    [[nodiscard]] T& value() & { return std::get<T>(m_state); }
    [[nodiscard]] const T& value() const& { return std::get<T>(m_state); }
    [[nodiscard]] T&& value() && { return std::get<T>(std::move(m_state)); }
    [[nodiscard]] const Error& error() const { return std::get<Error>(m_state); }

private:
    std::variant<T, Error> m_state;
};

// Success or an error, for calls with no value.
class [[nodiscard]] Status
{
public:
    Status() = default;
    Status(Error error) // NOLINT(google-explicit-constructor)
        : m_error(std::move(error))
    {}

    [[nodiscard]] bool ok() const { return !m_error.has_value(); }
    explicit operator bool() const { return ok(); }
    [[nodiscard]] const Error& error() const { return *m_error; }

private:
    std::optional<Error> m_error;
};

} // namespace pfx
