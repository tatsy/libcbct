// Batched radix-2 FFT for the power-of-two padded detector rows.
// One invocation owns each butterfly; separate dispatches provide stage barriers.

struct FftPrepareRealParameters {
    StructuredBuffer<float> padded;
    RWStructuredBuffer<float2> spectrum;
    int length;
    int detHeight;
    int bitCount;
};

struct FftPrepareComplexParameters {
    StructuredBuffer<float2> input;
    RWStructuredBuffer<float2> output;
    int length;
    int detHeight;
    int bitCount;
};

struct FftStageParameters {
    RWStructuredBuffer<float2> spectrum;
    int length;
    int detHeight;
    int halfSpan;
    int inverse;
};

struct CropFilteredParameters {
    StructuredBuffer<float2> padded;
    RWStructuredBuffer<float> filtered;
    int length;
    int detWidth;
    int detHeight;
};

[shader("compute")]
[numthreads(16, 16, 1)]
void fftPrepareRealKernel(uint3 threadID : SV_DispatchThreadID, uniform FftPrepareRealParameters params) {
    if (threadID.x >= uint(params.length) || threadID.y >= uint(params.detHeight)) return;
    uint reversed = reversebits(threadID.x) >> (32 - params.bitCount);
    uint row = threadID.y * uint(params.length);
    params.spectrum[row + reversed] = float2(params.padded[row + threadID.x], 0.0f);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void fftPrepareComplexKernel(uint3 threadID : SV_DispatchThreadID, uniform FftPrepareComplexParameters params) {
    if (threadID.x >= uint(params.length) || threadID.y >= uint(params.detHeight)) return;
    uint reversed = reversebits(threadID.x) >> (32 - params.bitCount);
    uint row = threadID.y * uint(params.length);
    params.output[row + reversed] = params.input[row + threadID.x];
}

[shader("compute")]
[numthreads(16, 16, 1)]
void fftStageKernel(uint3 threadID : SV_DispatchThreadID, uniform FftStageParameters params) {
    if (threadID.x >= uint(params.length / 2) || threadID.y >= uint(params.detHeight)) return;
    uint j = threadID.x % uint(params.halfSpan);
    uint group = threadID.x / uint(params.halfSpan);
    uint first = threadID.y * uint(params.length) + group * uint(2 * params.halfSpan) + j;
    uint second = first + uint(params.halfSpan);
    float angle = (params.inverse != 0 ? 1.0f : -1.0f) * 3.14159265358979323846f * float(j) / float(params.halfSpan);
    float sine, cosine;
    sincos(angle, sine, cosine);
    float2 a = params.spectrum[first];
    float2 b = params.spectrum[second];
    float2 rotated = float2(cosine * b.x - sine * b.y, sine * b.x + cosine * b.y);
    params.spectrum[first] = a + rotated;
    params.spectrum[second] = a - rotated;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void cropFilteredKernel(uint3 threadID : SV_DispatchThreadID, uniform CropFilteredParameters params) {
    if (threadID.x >= uint(params.detWidth) || threadID.y >= uint(params.detHeight)) return;
    params.filtered[threadID.y * uint(params.detWidth) + threadID.x] =
        params.padded[threadID.y * uint(params.length) + threadID.x].x;
}
