// ============================================================================
//
// HashMap.h
// Copyright (c) 2024 Sony Online Entertainment
//
// A compatibility shim for the legacy std::hash_map container.  Visual Studio
// 2013 still ships <hash_map> but the header pulls in a large amount of
// Dinkumware implementation detail that conflicts with STLPort, which is used
// by parts of this code base.  Rather than rely on that implementation we map
// the old container aliases onto std::unordered_map while preserving the
// existing include surface.
//
// ============================================================================

#ifndef INCLUDED_HashMap_H
#define INCLUDED_HashMap_H

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4159 4183)

#include <unordered_map>

namespace std
{
    /**
     * Visual Studio 2013 still ships the deprecated <hash_map> header but
     * the implementation is tightly coupled to the legacy Dinkumware
     * internals and does not compile cleanly when STLPort headers are also
     * present.  Provide aliases that map the old container names onto the
     * C++11 equivalents so existing code can continue to include
     * sharedFoundation/HashMap.h without pulling in <hash_map>.
     */
    template <typename _Kty, typename _Ty, typename _Hasher = std::hash<_Kty>, typename _Keyeq = std::equal_to<_Kty>, typename _Alloc = std::allocator<std::pair<const _Kty, _Ty> > >
    using hash_map = std::unordered_map<_Kty, _Ty, _Hasher, _Keyeq, _Alloc>;

    template <typename _Kty, typename _Ty, typename _Hasher = std::hash<_Kty>, typename _Keyeq = std::equal_to<_Kty>, typename _Alloc = std::allocator<std::pair<const _Kty, _Ty> > >
    using hash_multimap = std::unordered_multimap<_Kty, _Ty, _Hasher, _Keyeq, _Alloc>;
}

#pragma warning(pop)
#else
#include <hash_map>
#endif // _MSC_VER

#endif // INCLUDED_HashMap_H

