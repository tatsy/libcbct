// FDK compute kernels shared by CUDA/PTX and SPIR-V targets.
// Host-side FFT planning, normalization, and projection ordering stay unchanged.

struct FdkGeometry {
    int2 detSize;
    float2 pixSize;
    int3 volSize;
    float sod;
    float sdd;
};

struct WeightAndPadParameters {
    StructuredBuffer<float> projection;
    StructuredBuffer<float> weights;
    RWStructuredBuffer<float> padded;
    int detWidth;
    int detHeight;
    int nPad;
};

struct FilterParameters {
    RWStructuredBuffer<float2> spectrum;
    StructuredBuffer<float> response;
    int nFreq;
    int detHeight;
};

struct BackProjectionParameters {
    StructuredBuffer<float> filtered;
    RWStructuredBuffer<float> volume;
    int detWidth;
    int detHeight;
    FdkGeometry geometry;
    float theta;
    float scale;
};

float3 projectPoint(float3 xyz, float theta, FdkGeometry geometry) {
    float cosTheta = cos(theta);
    float sinTheta = sin(theta);
    float L = xyz.x * cosTheta - xyz.y * sinTheta + geometry.sod;
    float vy = xyz.x * sinTheta + xyz.y * cosTheta;
    float u = vy * geometry.sdd / L;
    float v = xyz.z * geometry.sdd / L;
    float w = (geometry.sod / L) * (geometry.sod / L);
    float2 detectorUV = float2(u, v) / geometry.pixSize + float2(geometry.detSize) * 0.5f;
    return float3(detectorUV, w);
}

float3 voxelToPixel(int3 voxel, float theta, FdkGeometry geometry) {
    float3 center = float3(geometry.volSize) * 0.5f - 0.5f;
    float voxelSize = (geometry.detSize.x * geometry.pixSize.x) *
                      (geometry.sod / geometry.sdd) / geometry.volSize.x;
    return projectPoint((float3(voxel) - center) * voxelSize, theta, geometry);
}

float bilinearSample(StructuredBuffer<float> image, int width, int height, float x, float y) {
    int x0 = clamp(int(floor(x)), 0, width - 2);
    int y0 = clamp(int(floor(y)), 0, height - 2);
    // Keep the original extrapolation in the half-pixel border region.
    float u = x - x0;
    float v = y - y0;
    int base = y0 * width + x0;
    float v00 = image[base];
    float v01 = image[base + 1];
    float v10 = image[base + width];
    float v11 = image[base + width + 1];
    float v0 = fma(u, v01 - v00, v00);
    float v1 = fma(u, v11 - v10, v10);
    return fma(v, v1 - v0, v0);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void weightAndPadKernel(uint3 threadID : SV_DispatchThreadID, uniform WeightAndPadParameters params) {
    int x = int(threadID.x);
    int y = int(threadID.y);
    if (x >= params.nPad || y >= params.detHeight) {
        return;
    }
    float value = 0.0f;
    if (x < params.detWidth) {
        int index = y * params.detWidth + x;
        value = params.projection[index] * params.weights[index];
    }
    params.padded[y * params.nPad + x] = value;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void applyFilterKernel(uint3 threadID : SV_DispatchThreadID, uniform FilterParameters params) {
    int k = int(threadID.x);
    int y = int(threadID.y);
    if (k >= params.nFreq || y >= params.detHeight) {
        return;
    }
    int index = y * params.nFreq + k;
    // float2 stores real and imaginary components, matching cufftComplex.
    float2 value = params.spectrum[index];
    value.x *= params.response[k];
    value.y *= params.response[k];
    params.spectrum[index] = value;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void backProjectionKernel(uint3 threadID : SV_DispatchThreadID, uniform BackProjectionParameters params) {
    int3 voxel = int3(threadID);
    if (voxel.x >= params.geometry.volSize.x || voxel.y >= params.geometry.volSize.y ||
        voxel.z >= params.geometry.volSize.z) {
        return;
    }
    float3 uvw = voxelToPixel(voxel, params.theta, params.geometry);
    if (uvw.x >= 0.0f && uvw.y >= 0.0f && uvw.x < params.detWidth && uvw.y < params.detHeight) {
        float value = bilinearSample(params.filtered, params.detWidth, params.detHeight,
                                     uvw.x - 0.5f, uvw.y - 0.5f);
        // Preserve CUDA's size_t arithmetic for large volumes.
        uint64_t index = (uint64_t(voxel.z) * uint64_t(params.geometry.volSize.y) + uint64_t(voxel.y)) *
                         uint64_t(params.geometry.volSize.x) + uint64_t(voxel.x);
        params.volume[index] += value * uvw.z * params.scale;
    }
}
