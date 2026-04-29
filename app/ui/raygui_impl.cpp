// raygui implementation translation unit
// This is the ONLY place in the entire project where RAYGUI_IMPLEMENTATION is defined
// to avoid ODR violations when raygui is included in multiple compilation units.

// Suppress warnings from third-party raygui code
#if defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wmissing-field-initializers"
    #pragma clang diagnostic ignored "-Wunused-parameter"
#elif defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    #pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4100) // unreferenced formal parameter
    #pragma warning(disable: 4458) // declaration hides class member
#endif

#define RAYGUI_IMPLEMENTATION
#include "vendor/raygui.h"

#if defined(__clang__)
    #pragma clang diagnostic pop
#elif defined(__GNUC__)
    #pragma GCC diagnostic pop
#elif defined(_MSC_VER)
    #pragma warning(pop)
#endif
