#define LIBCBCT_API_EXPORT
#include "FeldkampCUDA.h"

#include <vector>

#include "Common/Constants.h"
#include "Utils/CudaUtils.h"
#include "Utils/ImageUtils.h"
#include "Utils/DeviceBuffer.h"
#include "Common/ProgressBar.h"
#include "Reconstruction/FilterUtils.h"

#define BLOCK_SIZE 16

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
        volume[((size_t)z * volSize.y + y) * volSize.x + x] += val * uvw.z * scale;
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

    // Allocate CUDA memories
    DeviceBuffer<float> devProj(sizeof(float) * nPixels);
    DeviceBuffer<float> devCosW(sizeof(float) * nPixels);
    DeviceBuffer<float> devPadded(sizeof(float) * detHeight * nPad);
    DeviceBuffer<cufftComplex> devSpec(sizeof(cufftComplex) * detHeight * nFreq);
    DeviceBuffer<float> devH(sizeof(float) * nFreq);
    DeviceBuffer<float> devFiltered(sizeof(float) * nPixels);
    DeviceBuffer<float> devVolume(sizeof(float) * nVoxels);

    CUDA_CHECK(cudaMemset(devVolume.ptr(), 0, sizeof(float) * nVoxels));
    CUDA_CHECK(cudaMemcpy(devCosW.ptr(), cosWeights.data(), sizeof(float) * nPixels, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(devH.ptr(), H.data(), sizeof(float) * nFreq, cudaMemcpyHostToDevice));

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
        CUDA_CHECK(
            cudaMemcpy(devProj.ptr(), sinogram.ptr() + nPixels * i, sizeof(float) * nPixels, cudaMemcpyHostToDevice));

        // Cosine weighting and zero padding
        weightAndPadKernel<<<gridPad, block>>>(devProj.ptr(), devCosW.ptr(), devPadded.ptr(), detWidth, detHeight,
                                               nPad);

        // Ramp filtering
        CUFFT_CHECK(cufftExecR2C(planForward, devPadded.ptr(), devSpec.ptr()));
        applyFilterKernel<<<gridFreq, block>>>(devSpec.ptr(), devH.ptr(), nFreq, detHeight);
        CUFFT_CHECK(cufftExecC2R(planInverse, devSpec.ptr(), devPadded.ptr()));

        // Crop the valid detWidth columns
        CUDA_CHECK(cudaMemcpy2D(devFiltered.ptr(), sizeof(float) * detWidth, devPadded.ptr(), sizeof(float) * nPad,
                                sizeof(float) * detWidth, detHeight, cudaMemcpyDeviceToDevice));

        // Backprojection
        const float theta = (float)libcbct::kTwoPi * i / nProj;
        backProjectionKernel<<<gridVol, block>>>(devFiltered.ptr(), detWidth, detHeight, geometry, theta, scale,
                                                 devVolume.ptr());

        CUDA_SYNC_CHECK();

        pbar.step();
    }

    CUDA_CHECK(cudaMemcpy(tomogram.ptr(), devVolume.ptr(), sizeof(float) * nVoxels, cudaMemcpyDeviceToHost));

    // Free CUDA memories
    CUFFT_CHECK(cufftDestroy(planForward));
    CUFFT_CHECK(cufftDestroy(planInverse));

    return tomogram;
}
