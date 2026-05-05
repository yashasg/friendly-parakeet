#include "runtime_types.h"

#include "platform/sdl2/sdl2_graphics_context.h"
#include "platform/sdl2/sdl2_headers.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

int g_trace_log_level = LOG_INFO;
TraceLogCallback g_trace_log_callback = nullptr;

int g_mouse_offset_x = 0;
int g_mouse_offset_y = 0;
float g_mouse_scale_x = 1.0f;
float g_mouse_scale_y = 1.0f;

bool g_last_mouse_down[3] = {false, false, false};

#if !defined(__EMSCRIPTEN__)
[[nodiscard]] int clamp_mouse_button_index(int button) {
    if (button < 0 || button > 2) return 0;
    return button;
}

[[nodiscard]] bool is_mouse_button_down_now(int button) {
    const Uint32 state = SDL_GetMouseState(nullptr, nullptr);
    switch (button) {
        case MOUSE_BUTTON_RIGHT: return (state & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
        case MOUSE_BUTTON_MIDDLE: return (state & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
        default: return (state & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    }
}

[[nodiscard]] std::uint16_t wave_format_tag(const Wave& wave) noexcept {
    return wave.sampleSize == 32 ? static_cast<std::uint16_t>(3u) : static_cast<std::uint16_t>(1u);
}

[[nodiscard]] std::uint32_t wave_data_size_bytes(const Wave& wave) noexcept {
    const std::uint32_t bytes_per_sample = wave.sampleSize / 8u;
    if (bytes_per_sample == 0u) return 0u;
    return wave.frameCount * wave.channels * bytes_per_sample;
}

void append_u16_le(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
}

void append_u32_le(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
}

[[nodiscard]] std::vector<std::uint8_t> make_wav_blob(const Wave& wave) {
    const std::uint32_t data_bytes = wave_data_size_bytes(wave);
    if (!wave.data || data_bytes == 0u || wave.channels == 0u || wave.sampleRate == 0u) return {};

    std::vector<std::uint8_t> wav;
    wav.reserve(static_cast<std::size_t>(44u + data_bytes));
    wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
    append_u32_le(wav, 36u + data_bytes);
    wav.insert(wav.end(), {'W', 'A', 'V', 'E'});
    wav.insert(wav.end(), {'f', 'm', 't', ' '});
    append_u32_le(wav, 16u);
    append_u16_le(wav, wave_format_tag(wave));
    append_u16_le(wav, static_cast<std::uint16_t>(wave.channels));
    append_u32_le(wav, wave.sampleRate);
    const std::uint16_t block_align = static_cast<std::uint16_t>(wave.channels * (wave.sampleSize / 8u));
    append_u32_le(wav, wave.sampleRate * block_align);
    append_u16_le(wav, block_align);
    append_u16_le(wav, static_cast<std::uint16_t>(wave.sampleSize));
    wav.insert(wav.end(), {'d', 'a', 't', 'a'});
    append_u32_le(wav, data_bytes);

    const auto* bytes = static_cast<const std::uint8_t*>(wave.data);
    wav.insert(wav.end(), bytes, bytes + data_bytes);
    return wav;
}

[[nodiscard]] SDL_Color to_sdl_color(Color color) noexcept {
    return SDL_Color{color.r, color.g, color.b, color.a};
}
#endif

[[nodiscard]] Matrix matrix_from_cols(const std::array<float, 16>& v) {
    Matrix m{};
    m.m0 = v[0]; m.m4 = v[4]; m.m8 = v[8]; m.m12 = v[12];
    m.m1 = v[1]; m.m5 = v[5]; m.m9 = v[9]; m.m13 = v[13];
    m.m2 = v[2]; m.m6 = v[6]; m.m10 = v[10]; m.m14 = v[14];
    m.m3 = v[3]; m.m7 = v[7]; m.m11 = v[11]; m.m15 = v[15];
    return m;
}

[[nodiscard]] std::array<float, 16> cols_from_matrix(Matrix m) {
    return {
        m.m0, m.m1, m.m2, m.m3,
        m.m4, m.m5, m.m6, m.m7,
        m.m8, m.m9, m.m10, m.m11,
        m.m12, m.m13, m.m14, m.m15,
    };
}

void draw_gl_rect(float x, float y, float w, float h, Color color, bool fill) {
    if (!platform::sdl2::is_ready()) return;
    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(fill ? GL_QUADS : GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

Mesh make_mesh(std::vector<float>&& vertices,
               std::vector<float>&& normals,
               std::vector<unsigned short>&& indices) {
    Mesh mesh{};
    mesh.vertexCount = static_cast<int>(vertices.size() / 3);
    mesh.triangleCount = static_cast<int>(indices.size() / 3);

    mesh.vertices = static_cast<float*>(std::malloc(vertices.size() * sizeof(float)));
    mesh.normals = static_cast<float*>(std::malloc(normals.size() * sizeof(float)));
    mesh.indices = static_cast<unsigned short*>(std::malloc(indices.size() * sizeof(unsigned short)));

    if (mesh.vertices && !vertices.empty()) {
        std::memcpy(mesh.vertices, vertices.data(), vertices.size() * sizeof(float));
    }
    if (mesh.normals && !normals.empty()) {
        std::memcpy(mesh.normals, normals.data(), normals.size() * sizeof(float));
    }
    if (mesh.indices && !indices.empty()) {
        std::memcpy(mesh.indices, indices.data(), indices.size() * sizeof(unsigned short));
    }
    return mesh;
}

}  // namespace

void SetTraceLogLevel(int level) {
    g_trace_log_level = level;
}

void SetTraceLogCallback(TraceLogCallback callback) {
    g_trace_log_callback = callback;
}

void TraceLog(int logLevel, const char* text, ...) {
    if (logLevel < g_trace_log_level || logLevel == LOG_NONE) return;

    va_list args;
    va_start(args, text);
    if (g_trace_log_callback) {
        g_trace_log_callback(logLevel, text, args);
    } else {
        std::vfprintf(stderr, text, args);
        std::fprintf(stderr, "\n");
    }
    va_end(args);
}

bool FileExists(const char* fileName) {
    if (!fileName) return false;
    std::error_code ec;
    return std::filesystem::exists(fileName, ec);
}

const char* GetApplicationDirectory() {
    static std::string path = std::filesystem::current_path().string() + "/";
    return path.c_str();
}

bool IsWindowReady() {
    return platform::sdl2::is_ready();
}

void SetMouseOffset(int offsetX, int offsetY) {
    g_mouse_offset_x = offsetX;
    g_mouse_offset_y = offsetY;
}

void SetMouseScale(float scaleX, float scaleY) {
    g_mouse_scale_x = (scaleX == 0.0f) ? 1.0f : scaleX;
    g_mouse_scale_y = (scaleY == 0.0f) ? 1.0f : scaleY;
}

Vector2 GetMousePosition() {
    int x = 0;
    int y = 0;
    SDL_GetMouseState(&x, &y);
    return {
        (static_cast<float>(x) + static_cast<float>(g_mouse_offset_x)) * g_mouse_scale_x,
        (static_cast<float>(y) + static_cast<float>(g_mouse_offset_y)) * g_mouse_scale_y,
    };
}

float GetMouseWheelMove() {
    return 0.0f;
}

bool IsMouseButtonDown(int button) {
#if defined(__EMSCRIPTEN__)
    (void)button;
    return false;
#else
    return is_mouse_button_down_now(button);
#endif
}

bool IsMouseButtonPressed(int button) {
#if defined(__EMSCRIPTEN__)
    (void)button;
    return false;
#else
    const int idx = clamp_mouse_button_index(button);
    const bool down = is_mouse_button_down_now(button);
    const bool pressed = down && !g_last_mouse_down[idx];
    g_last_mouse_down[idx] = down;
    return pressed;
#endif
}

bool IsMouseButtonReleased(int button) {
#if defined(__EMSCRIPTEN__)
    (void)button;
    return false;
#else
    const int idx = clamp_mouse_button_index(button);
    const bool down = is_mouse_button_down_now(button);
    const bool released = !down && g_last_mouse_down[idx];
    g_last_mouse_down[idx] = down;
    return released;
#endif
}

bool IsKeyDown(int key) {
    const Uint8* state = SDL_GetKeyboardState(nullptr);
    if (!state) return false;
    return key >= 0 && key < SDL_NUM_SCANCODES && state[key] != 0;
}

bool IsKeyPressed(int key) {
    return platform::sdl2::input_key_pressed(key);
}

int GetCharPressed() {
    return platform::sdl2::input_consume_char_pressed();
}

void BeginMode2D(Camera2D /*camera*/) {
#if !defined(__EMSCRIPTEN__)
    if (!platform::sdl2::is_ready()) return;
    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const float w = static_cast<float>(viewport[2]);
    const float h = static_cast<float>(viewport[3]);
    if (w <= 0.0f || h <= 0.0f) return;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(w), static_cast<double>(h), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
#endif
}

void EndMode2D() {
#if !defined(__EMSCRIPTEN__)
    if (!platform::sdl2::is_ready()) return;
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
#endif
}

void ClearBackground(Color color) {
    if (!platform::sdl2::is_ready()) return;
    // UI pass clears with BLANK to keep world visible underneath. Treat a fully
    // transparent clear as a no-op on the default framebuffer.
    if (color.a == 0) return;
    constexpr float kScale = 1.0f / 255.0f;
    glClearColor(static_cast<float>(color.r) * kScale,
                 static_cast<float>(color.g) * kScale,
                 static_cast<float>(color.b) * kScale,
                 static_cast<float>(color.a) * kScale);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void BeginScissorMode(int x, int y, int width, int height) {
    if (!platform::sdl2::is_ready()) return;
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width, height);
}

void EndScissorMode() {
    if (!platform::sdl2::is_ready()) return;
    glDisable(GL_SCISSOR_TEST);
}

int GetScreenWidth() {
    return platform::sdl2::screen_width();
}

int GetScreenHeight() {
    return platform::sdl2::screen_height();
}

double GetTime() {
#if defined(__EMSCRIPTEN__)
    return static_cast<double>(SDL_GetTicks()) / 1000.0;
#else
    return static_cast<double>(SDL_GetTicks64()) / 1000.0;
#endif
}

void DrawRectangle(int posX, int posY, int width, int height, Color color) {
    draw_gl_rect(static_cast<float>(posX), static_cast<float>(posY), static_cast<float>(width), static_cast<float>(height), color, true);
}

void DrawRectangleRec(Rectangle rec, Color color) {
    draw_gl_rect(rec.x, rec.y, rec.width, rec.height, color, true);
}

void DrawRectangleGradientEx(Rectangle rec, Color col1, Color col2, Color col3, Color col4) {
    if (!platform::sdl2::is_ready()) return;
    glBegin(GL_QUADS);
    glColor4ub(col1.r, col1.g, col1.b, col1.a);
    glVertex2f(rec.x, rec.y);
    glColor4ub(col2.r, col2.g, col2.b, col2.a);
    glVertex2f(rec.x + rec.width, rec.y);
    glColor4ub(col3.r, col3.g, col3.b, col3.a);
    glVertex2f(rec.x + rec.width, rec.y + rec.height);
    glColor4ub(col4.r, col4.g, col4.b, col4.a);
    glVertex2f(rec.x, rec.y + rec.height);
    glEnd();
}

void DrawRectangleGradientV(int posX, int posY, int width, int height, Color top, Color bottom) {
    DrawRectangleGradientEx(
        {static_cast<float>(posX), static_cast<float>(posY), static_cast<float>(width), static_cast<float>(height)},
        top, top, bottom, bottom);
}

void DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color) {
    if (!platform::sdl2::is_ready()) return;
    glLineWidth(lineThick);
    draw_gl_rect(rec.x, rec.y, rec.width, rec.height, color, false);
    glLineWidth(1.0f);
}

void DrawRectangleRounded(Rectangle rec, float /*roundness*/, int /*segments*/, Color color) {
    DrawRectangleRec(rec, color);
}

void DrawRectangleRoundedLinesEx(Rectangle rec, float /*roundness*/, int /*segments*/, float lineThick, Color color) {
    DrawRectangleLinesEx(rec, lineThick, color);
}

void DrawLine(int startPosX, int startPosY, int endPosX, int endPosY, Color color) {
    if (!platform::sdl2::is_ready()) return;
    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(GL_LINES);
    glVertex2f(static_cast<float>(startPosX), static_cast<float>(startPosY));
    glVertex2f(static_cast<float>(endPosX), static_cast<float>(endPosY));
    glEnd();
}

void DrawLineV(Vector2 startPos, Vector2 endPos, Color color) {
    DrawLine(static_cast<int>(startPos.x), static_cast<int>(startPos.y),
             static_cast<int>(endPos.x), static_cast<int>(endPos.y), color);
}

void DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color) {
    if (!platform::sdl2::is_ready()) return;
    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(GL_TRIANGLES);
    glVertex2f(v1.x, v1.y);
    glVertex2f(v2.x, v2.y);
    glVertex2f(v3.x, v3.y);
    glEnd();
}

void DrawCircleV(Vector2 center, float radius, Color color) {
    if (!platform::sdl2::is_ready()) return;
    constexpr int segments = 24;
    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(center.x, center.y);
    for (int i = 0; i <= segments; ++i) {
        float a = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segments);
        glVertex2f(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius);
    }
    glEnd();
}

void DrawCircleLinesV(Vector2 center, float radius, Color color) {
    if (!platform::sdl2::is_ready()) return;
    constexpr int segments = 24;
    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        float a = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segments);
        glVertex2f(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius);
    }
    glEnd();
}

RenderTexture2D LoadRenderTexture(int width, int height) {
    RenderTexture2D rt{};
#if !defined(__EMSCRIPTEN__)
    if (!platform::sdl2::is_ready() || width <= 0 || height <= 0) return rt;

    GLuint texture_id = 0;
    GLuint framebuffer_id = 0;
    GLuint depthbuffer_id = 0;

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenRenderbuffers(1, &depthbuffer_id);
    glBindRenderbuffer(GL_RENDERBUFFER, depthbuffer_id);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &framebuffer_id);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthbuffer_id);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        if (framebuffer_id != 0) glDeleteFramebuffers(1, &framebuffer_id);
        if (depthbuffer_id != 0) glDeleteRenderbuffers(1, &depthbuffer_id);
        if (texture_id != 0) glDeleteTextures(1, &texture_id);
        return {};
    }

    rt.id = framebuffer_id;
    rt.texture.id = texture_id;
    rt.texture.width = width;
    rt.texture.height = height;
    rt.texture.mipmaps = 1;
    rt.depth.id = depthbuffer_id;
    rt.depth.width = width;
    rt.depth.height = height;
    rt.depth.mipmaps = 1;
#else
    rt.texture.width = width;
    rt.texture.height = height;
#endif
    return rt;
}

void UnloadRenderTexture(RenderTexture2D target) {
#if !defined(__EMSCRIPTEN__)
    if (target.id != 0u) {
        const GLuint framebuffer_id = static_cast<GLuint>(target.id);
        glDeleteFramebuffers(1, &framebuffer_id);
    }
    if (target.depth.id != 0u) {
        const GLuint depthbuffer_id = static_cast<GLuint>(target.depth.id);
        glDeleteRenderbuffers(1, &depthbuffer_id);
    }
    if (target.texture.id != 0u) {
        const GLuint texture_id = static_cast<GLuint>(target.texture.id);
        glDeleteTextures(1, &texture_id);
    }
#else
    (void)target;
#endif
}

void SetTextureFilter(Texture2D texture, int filter) {
#if !defined(__EMSCRIPTEN__)
    if (texture.id == 0u) return;
    GLint gl_filter = GL_LINEAR;
    if (filter != TEXTURE_FILTER_BILINEAR) {
        gl_filter = GL_NEAREST;
    }
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture.id));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter);
    glBindTexture(GL_TEXTURE_2D, 0);
#else
    (void)texture;
    (void)filter;
#endif
}

Shader LoadShaderFromMemory(const char* /*vsCode*/, const char* /*fsCode*/) {
    return {};
}

Material LoadMaterialDefault() {
    return {};
}

void UnloadMaterial(Material material) {
    if (material.shader.locs) {
        std::free(material.shader.locs);
    }
    if (material.maps) {
        std::free(material.maps);
    }
}

Mesh GenMeshCube(float width, float height, float length) {
    const float hw = width * 0.5f;
    const float hh = height * 0.5f;
    const float hl = length * 0.5f;

    std::vector<float> v = {
        -hw,-hh,-hl,  hw,-hh,-hl,  hw, hh,-hl, -hw, hh,-hl,
        -hw,-hh, hl,  hw,-hh, hl,  hw, hh, hl, -hw, hh, hl,
    };
    std::vector<float> n(v.size(), 0.0f);
    std::vector<unsigned short> idx = {
        0,1,2, 2,3,0,
        4,5,6, 6,7,4,
        0,4,7, 7,3,0,
        1,5,6, 6,2,1,
        3,2,6, 6,7,3,
        0,1,5, 5,4,0,
    };
    return make_mesh(std::move(v), std::move(n), std::move(idx));
}

Mesh GenMeshPlane(float width, float length, int /*resX*/, int /*resZ*/) {
    const float hw = width * 0.5f;
    const float hl = length * 0.5f;

    std::vector<float> v = {
        -hw,0.0f,-hl,
         hw,0.0f,-hl,
         hw,0.0f, hl,
        -hw,0.0f, hl,
    };
    std::vector<float> n(v.size(), 0.0f);
    std::vector<unsigned short> idx = {0,1,2, 2,3,0};
    return make_mesh(std::move(v), std::move(n), std::move(idx));
}

Mesh GenMeshCylinder(float radius, float height, int slices) {
    const int ring = std::max(slices, 3);
    const float hh = height * 0.5f;

    std::vector<float> v;
    std::vector<float> n;
    std::vector<unsigned short> idx;

    for (int i = 0; i < ring; ++i) {
        const float a = 2.0f * PI * static_cast<float>(i) / static_cast<float>(ring);
        const float x = std::cos(a) * radius;
        const float z = std::sin(a) * radius;
        v.push_back(x); v.push_back(-hh); v.push_back(z);
        v.push_back(x); v.push_back(hh); v.push_back(z);
        n.push_back(0.0f); n.push_back(0.0f); n.push_back(0.0f);
        n.push_back(0.0f); n.push_back(0.0f); n.push_back(0.0f);
    }

    for (int i = 0; i < ring; ++i) {
        const int next = (i + 1) % ring;
        const unsigned short b0 = static_cast<unsigned short>(i * 2);
        const unsigned short t0 = static_cast<unsigned short>(i * 2 + 1);
        const unsigned short b1 = static_cast<unsigned short>(next * 2);
        const unsigned short t1 = static_cast<unsigned short>(next * 2 + 1);

        idx.push_back(b0); idx.push_back(t0); idx.push_back(b1);
        idx.push_back(t0); idx.push_back(t1); idx.push_back(b1);
    }

    return make_mesh(std::move(v), std::move(n), std::move(idx));
}

void UnloadMesh(Mesh mesh) {
    std::free(mesh.vertices);
    std::free(mesh.texcoords);
    std::free(mesh.normals);
    std::free(mesh.indices);
    std::free(mesh.vboId);
}

void UnloadModel(Model model) {
    if (model.meshes) {
        for (int i = 0; i < model.meshCount; ++i) {
            UnloadMesh(model.meshes[i]);
        }
        std::free(model.meshes);
    }
    if (model.materials) {
        for (int i = 0; i < model.materialCount; ++i) {
            UnloadMaterial(model.materials[i]);
        }
        std::free(model.materials);
    }
    std::free(model.meshMaterial);
}

Matrix MatrixIdentity() {
    return {};
}

Matrix MatrixMultiply(Matrix left, Matrix right) {
    const auto a = cols_from_matrix(left);
    const auto b = cols_from_matrix(right);
    std::array<float, 16> r{};

    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a[k * 4 + row] * b[col * 4 + k];
            }
            r[col * 4 + row] = sum;
        }
    }

    return matrix_from_cols(r);
}

Matrix MatrixTranslate(float x, float y, float z) {
    Matrix m = MatrixIdentity();
    m.m12 = x;
    m.m13 = y;
    m.m14 = z;
    return m;
}

Matrix MatrixScale(float x, float y, float z) {
    Matrix m = MatrixIdentity();
    m.m0 = x;
    m.m5 = y;
    m.m10 = z;
    return m;
}

Matrix MatrixRotateY(float angle) {
    Matrix m = MatrixIdentity();
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    m.m0 = c;
    m.m8 = s;
    m.m2 = -s;
    m.m10 = c;
    return m;
}

Matrix MatrixOrtho(double left, double right, double bottom, double top, double znear, double zfar) {
    Matrix m{};
    m.m0 = static_cast<float>(2.0 / (right - left));
    m.m5 = static_cast<float>(2.0 / (top - bottom));
    m.m10 = static_cast<float>(-2.0 / (zfar - znear));
    m.m12 = static_cast<float>(-(right + left) / (right - left));
    m.m13 = static_cast<float>(-(top + bottom) / (top - bottom));
    m.m14 = static_cast<float>(-(zfar + znear) / (zfar - znear));
    m.m15 = 1.0f;
    return m;
}

Matrix MatrixPerspective(double fovY, double aspect, double znear, double zfar) {
    const double f = 1.0 / std::tan(fovY * 0.5);
    Matrix m{};
    m.m0 = static_cast<float>(f / aspect);
    m.m5 = static_cast<float>(f);
    m.m10 = static_cast<float>((zfar + znear) / (znear - zfar));
    m.m11 = -1.0f;
    m.m14 = static_cast<float>((2.0 * zfar * znear) / (znear - zfar));
    m.m15 = 0.0f;
    return m;
}

Matrix MatrixLookAt(Vector3 eye, Vector3 target, Vector3 up) {
    Vector3 z{};
    z.x = eye.x - target.x;
    z.y = eye.y - target.y;
    z.z = eye.z - target.z;
    const float z_len = std::sqrt(z.x * z.x + z.y * z.y + z.z * z.z);
    if (z_len > 0.0f) {
        z.x /= z_len;
        z.y /= z_len;
        z.z /= z_len;
    }

    Vector3 x{};
    x.x = up.y * z.z - up.z * z.y;
    x.y = up.z * z.x - up.x * z.z;
    x.z = up.x * z.y - up.y * z.x;
    const float x_len = std::sqrt(x.x * x.x + x.y * x.y + x.z * x.z);
    if (x_len > 0.0f) {
        x.x /= x_len;
        x.y /= x_len;
        x.z /= x_len;
    }

    Vector3 y{};
    y.x = z.y * x.z - z.z * x.y;
    y.y = z.z * x.x - z.x * x.z;
    y.z = z.x * x.y - z.y * x.x;

    Matrix m = MatrixIdentity();
    m.m0 = x.x; m.m4 = x.y; m.m8 = x.z; m.m12 = -(x.x * eye.x + x.y * eye.y + x.z * eye.z);
    m.m1 = y.x; m.m5 = y.y; m.m9 = y.z; m.m13 = -(y.x * eye.x + y.y * eye.y + y.z * eye.z);
    m.m2 = z.x; m.m6 = z.y; m.m10 = z.z; m.m14 = -(z.x * eye.x + z.y * eye.y + z.z * eye.z);
    return m;
}

const float* MatrixToFloat(Matrix mat) {
    thread_local std::array<float, 16> out{};
    out = cols_from_matrix(mat);
    return out.data();
}

Vector3 Vector3Transform(Vector3 v, Matrix mat) {
    return {
        mat.m0 * v.x + mat.m4 * v.y + mat.m8 * v.z + mat.m12,
        mat.m1 * v.x + mat.m5 * v.y + mat.m9 * v.z + mat.m13,
        mat.m2 * v.x + mat.m6 * v.y + mat.m10 * v.z + mat.m14,
    };
}

float Clamp(float value, float min_value, float max_value) {
    return std::clamp(value, min_value, max_value);
}

float Lerp(float start, float end, float amount) {
    return start + amount * (end - start);
}

Color Fade(Color color, float alpha) {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    color.a = static_cast<unsigned char>(std::round(alpha * 255.0f));
    return color;
}

Color GetColor(unsigned int hexValue) {
    return {
        static_cast<unsigned char>((hexValue >> 24) & 0xffu),
        static_cast<unsigned char>((hexValue >> 16) & 0xffu),
        static_cast<unsigned char>((hexValue >> 8) & 0xffu),
        static_cast<unsigned char>(hexValue & 0xffu),
    };
}

Vector2 MeasureTextEx(Font font, const char* text, float fontSize, float spacing) {
    if (!text) return {};
#if !defined(__EMSCRIPTEN__)
    if (font.handle) {
        int w = 0;
        int h = 0;
        if (TTF_SizeUTF8(static_cast<TTF_Font*>(font.handle), text, &w, &h) == 0) {
            const float scale = (font.baseSize > 0) ? fontSize / static_cast<float>(font.baseSize) : 1.0f;
            const std::size_t len = std::strlen(text);
            const float extra_spacing = len > 0 ? static_cast<float>(len - 1) * spacing : 0.0f;
            return {static_cast<float>(w) * scale + extra_spacing, static_cast<float>(h) * scale};
        }
    }
#else
    (void)font;
#endif
    const std::size_t len = std::strlen(text);
    return {static_cast<float>(len) * (fontSize * 0.6f + spacing), fontSize};
}

void DrawTextEx(Font font, const char* text, Vector2 position, float fontSize, float /*spacing*/, Color tint) {
#if !defined(__EMSCRIPTEN__)
    if (!text || !text[0] || !font.handle || !platform::sdl2::is_ready()) return;

    SDL_Surface* rendered = TTF_RenderUTF8_Blended(static_cast<TTF_Font*>(font.handle), text, to_sdl_color(tint));
    if (!rendered) return;
    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(rendered, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(rendered);
    if (!rgba) return;

    const float scale = (font.baseSize > 0) ? fontSize / static_cast<float>(font.baseSize) : 1.0f;
    const float width = static_cast<float>(rgba->w) * scale;
    const float height = static_cast<float>(rgba->h) * scale;

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 rgba->w,
                 rgba->h,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 rgba->pixels);
    SDL_FreeSurface(rgba);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glColor4ub(255, 255, 255, 255);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(position.x, position.y);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(position.x + width, position.y);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(position.x + width, position.y + height);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(position.x, position.y + height);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &texture);
#else
    (void)font;
    (void)text;
    (void)position;
    (void)fontSize;
    (void)tint;
#endif
}

Font LoadFontEx(const char* fileName, int fontSize, int* /*codepoints*/, int /*codepointCount*/) {
    Font font{};
#if !defined(__EMSCRIPTEN__)
    if (!fileName || !FileExists(fileName)) return font;
    if (!TTF_WasInit() && TTF_Init() != 0) return font;
    TTF_Font* loaded = TTF_OpenFont(fileName, fontSize);
    if (!loaded) return font;
    font.baseSize = fontSize;
    font.handle = loaded;
#else
    (void)fileName;
    (void)fontSize;
#endif
    return font;
}

void UnloadFont(Font font) {
#if !defined(__EMSCRIPTEN__)
    if (font.handle) {
        TTF_CloseFont(static_cast<TTF_Font*>(font.handle));
    }
#else
    (void)font;
#endif
}

Font GetFontDefault() {
    Font font = LoadFontEx("content/fonts/LiberationMono-Regular.ttf", 16, nullptr, 0);
    if (font.baseSize == 0) {
        font.baseSize = 16;
    }
    return font;
}

const char* TextFormat(const char* text, ...) {
    thread_local char buffer[256];
    va_list args;
    va_start(args, text);
    std::vsnprintf(buffer, sizeof(buffer), text, args);
    va_end(args);
    return buffer;
}

int TextToInteger(const char* text) {
    if (!text) return 0;
    return std::atoi(text);
}

int GetCodepointNext(const char* text, int* codepointSize) {
    if (codepointSize) *codepointSize = 1;
    if (!text || text[0] == '\0') return 0;
    return static_cast<unsigned char>(text[0]);
}

int GetCodepointPrevious(const char* text, int* codepointSize) {
    if (codepointSize) *codepointSize = 1;
    if (!text || text[0] == '\0') return 0;
    return static_cast<unsigned char>(text[0]);
}

const char* CodepointToUTF8(int codepoint, int* byteSize) {
    thread_local char utf8[5];
    utf8[0] = static_cast<char>(codepoint & 0xff);
    utf8[1] = '\0';
    if (byteSize) *byteSize = 1;
    return utf8;
}

int GetGlyphIndex(Font /*font*/, int /*codepoint*/) {
    return 0;
}

int ColorToInt(Color color) {
    return (static_cast<int>(color.r) << 24)
         | (static_cast<int>(color.g) << 16)
         | (static_cast<int>(color.b) << 8)
         | static_cast<int>(color.a);
}

bool CheckCollisionPointRec(Vector2 point, Rectangle rec) {
    return point.x >= rec.x && point.x <= (rec.x + rec.width)
        && point.y >= rec.y && point.y <= (rec.y + rec.height);
}

bool CheckCollisionRecs(Rectangle rec1, Rectangle rec2) {
    return rec1.x < rec2.x + rec2.width && rec1.x + rec1.width > rec2.x
        && rec1.y < rec2.y + rec2.height && rec1.y + rec1.height > rec2.y;
}

Vector2 GetWorldToScreenEx(Vector3 position, Camera3D camera, int width, int height) {
    const Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
    const float aspect = (height > 0) ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    const Matrix proj = (camera.projection == CAMERA_ORTHOGRAPHIC)
        ? MatrixOrtho(-camera.fovy * 0.5f * aspect, camera.fovy * 0.5f * aspect,
                      -camera.fovy * 0.5f, camera.fovy * 0.5f, 0.01, 10000.0)
        : MatrixPerspective(camera.fovy * DEG2RAD, aspect, 0.01, 10000.0);

    const Matrix vp = MatrixMultiply(view, MatrixIdentity());
    const Vector3 view_pos = Vector3Transform(position, vp);
    const std::array<float, 4> p = {
        proj.m0 * view_pos.x + proj.m4 * view_pos.y + proj.m8 * view_pos.z + proj.m12,
        proj.m1 * view_pos.x + proj.m5 * view_pos.y + proj.m9 * view_pos.z + proj.m13,
        proj.m2 * view_pos.x + proj.m6 * view_pos.y + proj.m10 * view_pos.z + proj.m14,
        proj.m3 * view_pos.x + proj.m7 * view_pos.y + proj.m11 * view_pos.z + proj.m15,
    };

    if (p[3] == 0.0f) return {};
    const float inv_w = 1.0f / p[3];
    const float ndc_x = p[0] * inv_w;
    const float ndc_y = p[1] * inv_w;

    return {
        (ndc_x + 1.0f) * 0.5f * static_cast<float>(width),
        (1.0f - ndc_y) * 0.5f * static_cast<float>(height),
    };
}

Wave LoadWave(const char* fileName) {
    Wave wave{};
    if (!fileName) return wave;
#if !defined(__EMSCRIPTEN__)
    SDL_AudioSpec spec{};
    Uint8* data = nullptr;
    Uint32 length = 0;
    if (!SDL_LoadWAV(fileName, &spec, &data, &length)) return wave;

    const unsigned int bytes_per_sample = static_cast<unsigned int>(SDL_AUDIO_BITSIZE(spec.format) / 8);
    if (bytes_per_sample == 0 || spec.channels == 0) {
        SDL_FreeWAV(data);
        return wave;
    }

    wave.sampleRate = static_cast<unsigned int>(spec.freq);
    wave.sampleSize = static_cast<unsigned int>(bytes_per_sample * 8);
    wave.channels = static_cast<unsigned int>(spec.channels);
    wave.frameCount = length / (bytes_per_sample * wave.channels);
    wave.data = data;
#else
    (void)fileName;
#endif
    return wave;
}

void UnloadWave(Wave wave) {
#if !defined(__EMSCRIPTEN__)
    SDL_FreeWAV(static_cast<Uint8*>(wave.data));
#else
    std::free(wave.data);
#endif
}

float* LoadWaveSamples(Wave wave) {
    if (!wave.data || wave.channels == 0 || wave.frameCount == 0) return nullptr;
    const size_t sample_count = static_cast<size_t>(wave.frameCount) * static_cast<size_t>(wave.channels);
    float* out = static_cast<float*>(std::calloc(sample_count, sizeof(float)));
    if (!out) return nullptr;

    if (wave.sampleSize == 8) {
        const auto* in = static_cast<const std::uint8_t*>(wave.data);
        for (size_t i = 0; i < sample_count; ++i) {
            out[i] = (static_cast<float>(in[i]) - 128.0f) / 128.0f;
        }
    } else if (wave.sampleSize == 16) {
        const auto* in = static_cast<const std::int16_t*>(wave.data);
        for (size_t i = 0; i < sample_count; ++i) {
            out[i] = static_cast<float>(in[i]) / 32768.0f;
        }
    } else if (wave.sampleSize == 32) {
        std::memcpy(out, wave.data, sample_count * sizeof(float));
    } else {
        std::free(out);
        return nullptr;
    }
    return out;
}

void UnloadWaveSamples(float* samples) {
    std::free(samples);
}

Sound LoadSoundFromWave(Wave wave) {
    Sound sound{};
#if !defined(__EMSCRIPTEN__)
    const std::vector<std::uint8_t> wav = make_wav_blob(wave);
    if (wav.empty()) return sound;
    SDL_RWops* rw = SDL_RWFromConstMem(wav.data(), static_cast<int>(wav.size()));
    if (!rw) return sound;
    Mix_Chunk* chunk = Mix_LoadWAV_RW(rw, 1);
    if (!chunk) return sound;

    sound.frameCount = wave.frameCount;
    sound.valid = true;
    sound.handle = chunk;
#else
    sound.frameCount = wave.frameCount;
    sound.valid = wave.frameCount > 0 && wave.data != nullptr;
#endif
    return sound;
}

bool IsSoundValid(Sound sound) {
#if defined(__EMSCRIPTEN__)
    return sound.valid;
#else
    return sound.valid && sound.handle != nullptr;
#endif
}

void PlaySound(Sound sound) {
#if !defined(__EMSCRIPTEN__)
    if (!sound.valid || !sound.handle) return;
    Mix_PlayChannel(-1, static_cast<Mix_Chunk*>(sound.handle), 0);
#else
    (void)sound;
#endif
}

void UnloadSound(Sound sound) {
#if !defined(__EMSCRIPTEN__)
    if (!sound.handle) return;
    Mix_FreeChunk(static_cast<Mix_Chunk*>(sound.handle));
#else
    (void)sound;
#endif
}

Music LoadMusicStream(const char* fileName) {
    Music music{};
#if !defined(__EMSCRIPTEN__)
    if (!fileName || !FileExists(fileName)) return music;
    Mix_Music* loaded = Mix_LoadMUS(fileName);
    if (!loaded) return music;
    music.frameCount = 1;
    music.handle = loaded;
#else
    if (FileExists(fileName)) music.frameCount = 1;
#endif
    return music;
}

void UnloadMusicStream(Music music) {
#if !defined(__EMSCRIPTEN__)
    if (music.handle) {
        Mix_FreeMusic(static_cast<Mix_Music*>(music.handle));
    }
#else
    (void)music;
#endif
}

void PlayMusicStream(Music music) {
#if !defined(__EMSCRIPTEN__)
    if (!music.handle) return;
    Mix_PlayMusic(static_cast<Mix_Music*>(music.handle), music.looping ? -1 : 0);
#else
    (void)music;
#endif
}

void PauseMusicStream(Music /*music*/) {
#if !defined(__EMSCRIPTEN__)
    if (!Mix_PlayingMusic() || Mix_PausedMusic() != 0) return;
    Mix_PauseMusic();
#endif
}

void ResumeMusicStream(Music /*music*/) {
#if !defined(__EMSCRIPTEN__)
    if (Mix_PausedMusic() == 0) return;
    Mix_ResumeMusic();
#endif
}

void StopMusicStream(Music /*music*/) {
#if !defined(__EMSCRIPTEN__)
    Mix_HaltMusic();
#endif
}

void UpdateMusicStream(Music /*music*/) {
}

void SetMusicVolume(Music /*music*/, float volume) {
#if !defined(__EMSCRIPTEN__)
    const int mixer_volume = static_cast<int>(std::round(std::clamp(volume, 0.0f, 1.0f) * MIX_MAX_VOLUME));
    Mix_VolumeMusic(mixer_volume);
#else
    (void)volume;
#endif
}

float GetMusicTimePlayed(Music /*music*/) {
    return 0.0f;
}
