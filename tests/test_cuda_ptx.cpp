#include <gtest/gtest.h>

#include <cuda.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "Common/Exception.h"
#include "Utils/CudaPtxModule.h"
#include "Utils/DeviceBuffer.h"

namespace libcbct::tests {

namespace {

// Model current Slang CUDA output: scalar entry arguments and a constant global block.
constexpr char kPtx[] = R"ptx(
.version 6.0
.target sm_50
.address_size 64
.const .align 8 .b8 SLANG_globalParams[16];
.visible .entry addValue(.param .u32 increment)
{
    .reg .b32 %r<6>;
    .reg .b64 %rd<7>;
    .reg .pred %p;
    mov.u32 %r0, %tid.x;
    mov.u32 %r1, %ctaid.x;
    mov.u32 %r2, %ntid.x;
    mad.lo.u32 %r0, %r1, %r2, %r0;
    cvt.u64.u32 %rd3, %r0;
    ld.const.u64 %rd1, [SLANG_globalParams];
    ld.const.u64 %rd2, [SLANG_globalParams+8];
    setp.ge.u64 %p, %rd3, %rd2;
    @%p bra done;
    shl.b64 %rd3, %rd3, 2;
    add.u64 %rd4, %rd1, %rd3;
    ld.global.u32 %r3, [%rd4];
    ld.param.u32 %r4, [increment];
    add.u32 %r5, %r3, %r4;
    st.global.u32 [%rd4], %r5;
done:
    ret;
}
)ptx";

struct GlobalParams {
    std::int32_t *data;
    std::uint64_t count;
};
static_assert(offsetof(GlobalParams, count) == 8 && sizeof(GlobalParams) == 16);

struct Stream {
    cudaStream_t value = nullptr;
    Stream() { CUDA_CHECK(cudaStreamCreateWithFlags(&value, cudaStreamNonBlocking)); }
    ~Stream() { cudaStreamDestroy(value); }
    Stream(const Stream &) = delete;
    Stream &operator=(const Stream &) = delete;
};

class CudaPtxTest : public ::testing::Test {
protected:
    void SetUp() override {
        int count = 0;
        if (cudaGetDeviceCount(&count) != cudaSuccess || count == 0) {
            GTEST_SKIP() << "No CUDA device available";
        }
        CUDA_CHECK(cudaSetDevice(0));
    }
};

TEST_F(CudaPtxTest, SlangGlobalsAndLaunchShareRuntimeMemoryAndStream) {
    Stream stream;
    std::array<std::int32_t, 37> input{};
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = static_cast<std::int32_t>(2 * i);
    }
    DeviceBuffer<std::int32_t> data(input.size());
    std::int32_t increment = 7;
    const GlobalParams globalValues{ data.ptr(), input.size() };
    CUDA_CHECK(cudaMemcpyAsync(data.ptr(), input.data(), sizeof(input), cudaMemcpyHostToDevice, stream.value));

    // The kernel retains the module after the original module object is destroyed.
    const auto kernel = [&] {
        const auto module = libcbct::CudaPtxModule::fromPtx(kPtx);
        module.setGlobalData("SLANG_globalParams", std::as_bytes(std::span(&globalValues, 1)));
        return module.getKernel("addValue");
    }();
    void *arguments[] = { &increment };
    kernel.launch(dim3(2), dim3(32), arguments, stream.value);
    std::array<std::int32_t, 37> output{};
    CUDA_CHECK(cudaMemcpyAsync(output.data(), data.ptr(), sizeof(output), cudaMemcpyDeviceToHost, stream.value));
    CUDA_CHECK(cudaStreamSynchronize(stream.value));
    for (std::size_t i = 0; i < input.size(); ++i) {
        EXPECT_EQ(output[i], input[i] + increment);
    }
}

TEST_F(CudaPtxTest, LoadsFileAndRestoresCallerContextForRawLaunch) {
    // Use a unique filename and always remove the fixture after this test.
    const auto path = std::filesystem::temp_directory_path() /
                      ("libcbct_ptx_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".ptx");
    struct RemoveFile {
        std::filesystem::path path;
        ~RemoveFile() {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    } cleanup{ path };
    std::ofstream file(path, std::ios::binary);
    ASSERT_TRUE(file);
    file << kPtx;
    file.close();
    ASSERT_TRUE(file);

    DeviceBuffer<std::int32_t> data(1);
    std::int32_t increment = 3;
    const GlobalParams globalValues{ data.ptr(), 1 };
    CUDA_CHECK(cudaMemset(data.ptr(), 0, sizeof(std::int32_t)));

    CUcontext previous = nullptr;
    ASSERT_EQ(cuCtxGetCurrent(&previous), CUDA_SUCCESS);
    struct RestoreContext {
        CUcontext context;
        ~RestoreContext() { cuCtxSetCurrent(context); }
    } restore{ previous };
    ASSERT_EQ(cuCtxSetCurrent(nullptr), CUDA_SUCCESS);
    const auto module = libcbct::CudaPtxModule::fromFile(path, 0);
    module.setGlobalData("SLANG_globalParams", std::as_bytes(std::span(&globalValues, 1)));
    const auto kernel = module.getKernel("addValue");
    void *arguments[] = { &increment };
    kernel.launch(dim3(1), dim3(32), arguments);
    CUcontext current = previous;
    ASSERT_EQ(cuCtxGetCurrent(&current), CUDA_SUCCESS);
    EXPECT_EQ(current, nullptr);
    ASSERT_EQ(cuCtxSetCurrent(previous), CUDA_SUCCESS);
    CUDA_CHECK(cudaDeviceSynchronize());
    std::int32_t output = 0;
    CUDA_CHECK(cudaMemcpy(&output, data.ptr(), sizeof(output), cudaMemcpyDeviceToHost));
    EXPECT_EQ(output, 3);
}

TEST_F(CudaPtxTest, ReportsJitAndEntryPointErrors) {
    const auto module = libcbct::CudaPtxModule::fromPtx(kPtx);
    EXPECT_THROW(module.getKernel("missingEntry"), libcbct::CudaError);
    EXPECT_THROW(module.setGlobalData("missingGlobal", {}), libcbct::CudaError);
    EXPECT_THROW(module.setGlobalData("SLANG_globalParams", {}), libcbct::InvalidArgument);
    const auto kernel = module.getKernel("addValue");
    EXPECT_THROW(kernel.launch(dim3(0), dim3(32)), libcbct::InvalidArgument);
    void *invalidArguments[] = { nullptr };
    EXPECT_THROW(kernel.launch(dim3(1), dim3(32), invalidArguments), libcbct::InvalidArgument);
    try {
        libcbct::CudaPtxModule::fromPtx("invalid PTX source");
        FAIL() << "Invalid PTX should fail JIT compilation";
    } catch (const libcbct::CudaError &error) {
        EXPECT_NE(std::string(error.what()).find("PTX JIT log:"), std::string::npos);
    }
}

TEST(CudaPtxInput, RejectsInvalidInputsBeforeInitializingCuda) {
    EXPECT_THROW(libcbct::CudaPtxModule::fromPtx(""), libcbct::InvalidArgument);
    EXPECT_THROW(libcbct::CudaPtxModule::fromPtx(std::string("text\0extra", 10)), libcbct::InvalidArgument);
    EXPECT_THROW(libcbct::CudaPtxModule::fromPtx(kPtx, -2), libcbct::InvalidArgument);
    EXPECT_THROW(libcbct::CudaPtxModule::fromFile("this/file/does/not/exist.ptx"), libcbct::IOError);
}

}  // namespace

}  // namespace libcbct::tests
