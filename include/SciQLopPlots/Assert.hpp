#pragma once

#include <cstdio>
#include <cstdlib>

// SQP_ASSERT — debug-only invariant check.
//
// In debug builds (NDEBUG not defined): prints diagnostic and calls std::abort().
// In release builds: the expression is not evaluated, no runtime cost.
//
// Use for *internal* invariants that should never fail if the boundary-layer
// validation is correct. Do NOT use for user-input validation — throw an
// exception from Validation.hpp instead.

#ifndef NDEBUG
#define SQP_ASSERT(expr)                                                       \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::fprintf(stderr, "SQP_ASSERT failed: %s\n  at %s:%d in %s\n",  \
                         #expr, __FILE__, __LINE__, __func__);                 \
            std::abort();                                                      \
        }                                                                      \
    } while (0)
#else
#define SQP_ASSERT(expr) ((void)0)
#endif
