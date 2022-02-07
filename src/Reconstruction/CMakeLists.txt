target_sources(
  ${LIBCBCT}
  PRIVATE
  ReconstructionBase.h
  FeldKampCPU.cpp
  FeldkampCPU.h)

if (CUDA_FOUND)
  target_sources(
    ${LIBCTCT}
    PRIVATE
    FeldkampCUDA.cu
    FeldkampCUDA.h)
endif()
