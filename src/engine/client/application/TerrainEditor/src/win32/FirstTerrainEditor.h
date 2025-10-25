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

#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

//-------------------------------------------------------------------

#include "sharedFoundation/FirstSharedFoundation.h"
#include "NumberEdit.h"
//-------------------------------------------------------------------

#endif 
