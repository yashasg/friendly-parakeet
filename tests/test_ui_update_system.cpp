// Tests for the entity-driven UI update system (issue #1287 / refs #1193 OoS-A).
//
// `ui_update_system` hit-tests `view<UiPosition, UiBounds, OnPress,
// UiButtonTag>` against the frame's pointer-release event and dispatches the
// matching `ActionId` via a per-action function-pointer table. These tests
// pin the dispatch semantics for the Paused-screen pilot:
//
//   * pointer hit on Resume button → `NextPhasePlayingTag` requested
//   * pointer hit on Menu button   → `NextPhaseTitleTag`   requested
//   * pointer miss                 → no transition requested
//   * click before UI_ENTRY_DEBOUNCE elapses → ignored (debounce honored)
//   * un-pressed frame (no click / no touch_up) → ignored (no spurious fire)
//
// The pilot wires only PausedScreenTag through the lifecycle system, so
// other-screen buttons have no spawned entities to hit-test and the system
// is trivially a no-op there.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "test_helpers.h"

#include "components/actions.h"
#include "components/game_state.h"
#include "components/ui.h"
#include "constants.h"
#include "entities/settings.h"
#include "systems/game_phase_transition.h"
#include "systems/input.h"
#include "systems/screen_lifecycle_system.h"
#include "systems/settings_system.h"
#include "systems/tutorial_dodge_hint_bind_system.h"
#include "systems/ui_update_system.h"
#include "tags/tags.h"
#include "ui/generated/screen_spawners.h"
#include "ui/tutorial_dodge_hint.h"

namespace {

void prime_paused_entry(entt::registry& reg) {
    sync_game_phase_tags<GamePhasePausedTag>(reg);
    spawn_paused_screen(reg);
    // Step the phase timer past the entry debounce so clicks register.
    reg.ctx().get<GameState>().phase_timer = constants::UI_ENTRY_DEBOUNCE + 0.05f;
    // Clear any prior next-phase tag noise.
    clear_next_phase_tags(reg);
}

void click_at(InputState& input, float x, float y) {
    input.click = true;
    input.touch_up = false;
    input.end_x = x;
    input.end_y = y;
}

bool any_next_phase_pending(entt::registry& reg) {
    return is_phase_transition_pending(reg);
}

}  // namespace

TEST_CASE("ui_update_system: Paused resume hit dispatches NextPhasePlayingTag",
          "[ui][ui_update_system]") {
    entt::registry reg = make_registry();
    prime_paused_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    // Resume button bounds from paused.rgl: (160, 620, 400, 100).
    click_at(input, 360.0f, 670.0f);

    ui_update_system(reg);

    CHECK(reg.ctx().contains<NextPhasePlayingTag>());
    CHECK_FALSE(reg.ctx().contains<NextPhaseTitleTag>());
}

TEST_CASE("ui_update_system: Paused menu hit dispatches NextPhaseTitleTag",
          "[ui][ui_update_system]") {
    entt::registry reg = make_registry();
    prime_paused_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    // Menu button bounds from paused.rgl: (160, 820, 400, 100).
    click_at(input, 360.0f, 870.0f);

    ui_update_system(reg);

    CHECK(reg.ctx().contains<NextPhaseTitleTag>());
    CHECK_FALSE(reg.ctx().contains<NextPhasePlayingTag>());
}

TEST_CASE("ui_update_system: pointer miss requests no transition",
          "[ui][ui_update_system]") {
    entt::registry reg = make_registry();
    prime_paused_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    // Click in a gap region between the two paused buttons.
    click_at(input, 360.0f, 770.0f);

    ui_update_system(reg);

    CHECK_FALSE(any_next_phase_pending(reg));
}

TEST_CASE("ui_update_system: click before debounce is ignored",
          "[ui][ui_update_system]") {
    entt::registry reg = make_registry();
    sync_game_phase_tags<GamePhasePausedTag>(reg);
    spawn_paused_screen(reg);
    // Phase entered this tick — timer at 0, debounce active.
    reg.ctx().get<GameState>().phase_timer = 0.0f;
    clear_next_phase_tags(reg);

    auto& input = reg.ctx().get<InputState>();
    click_at(input, 360.0f, 670.0f);  // resume hit

    ui_update_system(reg);

    CHECK_FALSE(any_next_phase_pending(reg));
}

TEST_CASE("ui_update_system: no click/touch_up flag means no dispatch",
          "[ui][ui_update_system]") {
    entt::registry reg = make_registry();
    prime_paused_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    // Pointer in resume bounds but neither click nor touch_up set —
    // matches a still-held / mid-drag state, no release event.
    input.click = false;
    input.touch_up = false;
    input.end_x = 360.0f;
    input.end_y = 670.0f;

    ui_update_system(reg);

    CHECK_FALSE(any_next_phase_pending(reg));
}

TEST_CASE("ui_update_system: touch_up release on resume dispatches transition",
          "[ui][ui_update_system]") {
    entt::registry reg = make_registry();
    prime_paused_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    input.click = false;
    input.touch_up = true;
    input.end_x = 360.0f;
    input.end_y = 670.0f;

    ui_update_system(reg);

    CHECK(reg.ctx().contains<NextPhasePlayingTag>());
}

TEST_CASE("screen_lifecycle_system: spawns Paused entities when phase entered, "
          "despawns on exit",
          "[ui][screen_lifecycle_system]") {
    entt::registry reg = make_registry();
    clear_next_phase_tags(reg);

    // Pre-condition: no PausedScreenTag entities, no GamePhasePausedTag.
    sync_game_phase_tags<GamePhaseTitleTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<PausedScreenTag>().size() == 0);

    // Transition into Paused — lifecycle should spawn entities.
    sync_game_phase_tags<GamePhasePausedTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<PausedScreenTag>().size() > 0);

    // A second call must be idempotent — no duplicate spawn.
    const auto count_after_first_spawn = reg.view<PausedScreenTag>().size();
    screen_lifecycle_system(reg);
    CHECK(reg.view<PausedScreenTag>().size() == count_after_first_spawn);

    // Transition out — lifecycle despawns.
    sync_game_phase_tags<GamePhasePlayingTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<PausedScreenTag>().size() == 0);
}

// ── Tutorial migration (issue #1291) ──────────────────────────────────

namespace {

void prime_tutorial_entry(entt::registry& reg) {
    sync_game_phase_tags<GamePhaseTutorialTag>(reg);
    spawn_tutorial_screen(reg);
    reg.ctx().get<GameState>().phase_timer = constants::UI_ENTRY_DEBOUNCE + 0.05f;
    clear_next_phase_tags(reg);
}

}  // namespace

TEST_CASE("screen_lifecycle_system: spawns Tutorial entities on phase entry, "
          "despawns on exit",
          "[ui][screen_lifecycle_system]") {
    entt::registry reg = make_registry();
    clear_next_phase_tags(reg);

    sync_game_phase_tags<GamePhaseTitleTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<TutorialScreenTag>().size() == 0);

    sync_game_phase_tags<GamePhaseTutorialTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<TutorialScreenTag>().size() > 0);

    // Idempotent on repeat call.
    const auto first = reg.view<TutorialScreenTag>().size();
    screen_lifecycle_system(reg);
    CHECK(reg.view<TutorialScreenTag>().size() == first);

    sync_game_phase_tags<GamePhasePlayingTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<TutorialScreenTag>().size() == 0);
}

TEST_CASE("ui_update_system: Tutorial continue button marks FTUE complete and "
          "requests Playing phase",
          "[ui][ui_update_system][issue1291]") {
    entt::registry reg = make_registry();
    prime_tutorial_entry(reg);

    auto& settings = settings_state(reg);
    settings.ftue_run_count = 0;
    const auto settings_entity = *reg.view<SettingsTag>().begin();
    reg.remove<SettingsDirtyTag>(settings_entity);
    auto& persistence = settings_persistence(reg);
    persistence.path.clear();

    auto& input = reg.ctx().get<InputState>();
    // Continue button bounds from tutorial.rgl: (180, 1075, 360, 102).
    click_at(input, 360.0f, 1126.0f);

    ui_update_system(reg);

    CHECK(reg.ctx().contains<NextPhasePlayingTag>());
    CHECK(settings::ftue_complete(settings_state(reg)));
    const auto current_settings_entity = *reg.view<SettingsTag>().begin();
    CHECK(reg.all_of<SettingsDirtyTag>(current_settings_entity));
}

TEST_CASE("tutorial_dodge_hint_bind_system: writes platform text into DodgeHint slot",
          "[ui][tutorial_dodge_hint_bind_system][issue1291]") {
    entt::registry reg = make_registry();
    prime_tutorial_entry(reg);

    // Pre-condition: DodgeHint label exists at the canonical position with
    // empty text (dynamic slot from the .rgl).
    bool found_empty = false;
    auto pre_view = reg.view<TutorialScreenTag, UiLabelTag, UiPosition, UiLabel>();
    for (auto [e, pos, label] : pre_view.each()) {
        (void)e;
        if (pos.x == 110.0f && pos.y == 710.0f) {
            CHECK(label.text[0] == '\0');
            found_empty = true;
        }
    }
    REQUIRE(found_empty);

    tutorial_dodge_hint_bind_system(reg);

    // After bind, the DodgeHint slot must contain one of the platform
    // strings produced by `tutorial_dodge_hint_text()`.
    bool wrote_text = false;
    auto post_view = reg.view<TutorialScreenTag, UiLabelTag, UiPosition, UiLabel>();
    for (auto [e, pos, label] : post_view.each()) {
        (void)e;
        if (pos.x != 110.0f || pos.y != 710.0f) continue;
        const char* expected_touch    = tutorial_dodge_hint_text(true);
        const char* expected_keyboard = tutorial_dodge_hint_text(false);
        const bool matches = (std::string(label.text.data()) == expected_touch) ||
                             (std::string(label.text.data()) == expected_keyboard);
        CHECK(matches);
        wrote_text = true;
    }
    CHECK(wrote_text);
}

TEST_CASE("tutorial_dodge_hint_bind_system: no-op when Tutorial entities absent",
          "[ui][tutorial_dodge_hint_bind_system][issue1291]") {
    entt::registry reg = make_registry();
    // No tutorial spawn — system must be safe to call.
    tutorial_dodge_hint_bind_system(reg);
    CHECK(reg.view<TutorialScreenTag>().size() == 0);
}
