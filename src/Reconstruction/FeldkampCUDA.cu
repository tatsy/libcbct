#define LIBCBCT_API_EXPORT
#include "FeldkampCUDA.h"

#include <cuda_runtime.h>
#include <device_functions.h>
#include <device_launch_parameters.h>
#include <cufft.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include "Utils/CudaUtils.h"
#include "Utils/ImageUtils.h"
#include "Common/ProgressBar.h"

#define BLOCK_SIZE 8

__global__ void rampFilterKernel(vec2f *buffer, int detWidth, int detHeight, RampFilter filter) {
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= 0 && y >= 0 && x < detWidth && y < detHeight) {
        const float q = min(x, detWidth - x) / (0.5f * detWidth);
        if (filter == RampFilter::RamLak) {
            buffer[y * detWidth + x] *= fabsf(q);
        } else if (filter == RampFilter::SheppLogan) {
            buffer[y * detWidth + x] *= (2.0f / M_PI) * fabsf(sinf(0.5f * M_PI * q));
        } else {
            asm("trap;");
        }
    }
}

__global__ void sliceBackProjectionKernel(int z, int i, Geometry &geometry, float *slice, vec3i volSize, float *image,
                                          vec3i sinoSize) {
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= 0 && y >= 0 && x < volSize.x && y < volSize.y) {
        const float theta = 2.0f * (float)M_PI * i / sinoSize.z;
        const vec3f uvw = vox2pix(vec3i(x, y, z), theta, geometry);
        if (uvw.x >= 0 && uvw.y >= 0 && uvw.x < sinoSize.x && uvw.y < sinoSize.y) {
            const float val = bilerp(image, sinoSize.x, sinoSize.y, uvw.x - 0.5f, uvw.y - 0.5f);
            slice[y * volSize.x + x] += val * uvw.z / sinoSize.z;
        }
    }
}

void FeldkampCUDA::reconstruct(const FloatVolume &sinogram, FloatVolume &tomogram, const Geometry &geometry) const {
    const int detWidth = sinogram.size<0>();
    const int detHeight = sinogram.size<1>();
    const int nProj = sinogram.size<2>();
    const vec3i sinoSize(detWidth, detHeight, nProj);
    const vec3i volSize = geometry.volSize;
    tomogram.resize(volSize.x, volSize.y, volSize.z);

    Geometry *devGeom;
    CUDA_CHECK(cudaMalloc(&devGeom, sizeof(Geometry)));
    CUDA_CHECK(cudaMemcpy(devGeom, &geometry, sizeof(Geometry), cudaMemcpyHostToDevice));

    // Allocate CUDA memories
    float *devImg;
    vec2f *devFft;
    float *devVolume;
    CUDA_CHECK(cudaMalloc(&devImg, sizeof(float) * detWidth * detHeight));
    CUDA_CHECK(cudaMalloc(&devFft, sizeof(vec2f) * detWidth * detHeight));
    CUDA_CHECK(cudaMalloc(&devVolume, sizeof(float) * (uint64_t)volSize.x * (uint64_t)volSize.y * (uint64_t)volSize.z));
    CUDA_CHECK(
        cudaMemset(devVolume, 0, sizeof(float) * (uint64_t)volSize.x * (uint64_t)volSize.y * (uint64_t)volSize.z));

    // FFT parameters
    const int rank = 1;
    int n[] = { detWidth };
    int inembed[] = { 0 };
    int onembed[] = { 0 };
    const int istride = 1;
    const int ostride = 1;
    const int idist = detWidth;
    const int odist = detWidth;
    const int batch = detHeight;

    cufftHandle planForward;
    CUDA_CHECK(
        cufftPlanMany(&planForward, rank, n, inembed, istride, idist, onembed, ostride, odist, CUFFT_R2C, batch));

    cufftHandle planInverse;
    CUDA_CHECK(
        cufftPlanMany(&planInverse, rank, n, inembed, istride, idist, onembed, ostride, odist, CUFFT_C2R, batch));

    ProgressBar pbar(nProj);
    pbar.setDescription("RECON: ");
    for (int i = 0; i < nProj; i++) {
        const uint64_t ptrOffset = detWidth * detHeight * (uint64_t)i;
        float *const imgPtr = sinogram.ptr() + ptrOffset;
        CUDA_CHECK(cudaMemcpy(devImg, imgPtr, sizeof(float) * detWidth * detHeight, cudaMemcpyHostToDevice));

        //Filter
        CUDA_CHECK(cufftExecR2C(planForward, devImg, (float2 *)devFft));
        CUDA_SYNC_CHECK();

        const int nGridX = (detWidth + BLOCK_SIZE - 1) / BLOCK_SIZE;
        const int nGridY = (detHeight + BLOCK_SIZE - 1) / BLOCK_SIZE;
        dim3 block(BLOCK_SIZE, BLOCK_SIZE, 1);
        dim3 grid(nGridX, nGridY, 1);
        rampFilterKernel<<<grid, block>>>(devFft, detWidth, detHeight, filter);
        CUDA_SYNC_CHECK();

        CUDA_CHECK(cufftExecC2R(planInverse, (float2 *)devFft, devImg));
        CUDA_SYNC_CHECK();

        // Back projection
        for (int z = 0; z < volSize.z; z++) {
            float *const devSlice = devVolume + (uint64_t)volSize.x * (uint64_t)volSize.y * (uint64_t)z;
            const int nGridX = (volSize.x + BLOCK_SIZE - 1) / BLOCK_SIZE;
            const int nGridY = (volSize.y + BLOCK_SIZE - 1) / BLOCK_SIZE;
            dim3 block(BLOCK_SIZE, BLOCK_SIZE, 1);
            dim3 grid(nGridX, nGridY, 1);
            sliceBackProjectionKernel<<<grid, block>>>(z, i, *devGeom, devSlice, volSize, devImg, sinoSize);
            CUDA_SYNC_CHECK();
        }
        CUDA_SYNC_CHECK();

        pbar.step();
    }

    CUDA_CHECK(cudaMemcpy(tomogram.ptr(), devVolume, sizeof(float) * volSize.x * volSize.y * volSize.z,
                          cudaMemcpyDeviceToHost));

    // Free CUDA memories
    CUDA_CHECK(cudaFree(devImg));
    CUDA_CHECK(cudaFree(devFft));
    CUDA_CHECK(cudaFree(devVolume));
    CUDA_CHECK(cudaFree(devGeom));
    CUDA_CHECK(cufftDestroy(planForward));
    CUDA_CHECK(cufftDestroy(planInverse));
    CUDA_SYNC_CHECK();
}
