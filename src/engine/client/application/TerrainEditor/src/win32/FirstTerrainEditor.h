//
// FirstTerrainEditor.h
// asommers 
//
// copyright 2000, verant interactive
//

//-------------------------------------------------------------------

#ifndef INCLUDED_FirstTerrainEditor_H
#define INCLUDED_FirstTerrainEditor_H

//-------------------------------------------------------------------

//#define NOMINMAX

#include "sharedFoundation/WinApiFamily.h"

// The TerrainEditor tool was originally written for an ANSI build of MFC.
// Modern Windows SDK headers default to Unicode which changes many common
// typedefs (LPCTSTR, etc.) to their wide-character equivalents.  The legacy
// editor codebase, however, expects the ANSI variants.  Explicitly disable
// the Unicode macros here so the included headers match the expectations of
// the existing source.
#ifdef UNICODE
#undef UNICODE
#endif
#ifdef _UNICODE
#undef _UNICODE
#endif

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

//-------------------------------------------------------------------

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxcview.h>
#include <afxdisp.h>        // MFC Automation classes
#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls

#include <ddraw.h>

#include <afxbutton.h>

#if defined(_MSC_VER)
// Visual Studio's MFC headers emit spurious C4183 warnings when
// afxvisualmanagerwindows.h pulls in afxcontrolbarutil.h.  Wrap the includes
// in a warning scope so the tool's build output stays clean regardless of the
// exact _MFC_VER that is defined by the toolchain.
#pragma warning(push)
#pragma warning(disable:4183)
#endif
#include <afxvisualmanager.h>
#include <afxvisualmanagerwindows.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

//-------------------------------------------------------------------

#include "sharedFoundation/FirstSharedFoundation.h"
#include "NumberEdit.h"
//-------------------------------------------------------------------

#endif 
