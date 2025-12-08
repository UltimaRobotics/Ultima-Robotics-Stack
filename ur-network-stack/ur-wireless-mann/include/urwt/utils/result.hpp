#ifndef URWT_UTILS_RESULT_HPP
#define URWT_UTILS_RESULT_HPP

#include <variant>
#include <stdexcept>
#include <string>

namespace urwt {

template <typename T, typename E = std::string>
class Result {
private:
    struct OkTag {};
    struct ErrorTag {};
    
    Result(OkTag, T value) : data_(std::in_place_index<0>, std::move(value)) {}
    Result(ErrorTag, E error) : data_(std::in_place_index<1>, std::move(error)) {}

public:
    static Result<T, E> ok(T value) {
        return Result<T, E>(OkTag{}, std::move(value));
    }

    static Result<T, E> error(E err) {
        return Result<T, E>(ErrorTag{}, std::move(err));
    }

    bool isOk() const {
        return data_.index() == 0;
    }

    bool isError() const {
        return data_.index() == 1;
    }

    const T& value() const {
        if (!isOk()) {
            throw std::runtime_error("Attempted to access value on error Result");
        }
        return std::get<0>(data_);
    }

    T& value() {
        if (!isOk()) {
            throw std::runtime_error("Attempted to access value on error Result");
        }
        return std::get<0>(data_);
    }

    const E& error() const {
        if (!isError()) {
            throw std::runtime_error("Attempted to access error on ok Result");
        }
        return std::get<1>(data_);
    }

    E& error() {
        if (!isError()) {
            throw std::runtime_error("Attempted to access error on ok Result");
        }
        return std::get<1>(data_);
    }

private:
    std::variant<T, E> data_;
};

}

#endif
