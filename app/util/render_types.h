#pragma once
// SDL2/glm-native render types — no Raylib compat, no wrappers.
// Math aliases map legacy names to direct glm types.
// Mesh/Model/Material are CPU-side structs for software-3D rendering.

#include <SDL.h>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <cstdlib>

// ── Math type aliases ─────────────────────────────────────────────────────────
// Legacy code used Raylib Vector2/Vector3/Matrix.  These are direct glm aliases.
using Vector2 = glm::vec2;
using Vector3 = glm::vec3;
using Matrix  = glm::mat4;

// ── SDL color constants ───────────────────────────────────────────────────────
constexpr SDL_Color WHITE   = {255, 255, 255, 255};
constexpr SDL_Color BLANK   = {  0,   0,   0,   0};
constexpr SDL_Color RED     = {230,  41,  55, 255};
constexpr SDL_Color SKYBLUE = {102, 191, 255, 255};

// ── Math constant ─────────────────────────────────────────────────────────────
constexpr float PI = 3.14159265358979323846f;

// ── Texture filter hint ───────────────────────────────────────────────────────
// Passed to render_api::set_texture_filter; no-op in SDL2_Renderer path.
enum TextureFilter : int {
    TEXTURE_FILTER_POINT    = 0,
    TEXTURE_FILTER_BILINEAR = 1,
};

// ── CPU-side geometry (software 3D via SDL2) ──────────────────────────────────
// vertices: float[vertexCount*3]  (x,y,z per vertex)
// indices:  unsigned short[triangleCount*3]  (3 per triangle, or nullptr for sequential)
struct Mesh {
    float*          vertices      = nullptr;
    float*          normals       = nullptr;  // optional, not used by renderer
    unsigned short* indices       = nullptr;  // nullptr → use sequential vertices
    int             vertexCount   = 0;
    int             triangleCount = 0;
};

// Free heap-allocated Mesh data.  Safe to call on an already-freed Mesh.
inline void free_mesh_data(Mesh& mesh) noexcept {
    std::free(mesh.vertices);  mesh.vertices  = nullptr;
    std::free(mesh.normals);   mesh.normals   = nullptr;
    std::free(mesh.indices);   mesh.indices   = nullptr;
    mesh.vertexCount   = 0;
    mesh.triangleCount = 0;
}

// No-op material (SDL2 renderer uses SDL_Color directly — no GPU material state)
struct Material {};

// No-op shader handle (SDL_Renderer has no programmable shader support)
struct Shader {};

// 3D model aggregate — trivially copyable so EnTT can store it as a value component
struct Model {
    glm::mat4 transform{1.0f};
    int       meshCount     = 0;
    Mesh*     meshes        = nullptr;
    int       materialCount = 0;
    Material* materials     = nullptr;
    int*      meshMaterial  = nullptr;
    int       boneCount     = 0;
    void*     bones         = nullptr;
};

static_assert(std::is_trivially_copyable_v<Mesh>,
    "Mesh must be trivially copyable for EnTT storage");
static_assert(std::is_trivially_copyable_v<Model>,
    "Model must be trivially copyable for EnTT storage");
static_assert(std::is_standard_layout_v<Model>,
    "Model must be standard layout for C-interop with render functions");

// ── 2D texture / render-target handles ───────────────────────────────────────
struct Texture2D      { SDL_Texture* texture = nullptr; };
struct RenderTexture2D { SDL_Texture* texture = nullptr; };

// ── 2D rectangle ─────────────────────────────────────────────────────────────
struct Rectangle { float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f; };
