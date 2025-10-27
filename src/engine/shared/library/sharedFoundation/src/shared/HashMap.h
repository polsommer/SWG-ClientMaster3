// ============================================================================
//
// HashMap.h
// Copyright (c) 2024 Sony Online Entertainment
//
// A thin wrapper around the legacy <hash_map> header that suppresses
// warnings emitted by the Visual C++ standard library when the header is
// included.  Older code in this code base still relies on std::hash_map, and
// Visual Studio 2013 generates warning C4183 (missing return type) and C4159
// (mismatched #pragma pack) from <xhash>.  Both warnings originate inside the
// implementation header and cannot be resolved without replacing the
// container usage entirely.  Centralising the suppression keeps call sites
// warning-free while we continue to support the existing data structures.
//
// ============================================================================

#ifndef INCLUDED_HashMap_H
#define INCLUDED_HashMap_H

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4159 4183)
#endif

#include <hash_map>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // INCLUDED_HashMap_H

