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
typedef std::uint8_t           uint8;

#ifdef uint16
#undef uint16
#endif
typedef std::uint16_t          uint16;

#ifdef uint32
#undef uint32
#endif
typedef std::uint32_t          uint32;

#ifdef uint64
#undef uint64
#endif
typedef std::uint64_t          uint64;

#ifdef int8
#undef int8
#endif
typedef std::int8_t            int8;

#ifdef int16
#undef int16
#endif
typedef std::int16_t           int16;

#ifdef int32
#undef int32
#endif
typedef std::int32_t           int32;

#ifdef int64
#undef int64
#endif
typedef std::int64_t           int64;

#ifdef FILE_HANDLE
#undef FILE_HANDLE
#endif
typedef int                    FILE_HANDLE;

// ======================================================================

#endif
