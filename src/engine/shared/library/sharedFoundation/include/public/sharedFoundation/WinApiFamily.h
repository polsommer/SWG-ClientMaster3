// ============================================================================
//
// WinApiFamily.h
// Copyright 2023 Sony Online Entertainment
//
// ============================================================================

#ifndef INCLUDED_WinApiFamily_H
#define INCLUDED_WinApiFamily_H

// Recent Windows SDKs partition the API surface by defaulting WINAPI_FAMILY to
// the App partition.  The legacy tooling in this repository was written against
// the unrestricted desktop APIs and depends on ATL/MFC headers such as
// <atlimage.h>.  Those headers are excluded from the App family which leads to
// the compiler emitting fatal C1189 errors unless the desktop intent is stated
// explicitly.  Centralise the partition override here so every Windows-specific
// "First" header can opt in with a single include.  Some SDK revisions gate the
// ATL image helper behind _ATL_ALLOW_WINRT_INCOMPATIBLE_CLASSES even when the
// compilation is known to target the desktop partition, so provide that opt-in
// alongside the partition overrides.

#if defined(_WIN32)
#if !defined(_ATL_ALLOW_WINRT_INCOMPATIBLE_CLASSES)
#define _ATL_ALLOW_WINRT_INCOMPATIBLE_CLASSES 1
#endif

// winapifamily.h first appeared with the Windows 8 SDK which shipped with
// Visual Studio 2012 (_MSC_VER == 1700).  Older SDKs neither provide the
// header nor use the partition macros so guard the include accordingly.
#if defined(_MSC_VER) && (_MSC_VER >= 1700)
#include <winapifamily.h>
#endif

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY != WINAPI_FAMILY_DESKTOP_APP)
#undef WINAPI_FAMILY
#endif

#if !defined(WINAPI_FAMILY)
#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP
#endif

//
// Some Windows SDKs ship a <winapifamily.h> implementation where the helper
// macro evaluates the supplied partition mask directly against
// WINAPI_FAMILY.  If the toolchain defined WINAPI_FAMILY to an App-centric
// value before this header is parsed, simply redefining the macro above is not
// sufficient—the existing family partition helpers will continue to report that
// the compilation unit targets the App partition.  Guard against that scenario
// by forcing the helper to recognise the desktop mask.
//
#if defined(WINAPI_FAMILY_PARTITION) && defined(WINAPI_PARTITION_DESKTOP)
#undef WINAPI_FAMILY_PARTITION
#define WINAPI_FAMILY_PARTITION(Partition) (((Partition) & WINAPI_PARTITION_DESKTOP) != 0)
#endif

#if !defined(WINAPI_PARTITION_DESKTOP)
#define WINAPI_PARTITION_DESKTOP 0x00000001
#endif

#if !defined(WINAPI_FAMILY_PARTITION)
#define WINAPI_FAMILY_PARTITION(Partition) (((Partition) & WINAPI_PARTITION_DESKTOP) != 0)
#endif

#endif // defined(_WIN32)

#endif // INCLUDED_WinApiFamily_H

// ============================================================================
