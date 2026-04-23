#include "camera_system.h"
#include "../components/shape_mesh.h"
#include "../components/rendering.h"
#include "../constants.h"
#include "../platform_display.h"
#include <raylib.h>
#include <rlgl.h>
#include <algorithm>

namespace camera {

ShapeMeshes build_shape_meshes() {
    ShapeMeshes sm = {};
    for (int i = 0; i < 4; ++i) {
        sm.shapes[i] = build_prism(SHAPE_TABLE[i]);
        UploadMesh(&sm.shapes[i], false);
    }
    sm.slab = build_unit_slab();
    UploadMesh(&sm.slab, false);
    sm.quad = build_unit_quad();
    UploadMesh(&sm.quad, false);
    sm.material = LoadMaterialDefault();
    return sm;
}

void unload_shape_meshes(ShapeMeshes& sm) {
    for (int i = 0; i < 4; ++i)
        UnloadMesh(sm.shapes[i]);
    UnloadMesh(sm.slab);
    UnloadMesh(sm.quad);
    UnloadMaterial(sm.material);
}

void update_screen_transform(entt::registry& reg) {
    float win_w, win_h;
    platform_get_display_size(win_w, win_h);

    float scale = std::min(
        win_w / static_cast<float>(constants::SCREEN_W),
        win_h / static_cast<float>(constants::SCREEN_H));
    float dst_w = constants::SCREEN_W * scale;
    float dst_h = constants::SCREEN_H * scale;
    auto& st     = reg.ctx().get<ScreenTransform>();
    st.offset_x  = (win_w - dst_w) * 0.5f;
    st.offset_y  = (win_h - dst_h) * 0.5f;
    st.scale     = scale;
}

} // namespace camera
