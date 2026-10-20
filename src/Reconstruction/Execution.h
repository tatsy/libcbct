#pragma once

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <vector>

#include "Utils/CheckedSize.h"
#include "Utils/Volume.h"

namespace libcbct {

enum class Backend { Cpu, Cuda, Vulkan };
enum class BufferLayout { XContiguous };
enum class BufferScalar { Float32, Complex64 };

namespace detail {
struct ExecutionContextState {
    Backend backend;
};
}  // namespace detail

class CpuExecutionSession;

// An identity, not a native device handle. Views keep their context alive.
class ExecutionContext {
public:
    Backend backend() const noexcept {
        return state_->backend;
    }
    bool operator==(const ExecutionContext &) const = default;

private:
    explicit ExecutionContext(Backend backend)
        : state_(std::make_shared<detail::ExecutionContextState>(backend)) {
    }
    std::shared_ptr<const detail::ExecutionContextState> state_;
    friend class CpuExecutionSession;
};

template <typename T, std::size_t Rank>
concept TensorConcept =
    (Rank == 2 || Rank == 3)
    && (std::is_same_v<std::remove_const_t<T>, float> || std::is_same_v<std::remove_const_t<T>, std::complex<float>>);

/**
 * @brief Contiguous x-fastest tensors. Rank 2 is [x,y]; rank 3 is [x,y,z/view].
 * @details Owned views retain storage. Borrowed views require the caller to keep the
 * original allocation alive and stable. Const views prohibit writes, not aliases.
 * Host access is explicit; future device adapters must not expose native handles
 * through the operator contracts. Only CPU storage is implemented in this phase.
 */
template <typename T, std::size_t Rank>
    requires(TensorConcept<T, Rank>)
class TensorView {
public:
    using Shape = std::array<std::size_t, Rank>;
    template <typename U>
        requires(std::is_const_v<T> && std::is_same_v<U, std::remove_const_t<T>>)
    TensorView(const TensorView<U, Rank> &other)
        : data_(other.data_)
        , shape_(other.shape_)
        , context_(other.context_)
        , owner_(other.owner_) {
    }

    const Shape &shape() const noexcept {
        return shape_;
    }
    const ExecutionContext &context() const noexcept {
        return context_;
    }
    Backend backend() const noexcept {
        return context_.backend();
    }
    BufferLayout layout() const noexcept {
        return BufferLayout::XContiguous;
    }
    BufferScalar scalar() const noexcept {
        return std::is_same_v<std::remove_const_t<T>, float> ? BufferScalar::Float32 : BufferScalar::Complex64;
    }
    std::span<T> hostElements() const {
        if (backend() != Backend::Cpu) {
            throw InvalidArgument("Buffer is not host accessible");
        }
        return data_;
    }
    TensorView<T, 2> slice(std::size_t index) const
        requires(Rank == 3)
    {
        if (index >= shape_[2]) {
            throw InvalidArgument("Tensor slice is out of range");
        }
        const auto count = shape_[0] * shape_[1];
        return TensorView<T, 2>(data_.subspan(index * count, count), { shape_[0], shape_[1] }, context_, owner_);
    }

    template <typename U, std::size_t OtherRank>
    bool overlaps(const TensorView<U, OtherRank> &other) const {
        const auto a = reinterpret_cast<std::uintptr_t>(hostElements().data());
        const auto b = reinterpret_cast<std::uintptr_t>(other.hostElements().data());
        return a <= b ? b - a < data_.size_bytes() : a - b < other.data_.size_bytes();
    }

private:
    TensorView(std::span<T> data, Shape shape, ExecutionContext context, std::shared_ptr<void> owner = {})
        : data_(data)
        , shape_(shape)
        , context_(std::move(context))
        , owner_(std::move(owner)) {
    }
    std::span<T> data_;
    Shape shape_;
    ExecutionContext context_;
    std::shared_ptr<void> owner_;

    template <typename U, std::size_t OtherRank>
        requires(TensorConcept<U, OtherRank>)
    friend class TensorView;

    friend class CpuExecutionSession;
};

using ProjectionView = TensorView<float, 2>;
using ConstProjectionView = TensorView<const float, 2>;
using SpectrumView = TensorView<std::complex<float>, 2>;
using ConstSpectrumView = TensorView<const std::complex<float>, 2>;
using VolumeView = TensorView<float, 3>;
using ConstVolumeView = TensorView<const float, 3>;

struct ExecutionStatistics {
    // Explicit owned buffers only; excludes control blocks and FFT-library scratch.
    std::size_t bufferAllocations = 0;
    std::size_t bufferBytes = 0;
    std::size_t transferBytes = 0;
};

// Synchronous CPU execution. Sessions/operators with mutable workspaces must not
// be used concurrently. Separate sessions may execute independently.
class CpuExecutionSession {
public:
    CpuExecutionSession()
        : context_(Backend::Cpu) {
    }
    CpuExecutionSession(const CpuExecutionSession &) = delete;
    CpuExecutionSession &operator=(const CpuExecutionSession &) = delete;
    const ExecutionContext &context() const noexcept {
        return context_;
    }
    ExecutionStatistics statistics() const noexcept {
        return statistics_;
    }

    template <typename T, std::size_t Rank>
        requires(!std::is_const_v<T>)
    TensorView<T, Rank> allocate(std::array<std::size_t, Rank> shape) {
        const auto count = checkedCount<T>(shape);
        auto storage = std::make_shared<std::vector<T>>(count);
        ++statistics_.bufferAllocations;
        statistics_.bufferBytes += count * sizeof(T);
        return TensorView<T, Rank>(std::span<T>(*storage), shape, context_, storage);
    }

    template <typename T, std::size_t Rank>
    TensorView<T, Rank> borrow(std::span<T> data, std::array<std::size_t, Rank> shape) const {
        if (checkedCount<T>(shape) != data.size() || !data.data()) {
            throw InvalidArgument("Tensor storage does not match its shape");
        }
        return TensorView<T, Rank>(data, shape, context_);
    }
    template <typename T>
    TensorView<T, 3> borrow(Volume<T> &volume) const {
        return borrow(volume.elements(),
                      std::array{ volume.template size<0>(), volume.template size<1>(), volume.template size<2>() });
    }
    template <typename T>
    TensorView<const T, 3> borrow(const Volume<T> &volume) const {
        return borrow(volume.elements(),
                      std::array{ volume.template size<0>(), volume.template size<1>(), volume.template size<2>() });
    }

private:
    template <typename T, std::size_t Rank>
    static std::size_t checkedCount(const std::array<std::size_t, Rank> &shape) {
        std::size_t count = 1;
        for (auto extent : shape) {
            if (!extent) {
                throw InvalidArgument("Tensor dimensions must be positive");
            }
            count = detail::checkedProduct(count, extent);
        }
        const auto bytes = detail::checkedProduct(count, sizeof(T));
        if (bytes > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max())) {
            throw InvalidArgument("Tensor exceeds addressable range");
        }
        return count;
    }
    ExecutionContext context_;
    ExecutionStatistics statistics_;
};

}  // namespace libcbct
