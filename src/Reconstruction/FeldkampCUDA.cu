#define LIBCBCT_API_EXPORT
#include "FeldkampCUDA.h"

#include <cuda_runtime.h>
#include <cufft.h>

#include <vector>

#include "Common/Constants.h"
#include "Utils/CudaUtils.h"
#include "Utils/ImageUtils.h"
#include "Common/ProgressBar.h"
#include "Reconstruction/FilterUtils.h"

#define BLOCK_SIZE 16

#define CUFFT_CHECK(call)                                                          \
    do {                                                                           \
        const cufftResult rc = (call);                                             \
        if (rc != CUFFT_SUCCESS) {                                                 \
            LIBCBCT_ERROR("cuFFT error %d at %s:%d", (int)rc, __FILE__, __LINE__); \
        }                                                                          \
    } while (false)

__global__ void weightAndPadKernel(const float *__restrict__ proj, const float *__restrict__ weights,
                                   float *__restrict__ padded, int detWidth, int detHeight, int nPad) {
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= nPad || y >= detHeight) {
        return;
    }

    float val = 0.0f;
    if (x < detWidth) {
        const int idx = y * detWidth + x;
        val = proj[idx] * weights[idx];
    }

    padded[y * nPad + x] = val;
}

__global__ void applyFilterKernel(cufftComplex *spec, const float *__restrict__ H, int nFreq, int detHeight) {
    const int k = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (k >= nFreq || y >= detHeight) {
        return;
    }

    const float h = H[k];
    cufftComplex &c = spec[y * nFreq + k];
    c.x *= h;
    c.y *= h;
}

__global__ void backProjectionKernel(const float *const filtered, int detWidth, int detHeight, Geometry geometry,
                                     float theta, float scale, float *volume) {
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    const int z = blockIdx.z;
    const vec3i volSize = geometry.volSize;
    if (x >= volSize.x || y >= volSize.y || z >= volSize.z) {
        return;
    }

    const vec3f uvw = vox2pix(vec3i(x, y, z), theta, geometry);
    if (uvw.x >= 0 && uvw.y >= 0 && uvw.x < detWidth && uvw.y < detHeight) {
        const float val = bilerp(filtered, detWidth, detHeight, uvw.x - 0.5f, uvw.y - 0.5f);
        volume[(z * volSize.y + y) * volSize.x + x] += val * uvw.z * scale;
    }
}

VolumeF32 FeldkampCUDA::reconstruct(const VolumeF32 &sinogram, const Geometry &geometry) const {
    const size_t detWidth = sinogram.size<0>();
    const size_t detHeight = sinogram.size<1>();
    const size_t nProj = sinogram.size<2>();
    const vec3i volSize = geometry.volSize;
    const size_t nPixels = detWidth * detHeight;
    const size_t nVoxels = (size_t)volSize.x * volSize.y * volSize.z;

    VolumeF32 tomogram(volSize.x, volSize.y, volSize.z);

    // Filter setup
    const size_t nPad = paddedFFTLength(detWidth);
    const size_t nFreq = nPad / 2 + 1;
    const double dp = geometry.pixSize.x * geometry.sod / geometry.sdd;
    std::vector<float> H = makeRampResponse(nPad, dp, filter);
    for (float &h : H) {
        h /= (float)nPad;
    }
    const std::vector<float> cosWeights = makeCosineWeights(geometry, detWidth, detHeight);

    Geometry *devGeom;
    CUDA_CHECK(cudaMalloc(&devGeom, sizeof(Geometry)));
    CUDA_CHECK(cudaMemcpy(devGeom, &geometry, sizeof(Geometry), cudaMemcpyHostToDevice));

    // Allocate CUDA memories
    float *devProj, *devCosW, *devPadded, *devH, *devFiltered, *devVolume;
    cufftComplex *devSpec;

    CUDA_CHECK(cudaMalloc(&devProj, sizeof(float) * nPixels));
    CUDA_CHECK(cudaMalloc(&devCosW, sizeof(float) * nPixels));
    CUDA_CHECK(cudaMalloc(&devPadded, sizeof(float) * detHeight * nPad));
    CUDA_CHECK(cudaMalloc(&devSpec, sizeof(cufftComplex) * detHeight * nFreq));
    CUDA_CHECK(cudaMalloc(&devH, sizeof(float) * nFreq));
    CUDA_CHECK(cudaMalloc(&devFiltered, sizeof(float) * nPixels));
    CUDA_CHECK(cudaMalloc(&devVolume, sizeof(float) * nVoxels));

    CUDA_CHECK(cudaMemset(devVolume, 0, sizeof(float) * nVoxels));
    CUDA_CHECK(cudaMemcpy(devCosW, cosWeights.data(), sizeof(float) * nPixels, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(devH, H.data(), sizeof(float) * nFreq, cudaMemcpyHostToDevice));

    // Reconstruction
    cufftHandle planForward, planInverse;
    CUFFT_CHECK(cufftPlan1d(&planForward, nPad, CUFFT_R2C, detHeight));
    CUFFT_CHECK(cufftPlan1d(&planInverse, nPad, CUFFT_C2R, detHeight));

    const dim3 block(BLOCK_SIZE, BLOCK_SIZE, 1);
    const dim3 gridPad(divRoundUp((int)nPad, BLOCK_SIZE), divRoundUp((int)detHeight, BLOCK_SIZE), 1);
    const dim3 gridFreq(divRoundUp((int)nFreq, BLOCK_SIZE), divRoundUp((int)detHeight, BLOCK_SIZE), 1);
    const dim3 gridVol(divRoundUp(volSize.x, BLOCK_SIZE), divRoundUp(volSize.y, BLOCK_SIZE), volSize.z);
    const float scale = (float)libcbct::kPi / nProj;

    ProgressBar pbar(nProj);
    pbar.setDescription("RECON: ");
    for (int i = 0; i < nProj; i++) {
        CUDA_CHECK(cudaMemcpy(devProj, sinogram.ptr() + nPixels * i, sizeof(float) * nPixels, cudaMemcpyHostToDevice));

        // Cosine weighting and zero padding
        weightAndPadKernel<<<gridPad, block>>>(devProj, devCosW, devPadded, detWidth, detHeight, nPad);

        // Ramp filtering
        CUFFT_CHECK(cufftExecR2C(planForward, devPadded, devSpec));
        applyFilterKernel<<<gridFreq, block>>>(devSpec, devH, nFreq, detHeight);
        CUFFT_CHECK(cufftExecC2R(planInverse, devSpec, devPadded));

        // Crop the valid detWidth columns
        CUDA_CHECK(cudaMemcpy2D(devFiltered, sizeof(float) * detWidth, devPadded, sizeof(float) * nPad,
                                sizeof(float) * detWidth, detHeight, cudaMemcpyDeviceToDevice));

        // Backprojection
        const float theta = (float)libcbct::kTwoPi * i / nProj;
        backProjectionKernel<<<gridVol, block>>>(devFiltered, detWidth, detHeight, geometry, theta, scale, devVolume);

        CUDA_SYNC_CHECK();

        pbar.step();
    }

    CUDA_CHECK(cudaMemcpy(tomogram.ptr(), devVolume, sizeof(float) * nVoxels, cudaMemcpyDeviceToHost));

    // Free CUDA memories
    CUFFT_CHECK(cufftDestroy(planForward));
    CUFFT_CHECK(cufftDestroy(planInverse));

    CUDA_CHECK(cudaFree(devProj));
    CUDA_CHECK(cudaFree(devCosW));
    CUDA_CHECK(cudaFree(devPadded));
    CUDA_CHECK(cudaFree(devSpec));
    CUDA_CHECK(cudaFree(devH));
    CUDA_CHECK(cudaFree(devFiltered));
    CUDA_CHECK(cudaFree(devVolume));

    return tomogram;
}
