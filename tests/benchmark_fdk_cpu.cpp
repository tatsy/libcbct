#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

#include "Phantom.h"
#include "Reconstruction/Feldkamp.h"
#include "Reconstruction/FeldkampCpu.h"

namespace libcbct::testing_utils {
namespace {

std::atomic<bool> countAllocations{false};
std::atomic<std::size_t> allocationCalls{0}, allocationBytes{0};

void recordAllocation(std::size_t bytes) {
    if (countAllocations.load(std::memory_order_relaxed)) {
        allocationCalls.fetch_add(1, std::memory_order_relaxed);
        allocationBytes.fetch_add(bytes, std::memory_order_relaxed);
    }
}

struct Measurement {
    double milliseconds;
    std::size_t calls, bytes;
};

Measurement measure(const ReconstructionBase &fdk, const VolumeF32 &input, const Geometry &geometry) {
    allocationCalls = 0;
    allocationBytes = 0;
    countAllocations = true;
    const auto start = std::chrono::steady_clock::now();
    const auto volume = fdk.reconstruct(input, geometry);
    const auto stop = std::chrono::steady_clock::now();
    countAllocations = false;
    return { std::chrono::duration<double, std::milli>(stop - start).count(),
             allocationCalls.load(), allocationBytes.load() };
}

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

bool compare(const char *name, const VolumeF32 &input, const Geometry &geometry, RampFilter filter) {
    const FeldkampCpu legacy(filter);
    const Feldkamp operators(Backend::Cpu, filter);
    // Warm both paths, and check numerical equivalence outside the timed runs.
    const auto a = legacy.reconstruct(input, geometry);
    const auto b = operators.reconstruct(input, geometry);
    double error = 0;
    for (std::size_t i = 0; i < a.numel(); ++i) {
        if (!std::isfinite(a.ptr()[i]) || !std::isfinite(b.ptr()[i])) return false;
        error = std::max(error, std::abs(static_cast<double>(a.ptr()[i]) - b.ptr()[i]));
    }
    std::vector<double> oldTimes, newTimes;
    Measurement oldMeasurement{}, newMeasurement{};
    for (int repetition = 0; repetition < 7; ++repetition) {
        // Alternate order to reduce systematic warm-cache/order bias.
        if (repetition % 2) {
            newMeasurement = measure(operators, input, geometry);
            oldMeasurement = measure(legacy, input, geometry);
        } else {
            oldMeasurement = measure(legacy, input, geometry);
            newMeasurement = measure(operators, input, geometry);
        }
        oldTimes.push_back(oldMeasurement.milliseconds);
        newTimes.push_back(newMeasurement.milliseconds);
    }
    std::fprintf(stderr, "%s,%s,%.3f,%.3f,%.3f,%.9g,%zu,%zu,%zu,%zu\n", name,
                 filter == RampFilter::RamLak ? "RamLak" : "SheppLogan", median(oldTimes), median(newTimes),
                 median(newTimes) / median(oldTimes), error, oldMeasurement.calls, newMeasurement.calls,
                 oldMeasurement.bytes, newMeasurement.bytes);
    return error < 1e-6;
}

}  // namespace
}  // namespace libcbct::testing_utils

// Benchmark-only replaceable allocation functions. Count ordinary C++ allocation
// requests, NOT all process/native allocations (pocketfft uses aligned malloc).
// These definitions are never linked into the library or the unit tests.
void *operator new(std::size_t bytes) {
    if (void *p = std::malloc(bytes ? bytes : 1)) {
        libcbct::testing_utils::recordAllocation(bytes);
        return p;
    }
    throw std::bad_alloc();
}
void *operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

int main() {
    using namespace libcbct;
    using namespace libcbct::testing_utils;
    std::fprintf(stderr, "case,filter,legacy_ms,operators_ms,ratio,max_abs_error,legacy_cpp_allocations,"
                         "operators_cpp_allocations,legacy_cpp_requested_bytes,operators_cpp_requested_bytes\n");
    const PhantomSetup setup;
    const auto sphere = makeSphereSinogram(setup, { { 2, 1, 0, 6, 0.02 }, { 0, 0, 7, 3, 0.02 } });
    VolumeF32 small(17, 19, 7);
    for (std::size_t i = 0; i < small.numel(); ++i) small.ptr()[i] = 0.01f * (static_cast<int>(i % 17) - 8);
    const Geometry smallGeometry(vec2i(17, 19), vec2f(0.8f, 1.1f), vec3i(17, 19, 5), 100, 400);
    bool passed = true;
    for (const auto filter : { RampFilter::RamLak, RampFilter::SheppLogan }) {
        passed &= compare("sphere_128x128x180_64cubed", sphere, setup.geometry(), filter);
        passed &= compare("rectangular_17x19x7", small, smallGeometry, filter);
    }
    return passed ? 0 : 1;
}
