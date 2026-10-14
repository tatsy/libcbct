target_sources(
  ${LIBCBCT}
  PRIVATE
  ReconstructionBase.h
  FilterUtils.cpp
  FilterUtils.h
  FeldKampCPU.cpp
  FeldkampCPU.h)

if (CUDA_FOUND)
  target_sources(
    ${LIBCTCT}
    PRIVATE
    FeldkampCUDA.cu
    FeldkampCUDA.h)
endif()
