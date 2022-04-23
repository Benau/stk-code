/* ==========================================================================
 * Copyright (c) 2022 SuperTuxKart-Team
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ==========================================================================
 */
#ifndef HEADER_SIMD_WRAPPER_HPP
#define HEADER_SIMD_WRAPPER_HPP

#include <simde/simde-arch.h>
#if defined(SIMDE_ARCH_AMD64) || defined(SIMDE_ARCH_X86)
#define OC_NATIVE_SIMD 1
#endif

#ifndef OC_NATIVE_SIMD
// Using simde
#define SIMDE_ENABLE_NATIVE_ALIASES

#ifdef _MSC_VER
// Fix math related functions missing in msvc
#include <cmath>

// Workaround for MSVC ARM64 build, those static asserts failed:
// static_assert(!std::is_same<__m128, __m128i>::value, "__m128 and __m128i should not be the same");
// static_assert(!std::is_same<float32x4_t, int64x2_t>::value, "float32x4_t and int64x2_t should not be the same");
#ifdef _M_ARM64
#define SIMDE_NO_NATIVE 1
#endif

#endif

#include "simde/x86/svml.h"

#else

// Native simd
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <immintrin.h>
#endif

#endif

#ifndef _MM_FROUND_TO_NEG_INF
#define _MM_FROUND_TO_NEG_INF SIMDE_MM_FROUND_TO_NEG_INF
#endif

#ifndef _MM_FROUND_NO_EXC
#define _MM_FROUND_NO_EXC SIMDE_MM_FROUND_NO_EXC
#endif

#ifndef _MM_SET_ROUNDING_MODE
#define _MM_SET_ROUNDING_MODE _MM_SET_ROUNDING_MODE
#endif

#ifndef _MM_ROUND_NEAREST
#define _MM_ROUND_NEAREST SIMDE_MM_ROUND_NEAREST
#endif

#ifndef _MM_ROUND_UP
#define _MM_ROUND_UP SIMDE_MM_ROUND_UP
#endif

#ifndef _MM_ROUND_DOWN
#define _MM_ROUND_DOWN SIMDE_MM_ROUND_DOWN
#endif

#endif
