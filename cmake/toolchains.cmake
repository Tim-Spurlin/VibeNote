# Toolchain configuration for VibeNote
# Sets compilers, build types and paths for an Arch Linux + Zen kernel environment.

include_guard(GLOBAL)

# Allow overriding the default compilers through environment variables
if(DEFINED ENV{CXX} AND NOT CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER "$ENV{CXX}" CACHE FILEPATH "C++ compiler" FORCE)
endif()
if(DEFINED ENV{CC} AND NOT CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER "$ENV{CC}" CACHE FILEPATH "C compiler" FORCE)
endif()

# Default to Release builds when not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Common compile and link flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -pthread")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--as-needed")

# Allow overriding Qt and KF6 locations via environment variables
if(DEFINED ENV{QT6_DIR})
  list(PREPEND CMAKE_PREFIX_PATH "$ENV{QT6_DIR}")
endif()
if(DEFINED ENV{KF6_DIR})
  list(PREPEND CMAKE_PREFIX_PATH "$ENV{KF6_DIR}")
endif()

# Standard search prefixes for Arch Linux
list(APPEND CMAKE_PREFIX_PATH "/usr" "/usr/lib/qt6" "/usr/lib/cmake/Qt6" "/usr/lib/cmake/KF6")

# Optional CUDA configuration for PaddleOCR acceleration
if(ENABLE_PADDLE_OCR)
  set(CMAKE_CUDA_COMPILER "/opt/cuda/bin/nvcc" CACHE FILEPATH "CUDA compiler" FORCE)
  set(CMAKE_CUDA_ARCHITECTURES "75;86" CACHE STRING "CUDA architectures" FORCE)
endif()

