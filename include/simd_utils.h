#ifndef SIMD_UTILS_H
#define SIMD_UTILS_H

#ifdef USE_SIMD

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    #include <arm_neon.h>
    #define SIMD_NEON 1
#elif defined(__AVX2__)
    #include <immintrin.h>
    #define SIMD_AVX2 1
    #define SIMD_SSE2 1
#elif defined(__SSE2__)
    #include <immintrin.h>
    #define SIMD_SSE2 1
#endif

#endif // USE_SIMD

#endif // SIMD_UTILS_H
