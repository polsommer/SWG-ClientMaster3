// PRIVATE.  Do not export this header file outside the package.

// ======================================================================
//
// FoundationTypesWin32.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#ifndef INCLUDED_FoundationTypesWin32_H
#define INCLUDED_FoundationTypesWin32_H

// ======================================================================
// specify what platform we're running on.

#define PLATFORM_WIN32

// ======================================================================
// basic types that we assume to be around

#include <cstdint>

#ifdef uint8
#undef uint8
#endif
using uint8 = std::uint8_t;

#ifdef uint16
#undef uint16
#endif
using uint16 = std::uint16_t;

#ifdef uint32
#undef uint32
#endif
using uint32 = std::uint32_t;

#ifdef uint64
#undef uint64
#endif
using uint64 = std::uint64_t;

#ifdef int8
#undef int8
#endif
using int8 = std::int8_t;

#ifdef int16
#undef int16
#endif
using int16 = std::int16_t;

#ifdef int32
#undef int32
#endif
using int32 = std::int32_t;

#ifdef int64
#undef int64
#endif
using int64 = std::int64_t;

#ifdef FILE_HANDLE
#undef FILE_HANDLE
#endif
using FILE_HANDLE = int;

// ======================================================================

#endif
