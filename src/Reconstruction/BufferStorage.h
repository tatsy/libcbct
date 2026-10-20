#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_BUFFER_STORAGE_H
#define LIBCBCT_BUFFER_STORAGE_H

#include <cstddef>
#include <utility>

#include "ExecutionContext.h"

namespace libcbct::detail {

class BufferStorage {
public:
    virtual ~BufferStorage() = default;

    BufferStorage(const BufferStorage &) = delete;
    BufferStorage &operator=(const BufferStorage &) = delete;

    std::size_t sizeBytes() const noexcept {
        return sizeBytes_;
    }

    const ExecutionContext &context() const noexcept {
        return context_;
    }

    virtual const void *hostDataConst() const noexcept {
        return nullptr;
    }

    virtual void *hostData() noexcept {
        return nullptr;
    }

protected:
    BufferStorage(ExecutionContext context, std::size_t sizeBytes)
        : context_(std::move(context))
        , sizeBytes_(sizeBytes) {
    }

private:
    ExecutionContext context_;
    std::size_t sizeBytes_;
};

}  // namespace libcbct::detail

#endif  // LIBCBCT_BUFFER_STORAGE_H
