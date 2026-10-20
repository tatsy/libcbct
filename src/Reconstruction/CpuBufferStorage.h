#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_CPU_BUFFER_STORAGE_H
#define LIBCBCT_CPU_BUFFER_STORAGE_H

#include <span>
#include <vector>
#include <type_traits>

#include "BufferStorage.h"
#include "Utils/CheckedSize.h"

namespace libcbct::detail {

template <typename T>
class CpuBufferView : public BufferStorage {
public:
    CpuBufferView(ExecutionContext context, std::span<T> elements)
        : BufferStorage(std::move(context), checkedProduct(elements.size(), sizeof(T)))
        , elements_(elements) {
    }

    const void *hostDataConst() const noexcept override {
        return elements_.data();
    }

    void *hostData() noexcept override {
        if constexpr (std::is_const_v<T>) {
            return nullptr;
        } else {
            return elements_.data();
        }
    }

private:
    std::span<T> elements_;
};

template <typename T>
    requires(!std::is_const_v<T>)
class CpuBufferStorage : public BufferStorage {
public:
    CpuBufferStorage(ExecutionContext context, std::size_t count)
        : BufferStorage(std::move(context), checkedProduct(count, sizeof(T)))
        , elements_(count) {
    }

    const void *hostDataConst() const noexcept override {
        return elements_.data();
    }

    void *hostData() noexcept override {
        return elements_.data();
    }

private:
    std::vector<T> elements_;
};

}  // namespace libcbct::detail

#endif  // LIBCBCT_CPU_BUFFER_STORAGE_H
