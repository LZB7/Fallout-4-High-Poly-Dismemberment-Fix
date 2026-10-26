#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <climits>
#include <cstring>
#include <cassert>
using UInt8=std::uint8_t; using UInt16=std::uint16_t; using UInt32=DWORD;
using UInt64=std::uint64_t; using SInt32=std::int32_t;
#define ASSERT(x) assert(x)
#define STATIC_ASSERT(x) static_assert(x)
#define _ERROR(...) ((void)0)
#define _WARNING(...) ((void)0)
#define _MESSAGE(...) ((void)0)
#ifndef _I32_MIN
#define _I32_MIN INT32_MIN
#define _I32_MAX INT32_MAX
#endif
