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

#include <string>

//
// Visual Studio's legacy <hash_map> implementation lives in namespace
// stdext but still makes use of the historical _STL namespace alias that
// was provided by Dinkumware's STL when STLPort was commonplace.  Modern
// versions of the compiler no longer declare that alias which leaves the
// header referencing an unknown namespace.  Some of our Windows builds
// also include STLPort which likewise omits the alias, so provide it
// unconditionally.
//
namespace _STL = std;
#endif // _MSC_VER

#include <hash_map>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // INCLUDED_HashMap_H

