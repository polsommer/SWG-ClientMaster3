// ======================================================================
//
// MemoryManager.h
// Portions copyright 1998 Bootprint Entertainment
// Portions copyright 2002 Sony Online Entertainment
// All Rights Reserved
//
// ======================================================================

#ifndef INCLUDED_MemoryManager_H
#define INCLUDED_MemoryManager_H

// ======================================================================

#if defined(__has_include)
#if __has_include("sharedDebug/DebugHelp.h")
#include "sharedDebug/DebugHelp.h"
#elif __has_include("sharedDebug/include/sharedDebug/DebugHelp.h")
#include "sharedDebug/include/sharedDebug/DebugHelp.h"
#elif __has_include("../../../sharedDebug/include/sharedDebug/DebugHelp.h")
#include "../../../sharedDebug/include/sharedDebug/DebugHelp.h"
#else
#error "Unable to locate sharedDebug/DebugHelp.h"
#endif
#else
// Older versions of MSVC do not provide __has_include, so fall back to the
// relative path that is valid within the engine source tree.
#include "../../../sharedDebug/include/sharedDebug/DebugHelp.h"
#endif

// ======================================================================

#include <new>
#if defined(__has_include)
#if __has_include("sharedMemoryManager/OsNewDel.h")
#include "sharedMemoryManager/OsNewDel.h"
#elif __has_include("sharedMemoryManager/include/sharedMemoryManager/OsNewDel.h")
#include "sharedMemoryManager/include/sharedMemoryManager/OsNewDel.h"
#elif __has_include("../../include/sharedMemoryManager/OsNewDel.h")
#include "../../include/sharedMemoryManager/OsNewDel.h"
#else
#error "Unable to locate sharedMemoryManager/OsNewDel.h"
#endif
#else
#include "sharedMemoryManager/OsNewDel.h"
#endif

// ======================================================================
// Memory manager class.
//
// This class API is multi-thread safe.
//
// This class provides extensive debugging features for applications, including
// overwrite guard bands, initialize pattern fills, free pattern fills, and 
// memory tracking.

class MemoryManager
{
public:

	MemoryManager();
	~MemoryManager();

	static void            setLimit(int megabytes, bool hardLimit, bool preallocate);
	static int             getLimit();
	static bool            isHardLimit();

	static void            registerDebugFlags();
	static void            debugReport();
	static void            debugReportMap();
	static bool            reportToFile(const char * fileName, bool leak);

	static int             getCurrentNumberOfAllocations();
	static unsigned long   getCurrentNumberOfBytesAllocated(const int processId = 0);
	static unsigned long   getCurrentNumberOfBytesAllocatedNoLeakTest();
	static int             getMaximumNumberOfAllocations();
	static unsigned long   getMaximumNumberOfBytesAllocated();
	static int             getSystemMemoryAllocatedMegabytes();

#ifndef _WIN32
	static int             getProcessVmSizeKBytes(const int processId = 0);
#endif

	static DLLEXPORT void *allocate(size_t size, uint32 owner, bool array, bool leakTest);
	static DLLEXPORT void  free(void *pointer, bool array);
	static DLLEXPORT void  own(void *pointer);
	static void *          reallocate(void *userPointer, size_t newSize);

	static void            verify(bool guardPatterns, bool freePatterns);
	static void            setReportAllocations(bool reportAllocations);
	static void            report();

private:

	// disabled
	MemoryManager(MemoryManager const &);
	MemoryManager &operator =(MemoryManager const &);
};

// ======================================================================

#ifdef _DEBUG
	#define MEM_OWN(a) MemoryManager::own(a)
#else
	#define MEM_OWN(a) UNREF(a)
#endif

// ======================================================================

#endif
