#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_EXCEPTION_H
#define LIBCBCT_EXCEPTION_H

#include <string>
#include <stdexcept>

namespace libcbct {

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class IOError : public Error {
    using Error::Error;
};

class InvalidArgument : public Error {
public:
    using Error::Error;
};

class CudaError : public Error {
public:
    CudaError(int code, const std::string &message)
        : Error(message)
        , code_(code) {
    }

    int code() const noexcept {
        return code_;
    }

private:
    int code_;
};

}  // namespace libcbct

#endif  // LIBCBCT_EXCEPTION_H
