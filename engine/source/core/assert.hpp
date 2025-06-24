#pragma once
#include <cstdlib>
#include <stacktrace>

#include "log_system.hpp"

#undef CNE_UNREACHABLE
#define CNE_UNREACHABLE() std::abort()

#define CNE_ASSERT(TRUTH) do { \
    if (!(TRUTH)) { \
        CNE_CRITICAL("assertion failed in file '{}' line {}, expression: '{}'", \
            __FILE__, __LINE__, #TRUTH); \
        CNE_UNREACHABLE(); \
    } \
} while (false)
#define CNE_ASSERT_WITH(TRUTH, msg) do { \
    if (!(TRUTH)) { \
        CNE_CRITICAL("{} \n (file: '{}' line: {})", msg, __FILE__, __LINE__); \
        CNE_UNREACHABLE(); \
    } \
} while (false)
