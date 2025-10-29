#include "Direct3d9ExSupport.h"

namespace
{
    // Intentionally empty translation unit to ensure the Direct3D 9Ex helper
    // declarations participate in the build as a static library. Visual Studio
    // ignores header-only projects, so we provide a single TU that simply
    // includes the compatibility definitions.
}
