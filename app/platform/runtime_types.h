#pragma once

#include <array>
#include <cstdarg>
#include <cstddef>
#include <cstdlib>
#include <cstdint>

// Core constants
constexpr float PI = 3.14159265358979323846f;
constexpr float DEG2RAD = PI / 180.0f;

// Logging levels
constexpr int LOG_ALL = 0;
constexpr int LOG_TRACE = 1;
constexpr int LOG_DEBUG = 2;
constexpr int LOG_INFO = 3;
constexpr int LOG_WARNING = 4;
constexpr int LOG_ERROR = 5;
constexpr int LOG_FATAL = 6;
constexpr int LOG_NONE = 7;

using TraceLogCallback = void (*)(int, const char*, va_list);

struct Vector2 { float x = 0.0f; float y = 0.0f; };
struct Vector3 { float x = 0.0f; float y = 0.0f; float z = 0.0f; };
struct Vector4 { float x = 0.0f; float y = 0.0f; float z = 0.0f; float w = 0.0f; };
struct Rectangle { float x = 0.0f; float y = 0.0f; float width = 0.0f; float height = 0.0f; };
struct Color { unsigned char r = 0; unsigned char g = 0; unsigned char b = 0; unsigned char a = 0; };

struct Matrix {
    float m0 = 1.0f, m4 = 0.0f, m8 = 0.0f, m12 = 0.0f;
    float m1 = 0.0f, m5 = 1.0f, m9 = 0.0f, m13 = 0.0f;
    float m2 = 0.0f, m6 = 0.0f, m10 = 1.0f, m14 = 0.0f;
    float m3 = 0.0f, m7 = 0.0f, m11 = 0.0f, m15 = 1.0f;
};

struct Camera2D {
    Vector2 offset{};
    Vector2 target{};
    float rotation = 0.0f;
    float zoom = 1.0f;
};

constexpr int CAMERA_PERSPECTIVE = 0;
constexpr int CAMERA_ORTHOGRAPHIC = 1;

struct Camera3D {
    Vector3 position{};
    Vector3 target{};
    Vector3 up{0.0f, 1.0f, 0.0f};
    float fovy = 60.0f;
    int projection = CAMERA_PERSPECTIVE;
};

struct Texture {
    unsigned int id = 0;
    int width = 0;
    int height = 0;
    int mipmaps = 1;
    int format = 0;
};
using Texture2D = Texture;

struct RenderTexture {
    unsigned int id = 0;
    Texture texture{};
    Texture depth{};
};
using RenderTexture2D = RenderTexture;

struct Shader {
    unsigned int id = 0;
    int* locs = nullptr;
};

struct MaterialMap {
    Texture2D texture{};
    Color color{255, 255, 255, 255};
    float value = 0.0f;
};

struct Material {
    Shader shader{};
    MaterialMap* maps = nullptr;
    float params[4] = {0, 0, 0, 0};
};

struct Mesh {
    int vertexCount = 0;
    int triangleCount = 0;
    float* vertices = nullptr;
    float* texcoords = nullptr;
    float* normals = nullptr;
    unsigned short* indices = nullptr;
    unsigned int vaoId = 0;
    unsigned int* vboId = nullptr;
};

struct Model {
    Matrix transform{};
    int meshCount = 0;
    int materialCount = 0;
    Mesh* meshes = nullptr;
    Material* materials = nullptr;
    int* meshMaterial = nullptr;
    int boneCount = 0;
    void* bones = nullptr;
    void* bindPose = nullptr;
};

struct Wave {
    unsigned int frameCount = 0;
    unsigned int sampleRate = 0;
    unsigned int sampleSize = 0;
    unsigned int channels = 0;
    void* data = nullptr;
};

struct Sound {
    unsigned int frameCount = 0;
    bool valid = false;
};

struct Music {
    bool looping = false;
    unsigned int frameCount = 0;
};

struct GlyphInfo {
    int value = 0;
    int offsetX = 0;
    int offsetY = 0;
    int advanceX = 0;
    void* image = nullptr;
};

struct Font {
    int baseSize = 0;
    int glyphCount = 0;
    int glyphPadding = 0;
    Texture2D texture{};
    Rectangle* recs = nullptr;
    GlyphInfo* glyphs = nullptr;
};

constexpr int TEXTURE_FILTER_BILINEAR = 1;

constexpr int GESTURE_NONE = 0;
constexpr int GESTURE_TAP = 1;
constexpr int GESTURE_DOUBLETAP = 2;
constexpr int GESTURE_HOLD = 4;
constexpr int GESTURE_DRAG = 8;
constexpr int GESTURE_SWIPE_RIGHT = 16;
constexpr int GESTURE_SWIPE_LEFT = 32;
constexpr int GESTURE_SWIPE_UP = 64;
constexpr int GESTURE_SWIPE_DOWN = 128;
constexpr int GESTURE_PINCH_IN = 256;
constexpr int GESTURE_PINCH_OUT = 512;

constexpr int MOUSE_BUTTON_LEFT = 0;
constexpr int MOUSE_BUTTON_RIGHT = 1;
constexpr int MOUSE_BUTTON_MIDDLE = 2;
constexpr int MOUSE_LEFT_BUTTON = MOUSE_BUTTON_LEFT;
constexpr int MOUSE_RIGHT_BUTTON = MOUSE_BUTTON_RIGHT;
constexpr int MOUSE_MIDDLE_BUTTON = MOUSE_BUTTON_MIDDLE;

constexpr int KEY_NULL = 0;
constexpr int KEY_APOSTROPHE = 52;
constexpr int KEY_COMMA = 54;
constexpr int KEY_MINUS = 45;
constexpr int KEY_PERIOD = 55;
constexpr int KEY_SLASH = 56;
constexpr int KEY_ZERO = 39;
constexpr int KEY_ONE = 30;
constexpr int KEY_TWO = 31;
constexpr int KEY_THREE = 32;
constexpr int KEY_FOUR = 33;
constexpr int KEY_FIVE = 34;
constexpr int KEY_SIX = 35;
constexpr int KEY_SEVEN = 36;
constexpr int KEY_EIGHT = 37;
constexpr int KEY_NINE = 38;
constexpr int KEY_SEMICOLON = 51;
constexpr int KEY_EQUAL = 46;
constexpr int KEY_A = 4;
constexpr int KEY_B = 5;
constexpr int KEY_C = 6;
constexpr int KEY_D = 7;
constexpr int KEY_E = 8;
constexpr int KEY_F = 9;
constexpr int KEY_G = 10;
constexpr int KEY_H = 11;
constexpr int KEY_I = 12;
constexpr int KEY_J = 13;
constexpr int KEY_K = 14;
constexpr int KEY_L = 15;
constexpr int KEY_M = 16;
constexpr int KEY_N = 17;
constexpr int KEY_O = 18;
constexpr int KEY_P = 19;
constexpr int KEY_Q = 20;
constexpr int KEY_R = 21;
constexpr int KEY_S = 22;
constexpr int KEY_T = 23;
constexpr int KEY_U = 24;
constexpr int KEY_V = 25;
constexpr int KEY_W = 26;
constexpr int KEY_X = 27;
constexpr int KEY_Y = 28;
constexpr int KEY_Z = 29;
constexpr int KEY_SPACE = 44;
constexpr int KEY_ESCAPE = 41;
constexpr int KEY_ENTER = 40;
constexpr int KEY_TAB = 43;
constexpr int KEY_BACKSPACE = 42;
constexpr int KEY_INSERT = 73;
constexpr int KEY_DELETE = 76;
constexpr int KEY_HOME = 74;
constexpr int KEY_END = 77;
constexpr int KEY_RIGHT = 79;
constexpr int KEY_LEFT = 80;
constexpr int KEY_DOWN = 81;
constexpr int KEY_UP = 82;
constexpr int KEY_LEFT_SHIFT = 225;
constexpr int KEY_RIGHT_SHIFT = 229;
constexpr int KEY_LEFT_CONTROL = 224;
constexpr int KEY_RIGHT_CONTROL = 228;

constexpr Color WHITE{255, 255, 255, 255};
constexpr Color BLACK{0, 0, 0, 255};
constexpr Color RED{230, 41, 55, 255};
constexpr Color SKYBLUE{102, 191, 255, 255};
constexpr Color BLANK{0, 0, 0, 0};

#ifndef RL_MALLOC
#define RL_MALLOC(sz) std::malloc(sz)
#endif
#ifndef RL_CALLOC
#define RL_CALLOC(n,sz) std::calloc(n,sz)
#endif

void SetTraceLogLevel(int level);
void SetTraceLogCallback(TraceLogCallback callback);
void TraceLog(int logLevel, const char* text, ...);

bool FileExists(const char* fileName);
const char* GetApplicationDirectory();

bool IsWindowReady();

void SetMouseOffset(int offsetX, int offsetY);
void SetMouseScale(float scaleX, float scaleY);
Vector2 GetMousePosition();
float GetMouseWheelMove();
bool IsMouseButtonDown(int button);
bool IsMouseButtonPressed(int button);
bool IsMouseButtonReleased(int button);

bool IsKeyDown(int key);
bool IsKeyPressed(int key);
int GetCharPressed();
void SetMouseCursor(int cursor);

int GetTouchPointCount();
Vector2 GetTouchPosition(int index);
int GetGestureDetected();
bool IsGestureDetected(unsigned int gesture);
Vector2 GetGestureDragVector();

void BeginMode2D(Camera2D camera);
void EndMode2D();
void ClearBackground(Color color);

void BeginScissorMode(int x, int y, int width, int height);
void EndScissorMode();
int GetScreenWidth();
int GetScreenHeight();

void DrawRectangle(int posX, int posY, int width, int height, Color color);
void DrawRectangleRec(Rectangle rec, Color color);
void DrawRectangleGradientEx(Rectangle rec, Color col1, Color col2, Color col3, Color col4);
void DrawRectangleGradientV(int posX, int posY, int width, int height, Color top, Color bottom);
void DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color);
void DrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color);
void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments, float lineThick, Color color);
void DrawLine(int startPosX, int startPosY, int endPosX, int endPosY, Color color);
void DrawLineV(Vector2 startPos, Vector2 endPos, Color color);
void DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color);
void DrawCircleV(Vector2 center, float radius, Color color);
void DrawCircleLinesV(Vector2 center, float radius, Color color);

RenderTexture2D LoadRenderTexture(int width, int height);
void UnloadRenderTexture(RenderTexture2D target);
void SetTextureFilter(Texture2D texture, int filter);

Shader LoadShaderFromMemory(const char* vsCode, const char* fsCode);
Material LoadMaterialDefault();
void UnloadMaterial(Material material);

Mesh GenMeshCube(float width, float height, float length);
Mesh GenMeshPlane(float width, float length, int resX, int resZ);
Mesh GenMeshCylinder(float radius, float height, int slices);
void UnloadMesh(Mesh mesh);

void UnloadModel(Model model);

Matrix MatrixIdentity();
Matrix MatrixMultiply(Matrix left, Matrix right);
Matrix MatrixTranslate(float x, float y, float z);
Matrix MatrixScale(float x, float y, float z);
Matrix MatrixRotateY(float angle);
Matrix MatrixOrtho(double left, double right, double bottom, double top, double znear, double zfar);
Matrix MatrixPerspective(double fovY, double aspect, double znear, double zfar);
Matrix MatrixLookAt(Vector3 eye, Vector3 target, Vector3 up);
const float* MatrixToFloat(Matrix mat);
Vector3 Vector3Transform(Vector3 v, Matrix mat);

float Clamp(float value, float min_value, float max_value);
float Lerp(float start, float end, float amount);
Color Fade(Color color, float alpha);
Color GetColor(unsigned int hexValue);

Vector2 MeasureTextEx(Font font, const char* text, float fontSize, float spacing);
void DrawTextEx(Font font, const char* text, Vector2 position, float fontSize, float spacing, Color tint);
Font LoadFontEx(const char* fileName, int fontSize, int* codepoints, int codepointCount);
void UnloadFont(Font font);
Font GetFontDefault();
const char* TextFormat(const char* text, ...);
int TextToInteger(const char* text);
int GetCodepointNext(const char* text, int* codepointSize);
int GetCodepointPrevious(const char* text, int* codepointSize);
const char* CodepointToUTF8(int codepoint, int* byteSize);
int GetGlyphIndex(Font font, int codepoint);

int ColorToInt(Color color);

bool CheckCollisionPointRec(Vector2 point, Rectangle rec);
bool CheckCollisionRecs(Rectangle rec1, Rectangle rec2);

Vector2 GetWorldToScreenEx(Vector3 position, Camera3D camera, int width, int height);

Wave LoadWave(const char* fileName);
void UnloadWave(Wave wave);
float* LoadWaveSamples(Wave wave);
void UnloadWaveSamples(float* samples);

Sound LoadSoundFromWave(Wave wave);
bool IsSoundValid(Sound sound);
void PlaySound(Sound sound);
void UnloadSound(Sound sound);

Music LoadMusicStream(const char* fileName);
void UnloadMusicStream(Music music);
void PlayMusicStream(Music music);
void PauseMusicStream(Music music);
void ResumeMusicStream(Music music);
void StopMusicStream(Music music);
void UpdateMusicStream(Music music);
void SetMusicVolume(Music music, float volume);
float GetMusicTimePlayed(Music music);
