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
// "First" header can opt in with a single include.

#if defined(_WIN32)
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

#endif // defined(_WIN32)

#endif // INCLUDED_WinApiFamily_H

// ============================================================================
