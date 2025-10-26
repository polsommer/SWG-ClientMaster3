// This is a part of the Active Template Library.
// Copyright (C) Microsoft Corporation
// All rights reserved.
//
// This source code is only intended as a supplement to the
// Active Template Library Reference and related
// electronic documentation provided with the library.
// See these sources for detailed information regarding the	
// Active Template Library product.

#define _ATL_STATIC_LIB_IMPL
#define _ATL_DEBUG_INTERFACES

#if defined(_WIN32)
//
// The upstream ATL headers ship with Windows SDKs that default to the App
// partition when WINAPI_FAMILY is undefined.  The legacy MFC tools in this
// repository, including the TerrainEditor, rely on unrestricted desktop APIs
// such as <atlimage.h>.  When the partition is not forced to the desktop
// family the ATL headers trigger a fatal C1189 diagnostic that halts the
// build.  Mirror the project-wide WinApiFamily shim here so the standalone ATL
// static library can be built without depending on the application's
// precompiled header.
//
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

#include <atlbase.h>
#include <atlcom.h>
#include <atlwin.h>
#include <atlimage.h>
#include <atlcomtime.h>
#include <atlmem.h>
#include <atlstr.h>
#include <atltime.h>

#include <atldebugapi.h>
