// ======================================================================
//
// FoundationTypes.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#ifndef INCLUDED_FoundationTypes_H
#define INCLUDED_FoundationTypes_H

// ======================================================================

#include <cstdint>
#include <cstdio>

// ======================================================================
// Platform detection

#if defined(_WIN32) || defined(WIN32)
#ifndef PLATFORM_WIN32
#define PLATFORM_WIN32
#endif
#elif defined(__linux__) || defined(linux) || defined(__linux)
#ifndef PLATFORM_UNIX
#define PLATFORM_UNIX
#endif
#ifndef PLATFORM_LINUX
#define PLATFORM_LINUX
#endif
#else
#error unsupported platform
#endif

// ======================================================================

typedef unsigned char          byte;
typedef unsigned int           uint;
typedef unsigned long          ulong;
typedef unsigned short         ushort;

// ======================================================================

// Integer typedefs used throughout the codebase.  The original project
// provided separate platform specific headers to supply these typedefs.
// Using the standard <cstdint> definitions keeps the existing type names
// while ensuring they are always available regardless of platform or
// toolchain configuration.

typedef std::uint8_t           uint8;
typedef std::uint16_t          uint16;
typedef std::uint32_t          uint32;
typedef std::uint64_t          uint64;

typedef std::int8_t            int8;
typedef std::int16_t           int16;
typedef std::int32_t           int32;
typedef std::int64_t           int64;

#if defined(PLATFORM_WIN32)
typedef int                    FILE_HANDLE;
#else
typedef FILE*                  FILE_HANDLE;
#endif

#endif

