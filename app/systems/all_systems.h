#pragma once

#include <entt/entt.hpp>

struct ButtonPressEvent;
struct GoEvent;
struct InputEvent;

// Phase 0: Raw input (polls raylib input)
void input_system(entt::registry& reg, float raw_dt);

// Phase 0.25a: Gesture routing — per-InputEvent dispatcher listener.
// Swipe InputEvents → GoEvents.  Registered in wire_input_dispatcher();
// delivered via disp.update<InputEvent>() in game_loop_frame before the
// fixed-step accumulator loop.
void gesture_routing_handle_input(entt::registry& reg, const InputEvent& evt);

// Phase 0.25b: Hit-test — per-InputEvent dispatcher listener.
// Tap InputEvents → ButtonPressEvents (semantic payload encoded at hit-test
// time, #273).  Registered in wire_input_dispatcher(); delivered alongside
// gesture_routing_handle_input via disp.update<InputEvent>().
void hit_test_handle_input(entt::registry& reg, const InputEvent& evt);

// Active-tag cache management (#317).
// Sync structural ActiveTag components against the current GameState.phase
// (O(1) when phase unchanged). Call invalidate after button spawn/destroy or
// out-of-seam phase mutation so the next ensure call does a full resync.
void ensure_active_tags_synced(entt::registry& reg);
void invalidate_active_tag_cache(entt::registry& reg);

// Phase 0.5: Test player AI (enqueues synthetic input actions)
void test_player_system(entt::registry& reg, float dt);

// Input dispatcher wiring
void wire_input_dispatcher(entt::registry& reg);
void unwire_input_dispatcher(entt::registry& reg);

// Per-event listener functions (exposed for make_registry in test_helpers.h)
void game_state_handle_go(entt::registry& reg, const GoEvent& evt);
void game_state_handle_press(entt::registry& reg, const ButtonPressEvent& evt);
void level_select_handle_go(entt::registry& reg, const GoEvent& evt);
void level_select_handle_press(entt::registry& reg, const ButtonPressEvent& evt);
void player_input_handle_go(entt::registry& reg, const GoEvent& evt);
void player_input_handle_press(entt::registry& reg, const ButtonPressEvent& evt);

// Test player initialization (call once from main if --test-player is set)
enum class TestPlayerSkill : uint8_t;
void test_player_init(entt::registry& reg, TestPlayerSkill skill,
                      const char* difficulty);

// Phase 2: Game State
void game_state_system(entt::registry& reg, float dt);
void level_select_system(entt::registry& reg, float dt);

// Phase 3: Rhythm Engine
void song_playback_system(entt::registry& reg, float dt);
void beat_log_system(entt::registry& reg, float dt);
void beat_scheduler_system(entt::registry& reg, float dt);

// Phase 4: Player
void player_input_system(entt::registry& reg, float dt);
void shape_window_system(entt::registry& reg, float dt);
void player_movement_system(entt::registry& reg, float dt);

// Obstacle counter signal wiring — call once per registry lifetime.
// Safe to call multiple times (guarded by ObstacleCounter::wired flag).
void wire_obstacle_counter(entt::registry& reg);

// Phase 5: World
void difficulty_system(entt::registry& reg, float dt);
void obstacle_spawn_system(entt::registry& reg, float dt);
void scroll_system(entt::registry& reg, float dt);
void ring_zone_log_system(entt::registry& reg, float dt);
void collision_system(entt::registry& reg, float dt);
void miss_detection_system(entt::registry& reg, float dt);
void scoring_system(entt::registry& reg, float dt);

// Phase 5.5: Energy
void energy_system(entt::registry& reg, float dt);

// Phase 6: Cleanup
void lifetime_system(entt::registry& reg, float dt);
void particle_system(entt::registry& reg, float dt);
void cleanup_system(entt::registry& reg, float dt);

// Phase 6.5: UI prep
void popup_display_system(entt::registry& reg, float dt);

// One-time initializer for a PopupDisplay (text + font size + base color).
// Called at spawn so popup_display_system never re-formats static fields per
// frame (#251).
struct ScorePopup;
struct PopupDisplay;
struct Color;
void init_popup_display(PopupDisplay& pd,
                        const ScorePopup& sp,
                        const Color& base);
void ui_navigation_system(entt::registry& reg, float dt);

// Phase 7: Camera
void game_camera_system(entt::registry& reg, float dt);  // model-to-world transforms
void ui_camera_system(entt::registry& reg, float dt);    // screen-space transforms

// Phase 8: Render — world pass (3D) + UI pass (2D)
void game_render_system(const entt::registry& reg, float alpha);
void ui_render_system(const entt::registry& reg, float alpha);

// Audio (no dt needed)
void sfx_bank_init(entt::registry& reg);
void sfx_playback_backend_init(entt::registry& reg);
void sfx_bank_unload(entt::registry& reg);
void audio_system(entt::registry& reg);

// Haptics (no dt needed) — drains HapticQueue; gating is at push time via haptics_enabled
void haptic_system(entt::registry& reg);
