#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_EXECUTION_CONTEXT_H
#define LIBCBCT_EXECUTION_CONTEXT_H

#include <memory>

namespace libcbct {

enum class Backend { Cpu, Cuda, Vulkan };
enum class BufferLayout { XContiguous };
enum class BufferScalar { Float32, Complex64 };

class CpuExecutionSession;
class CudaExecutionSession;

namespace detail {

struct ExecutionContextState {
    Backend backend;
};

}  // namespace detail

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
    friend class CudaExecutionSession;
};

}  // namespace libcbct

#endif  // LIBCBCT_EXECUTION_CONTEXT_H
