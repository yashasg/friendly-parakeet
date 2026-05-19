// Tests for the entity-driven UI update system (issue #1287 / refs #1193 OoS-A).
//
// `ui_update_system` hit-tests `view<UiPosition, UiBounds, UiButtonTag>`
// against the frame's pointer-release event and dispatches via the matching
// `UiAction*Tag` membership row. These tests
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

#include "components/game_state.h"
#include "components/ui.h"
#include "constants.h"
#include "entities/settings.h"
#include "systems/game_over_scoreboard_bind_system.h"
#include "systems/game_phase_transition.h"
#include "systems/input.h"
#include "systems/screen_lifecycle_system.h"
#include "systems/settings_system.h"
#include "systems/song_complete_scoreboard_bind_system.h"
#include "systems/settings_screen_bind_system.h"
#include "systems/title_start_tap_system.h"
#include "systems/tutorial_dodge_hint_bind_system.h"
#include "systems/ui_update_system.h"
#include "tags/tags.h"
#include "systems/generated/screen_spawners.h"
#include "util/tutorial_dodge_hint.h"
#include "util/level_content_config.h"

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

// ── Song Complete migration (issue #1292) ─────────────────────────────

namespace {

void prime_song_complete_entry(entt::registry& reg) {
    sync_game_phase_tags<GamePhaseSongCompleteTag>(reg);
    spawn_song_complete_screen(reg);
    // Step the phase timer past the SongComplete input delay so clicks register.
    reg.ctx().get<GameState>().phase_timer = constants::SONG_COMPLETE_INPUT_DELAY + 0.05f;
    clear_next_phase_tags(reg);
    if (reg.ctx().find<EndChoiceRestart>())     reg.ctx().erase<EndChoiceRestart>();
    if (reg.ctx().find<EndChoiceLevelSelect>()) reg.ctx().erase<EndChoiceLevelSelect>();
    if (reg.ctx().find<EndChoiceMainMenu>())    reg.ctx().erase<EndChoiceMainMenu>();
}

}  // namespace

TEST_CASE("screen_lifecycle_system: spawns Song Complete entities on phase entry, "
          "despawns on exit",
          "[ui][screen_lifecycle_system][issue1292]") {
    entt::registry reg = make_registry();
    clear_next_phase_tags(reg);

    sync_game_phase_tags<GamePhaseTitleTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<SongCompleteScreenTag>().size() == 0);

    sync_game_phase_tags<GamePhaseSongCompleteTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<SongCompleteScreenTag>().size() > 0);

    const auto first = reg.view<SongCompleteScreenTag>().size();
    screen_lifecycle_system(reg);
    CHECK(reg.view<SongCompleteScreenTag>().size() == first);

    sync_game_phase_tags<GamePhaseTitleTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<SongCompleteScreenTag>().size() == 0);
}

TEST_CASE("ui_update_system: Song Complete restart hit emplaces EndChoiceRestart",
          "[ui][ui_update_system][issue1292]") {
    entt::registry reg = make_registry();
    prime_song_complete_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    // RestartButton bounds from song_complete.rgl: (220, 870, 280, 50).
    click_at(input, 360.0f, 895.0f);

    ui_update_system(reg);

    CHECK(reg.ctx().find<EndChoiceRestart>() != nullptr);
    CHECK(reg.ctx().find<EndChoiceLevelSelect>() == nullptr);
    CHECK(reg.ctx().find<EndChoiceMainMenu>() == nullptr);
    CHECK_FALSE(any_next_phase_pending(reg));
}

TEST_CASE("ui_update_system: Song Complete level-select hit emplaces EndChoiceLevelSelect",
          "[ui][ui_update_system][issue1292]") {
    entt::registry reg = make_registry();
    prime_song_complete_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    // LevelSelectButton bounds from song_complete.rgl: (220, 935, 280, 50).
    click_at(input, 360.0f, 960.0f);

    ui_update_system(reg);

    CHECK(reg.ctx().find<EndChoiceLevelSelect>() != nullptr);
    CHECK(reg.ctx().find<EndChoiceRestart>() == nullptr);
    CHECK(reg.ctx().find<EndChoiceMainMenu>() == nullptr);
}

TEST_CASE("ui_update_system: Song Complete menu hit emplaces EndChoiceMainMenu (not direct transition)",
          "[ui][ui_update_system][issue1292]") {
    entt::registry reg = make_registry();
    prime_song_complete_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    // MenuButton bounds from song_complete.rgl: (220, 1000, 280, 50).
    click_at(input, 360.0f, 1025.0f);

    ui_update_system(reg);

    CHECK(reg.ctx().find<EndChoiceMainMenu>() != nullptr);
    CHECK(reg.ctx().find<EndChoiceRestart>() == nullptr);
    CHECK(reg.ctx().find<EndChoiceLevelSelect>() == nullptr);
    // Critically: end-screen menu must NOT bypass the input-delay state
    // machine by directly requesting NextPhaseTitleTag.
    CHECK_FALSE(reg.ctx().contains<NextPhaseTitleTag>());
}

TEST_CASE("ui_update_system: Song Complete click before SONG_COMPLETE_INPUT_DELAY is ignored",
          "[ui][ui_update_system][issue1292]") {
    entt::registry reg = make_registry();
    sync_game_phase_tags<GamePhaseSongCompleteTag>(reg);
    spawn_song_complete_screen(reg);
    clear_next_phase_tags(reg);
    if (reg.ctx().find<EndChoiceRestart>()) reg.ctx().erase<EndChoiceRestart>();

    // Timer in the [UI_ENTRY_DEBOUNCE, SONG_COMPLETE_INPUT_DELAY) range —
    // would clear the generic 0.2s debounce but NOT the 0.5s end-screen
    // delay, pinning that the per-phase debounce table is being consulted.
    reg.ctx().get<GameState>().phase_timer = constants::UI_ENTRY_DEBOUNCE + 0.05f;
    REQUIRE(reg.ctx().get<GameState>().phase_timer <
            constants::SONG_COMPLETE_INPUT_DELAY);

    auto& input = reg.ctx().get<InputState>();
    click_at(input, 360.0f, 895.0f);  // restart hit

    ui_update_system(reg);

    CHECK(reg.ctx().find<EndChoiceRestart>() == nullptr);
}

TEST_CASE("song_complete_scoreboard_bind_system: writes score / high score / stats / energy "
          "into dynamic-text slots",
          "[ui][song_complete_scoreboard_bind_system][issue1292]") {
    entt::registry reg = make_registry();
    prime_song_complete_entry(reg);

    auto& score = reg.ctx().get<ScoreState>();
    score.score = 12345;
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    current.value = 9000;
    auto& results = reg.ctx().get<SongResults>();
    results.perfect_count = 12;
    results.good_count    = 3;
    results.ok_count      = 2;
    results.bad_count     = 1;
    results.miss_count    = 0;
    results.max_chain     = 11;
    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 0.75f;

    song_complete_scoreboard_bind_system(reg);

    auto resolve = [&](float x, float y) -> std::string {
        auto view = reg.view<SongCompleteScreenTag, UiLabelTag, UiPosition, UiLabel>();
        for (auto [e, pos, label] : view.each()) {
            (void)e;
            if (pos.x == x && pos.y == y) return std::string(label.text.data());
        }
        return {};
    };

    CHECK(resolve(160.0f, 463.0f) == "12345");
    CHECK(resolve(160.0f, 573.0f) == "9000");
    CHECK(resolve(120.0f, 650.0f) == "PERFECT 12     GOOD 3");
    CHECK(resolve(120.0f, 684.0f) == "OK 2     BAD 1     MISS 0");
    CHECK(resolve(120.0f, 718.0f) == "MAX CHAIN 11");
    CHECK(resolve(120.0f, 752.0f) == "ENERGY 75%");
    // NEW BEST line is empty when no NewBestRecord ctx row is present.
    CHECK(resolve(120.0f, 620.0f).empty());
}

TEST_CASE("song_complete_scoreboard_bind_system: writes NEW BEST line when terminal "
          "result reports new_best",
          "[ui][song_complete_scoreboard_bind_system][issue1292]") {
    entt::registry reg = make_registry();
    prime_song_complete_entry(reg);

    reg.ctx().insert_or_assign(NewBestRecord{4242});

    song_complete_scoreboard_bind_system(reg);

    auto view = reg.view<SongCompleteScreenTag, UiLabelTag, UiPosition, UiLabel>();
    bool found = false;
    for (auto [e, pos, label] : view.each()) {
        (void)e;
        if (pos.x != 120.0f || pos.y != 620.0f) continue;
        CHECK(std::string(label.text.data()) == "NEW BEST! PREV 4242");
        found = true;
    }
    CHECK(found);
}

TEST_CASE("song_complete_scoreboard_bind_system: no-op when Song Complete entities absent",
          "[ui][song_complete_scoreboard_bind_system][issue1292]") {
    entt::registry reg = make_registry();
    // No song-complete spawn — system must be safe to call (and must not
    // touch ScoreState even though it is emplaced by make_registry; the
    // view-empty short-circuit covers that).
    song_complete_scoreboard_bind_system(reg);
    CHECK(reg.view<SongCompleteScreenTag>().size() == 0);
}

// ── Game Over migration (issue #1293) ─────────────────────────────────

namespace {

void prime_game_over_entry(entt::registry& reg) {
    sync_game_phase_tags<GamePhaseGameOverTag>(reg);
    spawn_game_over_screen(reg);
    // Step the phase timer past the GameOver input delay so clicks register.
    reg.ctx().get<GameState>().phase_timer = constants::GAME_OVER_INPUT_DELAY + 0.05f;
    clear_next_phase_tags(reg);
    if (reg.ctx().find<EndChoiceRestart>())     reg.ctx().erase<EndChoiceRestart>();
    if (reg.ctx().find<EndChoiceLevelSelect>()) reg.ctx().erase<EndChoiceLevelSelect>();
    if (reg.ctx().find<EndChoiceMainMenu>())    reg.ctx().erase<EndChoiceMainMenu>();
}

std::string resolve_game_over_slot(entt::registry& reg, float x, float y) {
    auto view = reg.view<GameOverScreenTag, UiLabelTag, UiPosition, UiLabel>();
    for (auto [e, pos, label] : view.each()) {
        (void)e;
        if (pos.x == x && pos.y == y) return std::string(label.text.data());
    }
    return {};
}

}  // namespace

TEST_CASE("screen_lifecycle_system: spawns Game Over entities on phase entry, "
          "despawns on exit",
          "[ui][screen_lifecycle_system][issue1293]") {
    entt::registry reg = make_registry();
    clear_next_phase_tags(reg);

    sync_game_phase_tags<GamePhaseTitleTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<GameOverScreenTag>().size() == 0);

    sync_game_phase_tags<GamePhaseGameOverTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<GameOverScreenTag>().size() > 0);

    const auto first = reg.view<GameOverScreenTag>().size();
    screen_lifecycle_system(reg);
    CHECK(reg.view<GameOverScreenTag>().size() == first);

    sync_game_phase_tags<GamePhaseTitleTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<GameOverScreenTag>().size() == 0);
}

TEST_CASE("ui_update_system: Game Over restart hit emplaces EndChoiceRestart",
          "[ui][ui_update_system][issue1293]") {
    entt::registry reg = make_registry();
    prime_game_over_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    // RestartButton bounds from game_over.rgl: (220, 870, 280, 50).
    click_at(input, 360.0f, 895.0f);

    ui_update_system(reg);

    CHECK(reg.ctx().find<EndChoiceRestart>() != nullptr);
    CHECK(reg.ctx().find<EndChoiceLevelSelect>() == nullptr);
    CHECK(reg.ctx().find<EndChoiceMainMenu>() == nullptr);
    CHECK_FALSE(any_next_phase_pending(reg));
}

TEST_CASE("ui_update_system: Game Over level-select hit emplaces EndChoiceLevelSelect",
          "[ui][ui_update_system][issue1293]") {
    entt::registry reg = make_registry();
    prime_game_over_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    // LevelSelectButton bounds from game_over.rgl: (220, 935, 280, 50).
    click_at(input, 360.0f, 960.0f);

    ui_update_system(reg);

    CHECK(reg.ctx().find<EndChoiceLevelSelect>() != nullptr);
    CHECK(reg.ctx().find<EndChoiceRestart>() == nullptr);
    CHECK(reg.ctx().find<EndChoiceMainMenu>() == nullptr);
}

TEST_CASE("ui_update_system: Game Over menu hit emplaces EndChoiceMainMenu (not direct transition)",
          "[ui][ui_update_system][issue1293]") {
    entt::registry reg = make_registry();
    prime_game_over_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    // MenuButton bounds from game_over.rgl: (220, 1000, 280, 50).
    click_at(input, 360.0f, 1025.0f);

    ui_update_system(reg);

    CHECK(reg.ctx().find<EndChoiceMainMenu>() != nullptr);
    CHECK(reg.ctx().find<EndChoiceRestart>() == nullptr);
    CHECK(reg.ctx().find<EndChoiceLevelSelect>() == nullptr);
    // End-screen menu must NOT bypass the input-delay state machine by
    // requesting NextPhaseTitleTag directly — the legacy controller routes
    // every end-screen choice through `game_state_end_screen_system`.
    CHECK_FALSE(reg.ctx().contains<NextPhaseTitleTag>());
}

TEST_CASE("ui_update_system: Game Over click before GAME_OVER_INPUT_DELAY is ignored",
          "[ui][ui_update_system][issue1293]") {
    entt::registry reg = make_registry();
    sync_game_phase_tags<GamePhaseGameOverTag>(reg);
    spawn_game_over_screen(reg);
    clear_next_phase_tags(reg);
    if (reg.ctx().find<EndChoiceRestart>()) reg.ctx().erase<EndChoiceRestart>();

    // Timer in the [UI_ENTRY_DEBOUNCE, GAME_OVER_INPUT_DELAY) range —
    // would clear the generic 0.2s debounce but NOT the 0.4s end-screen
    // delay, pinning that the per-phase debounce table is being consulted.
    reg.ctx().get<GameState>().phase_timer = constants::UI_ENTRY_DEBOUNCE + 0.05f;
    REQUIRE(reg.ctx().get<GameState>().phase_timer <
            constants::GAME_OVER_INPUT_DELAY);

    auto& input = reg.ctx().get<InputState>();
    click_at(input, 360.0f, 895.0f);  // restart hit

    ui_update_system(reg);

    CHECK(reg.ctx().find<EndChoiceRestart>() == nullptr);
}

TEST_CASE("game_over_scoreboard_bind_system: writes score / high-score into slots",
          "[ui][game_over_scoreboard_bind_system][issue1293]") {
    entt::registry reg = make_registry();
    prime_game_over_entry(reg);

    auto& score = reg.ctx().get<ScoreState>();
    score.score = 12345;
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    current.value = 9000;

    game_over_scoreboard_bind_system(reg);

    CHECK(resolve_game_over_slot(reg, 210.0f, 540.0f) == "12345");
    CHECK(resolve_game_over_slot(reg, 210.0f, 634.0f) == "9000");
    // No NewBestRecord / EnergyDepletedDeath — all dynamic reason /
    // best slots empty.
    CHECK(resolve_game_over_slot(reg, 110.0f, 665.0f).empty());
    CHECK(resolve_game_over_slot(reg, 110.0f, 712.0f).empty());
    CHECK(resolve_game_over_slot(reg, 110.0f, 685.0f).empty());
    CHECK(resolve_game_over_slot(reg, 110.0f, 742.0f).empty());
}

TEST_CASE("game_over_scoreboard_bind_system: ENERGY DEPLETED at y=685 when NOT new_best",
          "[ui][game_over_scoreboard_bind_system][issue1293]") {
    entt::registry reg = make_registry();
    prime_game_over_entry(reg);
    reg.ctx().insert_or_assign(EnergyDepletedDeath{});

    game_over_scoreboard_bind_system(reg);

    CHECK(resolve_game_over_slot(reg, 110.0f, 685.0f) == "ENERGY DEPLETED");
    CHECK(resolve_game_over_slot(reg, 110.0f, 742.0f).empty());
    CHECK(resolve_game_over_slot(reg, 110.0f, 665.0f).empty());
    CHECK(resolve_game_over_slot(reg, 110.0f, 712.0f).empty());
}

TEST_CASE("game_over_scoreboard_bind_system: ENERGY DEPLETED shifts to y=742 + NEW BEST/PREV "
          "lines populated when new_best",
          "[ui][game_over_scoreboard_bind_system][issue1293]") {
    entt::registry reg = make_registry();
    prime_game_over_entry(reg);

    reg.ctx().insert_or_assign(EnergyDepletedDeath{});
    reg.ctx().insert_or_assign(NewBestRecord{3000});

    game_over_scoreboard_bind_system(reg);

    CHECK(resolve_game_over_slot(reg, 110.0f, 665.0f) == "NEW BEST!");
    CHECK(resolve_game_over_slot(reg, 110.0f, 712.0f) == "PREV 3000");
    CHECK(resolve_game_over_slot(reg, 110.0f, 742.0f) == "ENERGY DEPLETED");
    // Lower-position reason slot must be empty so the legacy positional
    // semantic (text moves down to clear the NEW BEST! / PREV lines) is
    // preserved without overlap.
    CHECK(resolve_game_over_slot(reg, 110.0f, 685.0f).empty());
}

TEST_CASE("game_over_scoreboard_bind_system: NEW BEST/PREV without death cause leaves reason empty",
          "[ui][game_over_scoreboard_bind_system][issue1293]") {
    entt::registry reg = make_registry();
    prime_game_over_entry(reg);
    reg.ctx().insert_or_assign(NewBestRecord{4242});

    game_over_scoreboard_bind_system(reg);

    CHECK(resolve_game_over_slot(reg, 110.0f, 665.0f) == "NEW BEST!");
    CHECK(resolve_game_over_slot(reg, 110.0f, 712.0f) == "PREV 4242");
    CHECK(resolve_game_over_slot(reg, 110.0f, 685.0f).empty());
    CHECK(resolve_game_over_slot(reg, 110.0f, 742.0f).empty());
}

TEST_CASE("game_over_scoreboard_bind_system: no-op when Game Over entities absent",
          "[ui][game_over_scoreboard_bind_system][issue1293]") {
    entt::registry reg = make_registry();
    // No game-over spawn — system must be safe to call.
    game_over_scoreboard_bind_system(reg);
    CHECK(reg.view<GameOverScreenTag>().size() == 0);
}

// ── Title migration (issue #1294) ─────────────────────────────────────

namespace {

void prime_title_entry(entt::registry& reg) {
    sync_game_phase_tags<GamePhaseTitleTag>(reg);
    spawn_title_screen(reg);
    reg.ctx().get<GameState>().phase_timer = constants::UI_ENTRY_DEBOUNCE + 0.05f;
    clear_next_phase_tags(reg);
}

}  // namespace

TEST_CASE("screen_lifecycle_system: spawns Title entities on phase entry, "
          "despawns on exit",
          "[ui][screen_lifecycle_system][issue1294]") {
    entt::registry reg = make_registry();
    clear_next_phase_tags(reg);

    // Pre-condition: not on Title.
    sync_game_phase_tags<GamePhasePausedTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<TitleScreenTag>().size() == 0);

    sync_game_phase_tags<GamePhaseTitleTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<TitleScreenTag>().size() > 0);

    const auto count_after_first_spawn = reg.view<TitleScreenTag>().size();
    screen_lifecycle_system(reg);
    CHECK(reg.view<TitleScreenTag>().size() == count_after_first_spawn);

    sync_game_phase_tags<GamePhasePlayingTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<TitleScreenTag>().size() == 0);
}

TEST_CASE("Title shape preview entities carry per-shape icon tags (issue #1294)",
          "[ui][title][codegen]") {
    entt::registry reg;
    spawn_title_screen(reg);

    // Exactly one icon entity per shape (Hexagon not present on the Title
    // screen; codegen NAME_EXTRA_TAGS only declares the three preview shapes).
    CHECK(reg.view<UiShapeIconCircleTag>().size()   == 1);
    CHECK(reg.view<UiShapeIconSquareTag>().size()   == 1);
    CHECK(reg.view<UiShapeIconTriangleTag>().size() == 1);
    CHECK(reg.view<UiShapeIconHexagonTag>().size()  == 0);

    // Each icon entity also carries the parent screen tag plus UiDummyRecTag
    // (Fabian existential mechanic — icon kind is presence, not a Shape field).
    auto circles = reg.view<UiShapeIconCircleTag, TitleScreenTag, UiDummyRecTag>();
    CHECK(circles.size_hint() > 0);
}

TEST_CASE("Title ExitButton carries UiHiddenOnWebTag (#511, issue #1294)",
          "[ui][title][codegen]") {
    entt::registry reg;
    spawn_title_screen(reg);

    auto exit_view = reg.view<UiActionExitButtonTag, UiHiddenOnWebTag,
                              TitleScreenTag>();
    int matches = 0;
    for (auto e : exit_view) {
        (void)e;
        ++matches;
    }
    CHECK(matches == 1);
}

#ifndef PLATFORM_WEB
TEST_CASE("ui_update_system: Title ExitButton press sets quit_requested (#1294)",
          "[ui][ui_update_system][issue1294]") {
    entt::registry reg = make_registry();
    prime_title_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    // ExitButton bounds from title.rgl: (260, 1080, 200, 56).
    click_at(input, 360.0f, 1108.0f);

    ui_update_system(reg);

    CHECK(input.quit_requested);
    CHECK_FALSE(any_next_phase_pending(reg));
}
#endif

TEST_CASE("ui_update_system: Title SettingsButton press requests Settings phase (#1294)",
          "[ui][ui_update_system][issue1294]") {
    entt::registry reg = make_registry();
    prime_title_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    // SettingsButton bounds from title.rgl: (632, 1170, 64, 64).
    click_at(input, 664.0f, 1202.0f);

    ui_update_system(reg);

    CHECK(reg.ctx().contains<NextPhaseSettingsTag>());
    CHECK_FALSE(input.quit_requested);
}

TEST_CASE("title_start_tap_system: tap outside button bounds requests LevelSelect (#1294)",
          "[ui][title_start_tap_system][issue1294]") {
    entt::registry reg = make_registry();
    prime_title_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    // Centre of the "TAP TO START" label — definitely not in any button.
    click_at(input, 360.0f, 668.0f);

    title_start_tap_system(reg);

    CHECK(reg.ctx().contains<NextPhaseLevelSelectTag>());
}

TEST_CASE("title_start_tap_system: tap on SettingsButton bounds does NOT start (#1294)",
          "[ui][title_start_tap_system][issue1294]") {
    entt::registry reg = make_registry();
    prime_title_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    click_at(input, 664.0f, 1202.0f);  // inside Settings button bounds

    title_start_tap_system(reg);

    CHECK_FALSE(reg.ctx().contains<NextPhaseLevelSelectTag>());
}

TEST_CASE("title_start_tap_system: tap on ExitButton bounds is a dead-zone on every platform (#511, #1294)",
          "[ui][title_start_tap_system][issue1294]") {
    entt::registry reg = make_registry();
    prime_title_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    // Inside the ExitButton bounds (260, 1080, 200, 56).
    click_at(input, 360.0f, 1108.0f);

    title_start_tap_system(reg);

    // Must remain a dead-zone on every platform: the entity is invisible on
    // Web (UiHiddenOnWebTag) but its bounds still gate the start gesture.
    CHECK_FALSE(reg.ctx().contains<NextPhaseLevelSelectTag>());
}

TEST_CASE("title_start_tap_system: respects UI_ENTRY_DEBOUNCE (#1294)",
          "[ui][title_start_tap_system][issue1294]") {
    entt::registry reg = make_registry();
    sync_game_phase_tags<GamePhaseTitleTag>(reg);
    spawn_title_screen(reg);
    clear_next_phase_tags(reg);

    auto& gs = reg.ctx().get<GameState>();
    gs.phase_timer = constants::UI_ENTRY_DEBOUNCE * 0.5f;  // before debounce

    auto& input = reg.ctx().get<InputState>();
    click_at(input, 360.0f, 668.0f);

    title_start_tap_system(reg);

    CHECK_FALSE(reg.ctx().contains<NextPhaseLevelSelectTag>());
}

TEST_CASE("title_start_tap_system: no-op when GamePhaseTitleTag absent (#1294)",
          "[ui][title_start_tap_system][issue1294]") {
    entt::registry reg = make_registry();
    sync_game_phase_tags<GamePhasePausedTag>(reg);
    clear_next_phase_tags(reg);

    auto& input = reg.ctx().get<InputState>();
    click_at(input, 360.0f, 668.0f);

    title_start_tap_system(reg);

    CHECK_FALSE(reg.ctx().contains<NextPhaseLevelSelectTag>());
}

TEST_CASE("title_start_tap_system: idle frame (no click/touch_up) is a no-op (#1294)",
          "[ui][title_start_tap_system][issue1294]") {
    entt::registry reg = make_registry();
    prime_title_entry(reg);

    title_start_tap_system(reg);  // no click_at — InputState all-default

    CHECK_FALSE(reg.ctx().contains<NextPhaseLevelSelectTag>());
}

#ifdef PLATFORM_WEB
TEST_CASE("ui_update_system: PLATFORM_WEB skips UiHiddenOnWebTag entities (#511, #1294)",
          "[ui][ui_update_system][issue1294][web]") {
    entt::registry reg = make_registry();
    prime_title_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    click_at(input, 360.0f, 1108.0f);  // ExitButton centre

    ui_update_system(reg);

    CHECK_FALSE(input.quit_requested);
    CHECK_FALSE(any_next_phase_pending(reg));
}
#endif

// ── Settings screen migration (#1295) ────────────────────────────────────

namespace {

void prime_settings_entry(entt::registry& reg) {
    sync_game_phase_tags<GamePhaseSettingsTag>(reg);
    spawn_settings_screen(reg);
    reg.ctx().get<GameState>().phase_timer = constants::UI_ENTRY_DEBOUNCE + 0.05f;
    clear_next_phase_tags(reg);
}

}  // namespace

TEST_CASE("screen_lifecycle_system: spawns Settings entities on phase entry, "
          "despawns on exit",
          "[ui][screen_lifecycle_system][issue1295]") {
    entt::registry reg = make_registry();
    clear_next_phase_tags(reg);

    // Pre-condition: not on Settings.
    sync_game_phase_tags<GamePhaseTitleTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<SettingsScreenTag>().size() == 0);

    sync_game_phase_tags<GamePhaseSettingsTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<SettingsScreenTag>().size() > 0);

    // Idempotent — second tick on the same phase does not double-spawn.
    const auto count_after_first_spawn = reg.view<SettingsScreenTag>().size();
    screen_lifecycle_system(reg);
    CHECK(reg.view<SettingsScreenTag>().size() == count_after_first_spawn);

    sync_game_phase_tags<GamePhaseTitleTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<SettingsScreenTag>().size() == 0);
}

TEST_CASE("Settings toggle buttons carry UiToggleTag (issue #1295)",
          "[ui][settings][codegen][issue1295]") {
    entt::registry reg;
    spawn_settings_screen(reg);

    // Exactly two toggle buttons: HapticsToggle + ReduceMotionToggle.
    auto toggles = reg.view<UiToggleTag, UiButtonTag, SettingsScreenTag>();
    CHECK(toggles.size_hint() == 2);

    CHECK(reg.view<UiToggleTag, UiActionHapticsToggleTag>().size_hint() == 1);
    CHECK(reg.view<UiToggleTag, UiActionReduceMotionToggleTag>().size_hint() == 1);

    // Non-toggle Settings buttons (audio +/- and CloseButton) must NOT
    // carry the toggle tag — otherwise the render system would apply
    // the green/grey style to them.
    auto non_toggle_btns = reg.view<UiButtonTag, SettingsScreenTag>(
        entt::exclude<UiToggleTag>);
    int non_toggle_count = 0;
    for (auto e : non_toggle_btns) { (void)e; ++non_toggle_count; }
    CHECK(non_toggle_count == 3);
}

TEST_CASE("ui_update_system: Settings audio offset minus decrements and persists",
          "[ui][ui_update_system][issue1295]") {
    entt::registry reg = make_registry();
    prime_settings_entry(reg);

    auto& settings = settings_state(reg);
    settings.audio_offset_ms = 50;
    const auto settings_entity = *reg.view<SettingsTag>().begin();
    reg.remove<SettingsDirtyTag>(settings_entity);

    auto& input = reg.ctx().get<InputState>();
    // AudioOffsetMinus bounds from settings.rgl: (180, 560, 72, 77).
    click_at(input, 216.0f, 600.0f);

    ui_update_system(reg);

    CHECK(settings_state(reg).audio_offset_ms ==
          50 - SettingsState::AUDIO_OFFSET_STEP_MS);
    const auto settings_entity_after = *reg.view<SettingsTag>().begin();
    CHECK(reg.all_of<SettingsDirtyTag>(settings_entity_after));
    CHECK_FALSE(any_next_phase_pending(reg));
}

TEST_CASE("ui_update_system: Settings audio offset plus increments and persists",
          "[ui][ui_update_system][issue1295]") {
    entt::registry reg = make_registry();
    prime_settings_entry(reg);

    auto& settings = settings_state(reg);
    settings.audio_offset_ms = -50;
    const auto settings_entity = *reg.view<SettingsTag>().begin();
    reg.remove<SettingsDirtyTag>(settings_entity);

    auto& input = reg.ctx().get<InputState>();
    // AudioOffsetPlus bounds from settings.rgl: (468, 560, 72, 77).
    click_at(input, 504.0f, 600.0f);

    ui_update_system(reg);

    CHECK(settings_state(reg).audio_offset_ms ==
          -50 + SettingsState::AUDIO_OFFSET_STEP_MS);
    const auto settings_entity_after = *reg.view<SettingsTag>().begin();
    CHECK(reg.all_of<SettingsDirtyTag>(settings_entity_after));
}

TEST_CASE("ui_update_system: Settings audio offset clamps at MIN/MAX",
          "[ui][ui_update_system][issue1295]") {
    entt::registry reg = make_registry();
    prime_settings_entry(reg);

    auto& input = reg.ctx().get<InputState>();

    // Clamp at min.
    settings_state(reg).audio_offset_ms = SettingsState::MIN_AUDIO_OFFSET_MS;
    click_at(input, 216.0f, 600.0f);  // minus
    ui_update_system(reg);
    CHECK(settings_state(reg).audio_offset_ms ==
          SettingsState::MIN_AUDIO_OFFSET_MS);

    // Clamp at max.
    settings_state(reg).audio_offset_ms = SettingsState::MAX_AUDIO_OFFSET_MS;
    click_at(input, 504.0f, 600.0f);  // plus
    ui_update_system(reg);
    CHECK(settings_state(reg).audio_offset_ms ==
          SettingsState::MAX_AUDIO_OFFSET_MS);
}

TEST_CASE("ui_update_system: Settings close button requests Title phase",
          "[ui][ui_update_system][issue1295]") {
    entt::registry reg = make_registry();
    prime_settings_entry(reg);

    auto& input = reg.ctx().get<InputState>();
    // CloseButton bounds from settings.rgl: (260, 1040, 200, 100).
    click_at(input, 360.0f, 1090.0f);

    ui_update_system(reg);

    CHECK(reg.ctx().contains<NextPhaseTitleTag>());
}

TEST_CASE("ui_update_system: Settings haptics toggle flips haptics_enabled and persists",
          "[ui][ui_update_system][issue1295]") {
    entt::registry reg = make_registry();
    prime_settings_entry(reg);

    settings_state(reg).haptics_enabled = true;
    const auto settings_entity = *reg.view<SettingsTag>().begin();
    reg.remove<SettingsDirtyTag>(settings_entity);

    auto& input = reg.ctx().get<InputState>();
    // HapticsToggle bounds from settings.rgl: (152, 720, 416, 100).
    click_at(input, 360.0f, 770.0f);

    ui_update_system(reg);

    CHECK_FALSE(settings_state(reg).haptics_enabled);
    const auto settings_entity_after = *reg.view<SettingsTag>().begin();
    CHECK(reg.all_of<SettingsDirtyTag>(settings_entity_after));
}

TEST_CASE("ui_update_system: Settings reduce motion toggle flips reduce_motion",
          "[ui][ui_update_system][issue1295]") {
    entt::registry reg = make_registry();
    prime_settings_entry(reg);

    settings_state(reg).reduce_motion = false;
    const auto settings_entity = *reg.view<SettingsTag>().begin();
    reg.remove<SettingsDirtyTag>(settings_entity);

    auto& input = reg.ctx().get<InputState>();
    // ReduceMotionToggle bounds from settings.rgl: (152, 880, 416, 100).
    click_at(input, 360.0f, 930.0f);

    ui_update_system(reg);

    CHECK(settings_state(reg).reduce_motion);
    const auto settings_entity_after = *reg.view<SettingsTag>().begin();
    CHECK(reg.all_of<SettingsDirtyTag>(settings_entity_after));
}

TEST_CASE("ui_update_system: Settings click before debounce ignored (issue #1295)",
          "[ui][ui_update_system][issue1295]") {
    entt::registry reg = make_registry();
    sync_game_phase_tags<GamePhaseSettingsTag>(reg);
    spawn_settings_screen(reg);
    reg.ctx().get<GameState>().phase_timer = 0.0f;
    clear_next_phase_tags(reg);

    const auto offset_before = settings_state(reg).audio_offset_ms;

    auto& input = reg.ctx().get<InputState>();
    click_at(input, 216.0f, 600.0f);  // AudioOffsetMinus centre

    ui_update_system(reg);

    CHECK(settings_state(reg).audio_offset_ms == offset_before);
}

TEST_CASE("settings_screen_bind_system: writes formatted audio offset text",
          "[ui][settings_screen_bind_system][issue1295]") {
    entt::registry reg = make_registry();
    prime_settings_entry(reg);

    settings_state(reg).audio_offset_ms = -40;
    settings_screen_bind_system(reg);

    // AudioOffsetDisplay at (252, 560) — see settings.rgl.
    auto view = reg.view<SettingsScreenTag, UiPosition, UiLabel>();
    bool found = false;
    for (auto [e, pos, label] : view.each()) {
        (void)e;
        if (pos.x == 252.0f && pos.y == 560.0f) {
            CHECK(std::string(label.text.data()) == "-40 ms");
            found = true;
            break;
        }
    }
    CHECK(found);

    // Positive offset gets a `+` prefix.
    settings_state(reg).audio_offset_ms = 70;
    settings_screen_bind_system(reg);
    for (auto [e, pos, label] : view.each()) {
        (void)e;
        if (pos.x == 252.0f && pos.y == 560.0f) {
            CHECK(std::string(label.text.data()) == "+70 ms");
            break;
        }
    }
}

TEST_CASE("settings_screen_bind_system: writes toggle labels and state tags",
          "[ui][settings_screen_bind_system][issue1295]") {
    entt::registry reg = make_registry();
    prime_settings_entry(reg);

    settings_state(reg).haptics_enabled = true;
    settings_state(reg).reduce_motion = false;
    settings_screen_bind_system(reg);

    auto view = reg.view<SettingsScreenTag, UiToggleTag, UiPosition, UiLabel>();
    int haptics_match = 0;
    int motion_match = 0;
    for (auto entity : view) {
        const auto& pos = view.get<UiPosition>(entity);
        const auto& label = view.get<UiLabel>(entity);
        if (pos.x == 152.0f && pos.y == 720.0f) {
            CHECK(std::string(label.text.data()) == "[X] HAPTICS: ON");
            CHECK(reg.all_of<UiToggleOnTag>(entity));
            CHECK_FALSE(reg.all_of<UiToggleOffTag>(entity));
            ++haptics_match;
        } else if (pos.x == 152.0f && pos.y == 880.0f) {
            // motion_on = !reduce_motion → true → label shows MOTION: ON
            CHECK(std::string(label.text.data()) == "[X] MOTION: ON");
            CHECK(reg.all_of<UiToggleOnTag>(entity));
            CHECK_FALSE(reg.all_of<UiToggleOffTag>(entity));
            ++motion_match;
        }
    }
    CHECK(haptics_match == 1);
    CHECK(motion_match == 1);

    // Flip both — verify off-state cue.
    settings_state(reg).haptics_enabled = false;
    settings_state(reg).reduce_motion = true;
    settings_screen_bind_system(reg);
    for (auto entity : view) {
        const auto& pos = view.get<UiPosition>(entity);
        const auto& label = view.get<UiLabel>(entity);
        if (pos.x == 152.0f && pos.y == 720.0f) {
            CHECK(std::string(label.text.data()) == "[ ] HAPTICS: OFF");
            CHECK(reg.all_of<UiToggleOffTag>(entity));
            CHECK_FALSE(reg.all_of<UiToggleOnTag>(entity));
        } else if (pos.x == 152.0f && pos.y == 880.0f) {
            CHECK(std::string(label.text.data()) == "[ ] MOTION: OFF");
            CHECK(reg.all_of<UiToggleOffTag>(entity));
            CHECK_FALSE(reg.all_of<UiToggleOnTag>(entity));
        }
    }
}

TEST_CASE("settings_screen_bind_system: no-op when Settings entities absent",
          "[ui][settings_screen_bind_system][issue1295]") {
    entt::registry reg = make_registry();
    // No spawn_settings_screen call — the bind must short-circuit.
    settings_screen_bind_system(reg);
    SUCCEED("bind ran without spawning Settings entities");
}

// ── Level Select screen migration (#1296) ────────────────────────────────

namespace {

void prime_level_select_entry(entt::registry& reg) {
    sync_game_phase_tags<GamePhaseLevelSelectTag>(reg);
    screen_lifecycle_system(reg);
    reg.ctx().get<GameState>().phase_timer = constants::UI_ENTRY_DEBOUNCE + 0.05f;
    clear_next_phase_tags(reg);
    reg.ctx().erase<LevelSelectConfirmedTag>();
}

}  // namespace

TEST_CASE("screen_lifecycle_system: spawns Level Select entities on phase entry, "
          "despawns on exit (#1296)",
          "[ui][screen_lifecycle_system][issue1296]") {
    entt::registry reg = make_registry();
    clear_next_phase_tags(reg);

    sync_game_phase_tags<GamePhaseTitleTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<LevelSelectScreenTag>().size() == 0);

    sync_game_phase_tags<GamePhaseLevelSelectTag>(reg);
    screen_lifecycle_system(reg);
    // Static (codegen): HeaderText label + StartButton.
    // Dynamic: LEVEL_COUNT cards + LEVEL_COUNT*DIFFICULTY_COUNT diff buttons.
    const auto expected = 2 + content_config::LEVEL_COUNT
                           + content_config::LEVEL_COUNT
                             * content_config::DIFFICULTY_COUNT;
    CHECK(reg.view<LevelSelectScreenTag>().size()
          == static_cast<std::size_t>(expected));

    // Idempotent — second tick on the same phase does not double-spawn.
    const auto count_after_first = reg.view<LevelSelectScreenTag>().size();
    screen_lifecycle_system(reg);
    CHECK(reg.view<LevelSelectScreenTag>().size() == count_after_first);

    sync_game_phase_tags<GamePhaseTitleTag>(reg);
    screen_lifecycle_system(reg);
    CHECK(reg.view<LevelSelectScreenTag>().size() == 0);
}

TEST_CASE("Level Select cards carry LevelCardTag + LevelIndex; no UiButtonTag (#1296)",
          "[ui][level_select][issue1296]") {
    entt::registry reg = make_registry();
    sync_game_phase_tags<GamePhaseLevelSelectTag>(reg);
    screen_lifecycle_system(reg);

    auto cards = reg.view<LevelCardTag, LevelIndex, LevelSelectScreenTag>();
    int card_count = 0;
    bool indices_seen[content_config::LEVEL_COUNT] = {};
    for (auto e : cards) {
        ++card_count;
        const auto& idx = cards.get<LevelIndex>(e);
        REQUIRE(content_config::is_valid_level_index(idx.value));
        indices_seen[idx.value] = true;
        // Per #1296: cards intentionally do NOT carry the regular button
        // archetype components — Pass C of `ui_update_system` reads
        // `LevelCardTag` + `LevelIndex` directly. Keeping them out of the
        // generic button view means diff buttons (Pass A) and other UI
        // buttons (Pass B) win priority when they overlap the card region.
        CHECK_FALSE(reg.all_of<UiButtonTag>(e));
    }
    CHECK(card_count == content_config::LEVEL_COUNT);
    for (int i = 0; i < content_config::LEVEL_COUNT; ++i) {
        CHECK(indices_seen[i]);
    }
}

TEST_CASE("Level Select difficulty buttons carry DifficultyButtonTag + LevelIndex + "
          "DifficultyIndex + matching action tag (#1296)",
          "[ui][level_select][issue1296]") {
    entt::registry reg = make_registry();
    sync_game_phase_tags<GamePhaseLevelSelectTag>(reg);
    screen_lifecycle_system(reg);

    auto diffs = reg.view<DifficultyButtonTag, LevelIndex, DifficultyIndex,
                          UiButtonTag, LevelSelectScreenTag>();
    int per_pair[content_config::LEVEL_COUNT][content_config::DIFFICULTY_COUNT] = {};
    int total = 0;
    for (auto e : diffs) {
        ++total;
        const auto& li = diffs.get<LevelIndex>(e);
        const auto& di = diffs.get<DifficultyIndex>(e);
        REQUIRE(content_config::is_valid_level_index(li.value));
        REQUIRE(content_config::is_valid_difficulty_index(di.value));
        ++per_pair[li.value][di.value];
        if (di.value == 0) {
            CHECK(reg.all_of<UiActionDifficultyEasyTag>(e));
        } else if (di.value == 1) {
            CHECK(reg.all_of<UiActionDifficultyMediumTag>(e));
        } else {
            CHECK(reg.all_of<UiActionDifficultyHardTag>(e));
        }
    }
    CHECK(total
          == content_config::LEVEL_COUNT * content_config::DIFFICULTY_COUNT);
    for (int l = 0; l < content_config::LEVEL_COUNT; ++l) {
        for (int d = 0; d < content_config::DIFFICULTY_COUNT; ++d) {
            CHECK(per_pair[l][d] == 1);
        }
    }
}

TEST_CASE("ui_update_system: Level Select StartButton press latches "
          "LevelSelectConfirmedTag (#1296)",
          "[ui][ui_update_system][issue1296]") {
    entt::registry reg = make_registry();
    prime_level_select_entry(reg);

    CHECK_FALSE(reg.ctx().contains<LevelSelectConfirmedTag>());

    auto& input = reg.ctx().get<InputState>();
    // StartButton bounds from level_select.rgl: (210, 1050, 300, 60).
    click_at(input, 360.0f, 1080.0f);

    ui_update_system(reg);

    CHECK(reg.ctx().contains<LevelSelectConfirmedTag>());
    CHECK_FALSE(any_next_phase_pending(reg));  // gated by game_state_system
}

TEST_CASE("ui_update_system: Level Select card press writes LevelSelectState::selected_level (#1296)",
          "[ui][ui_update_system][issue1296]") {
    entt::registry reg = make_registry();
    prime_level_select_entry(reg);
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    // Card 2 lives at y = 200 + 2*(200+40) = 680, x = 110, h = 200, w = 500.
    // Click within the card's bounds but NOT in the difficulty-button band
    // (which sits at y=800-850 — well inside the card rect by design).
    // Use the very top of card 2 (y≈700) for an unambiguous card-only hit.
    auto& input = reg.ctx().get<InputState>();
    click_at(input, 360.0f, 700.0f);

    ui_update_system(reg);

    CHECK(lss.selected_level == 2);
}

TEST_CASE("ui_update_system: Level Select difficulty press writes "
          "LevelSelectState::selected_difficulty (#1296)",
          "[ui][ui_update_system][issue1296]") {
    entt::registry reg = make_registry();
    prime_level_select_entry(reg);
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;
    lss.selected_difficulty = 1;  // medium

    // Easy button under card 0: x=160 y=320 w=120 h=50.
    auto& input = reg.ctx().get<InputState>();
    click_at(input, 220.0f, 345.0f);

    ui_update_system(reg);

    CHECK(lss.selected_difficulty == 0);

    // Now Hard under same card: x=160+2*(120+30)=460, y=320, w=120, h=50.
    click_at(input, 520.0f, 345.0f);
    ui_update_system(reg);
    CHECK(lss.selected_difficulty == 2);
}

TEST_CASE("ui_update_system: Level Select difficulty press on non-active card "
          "is ignored (#1296)",
          "[ui][ui_update_system][issue1296]") {
    entt::registry reg = make_registry();
    prime_level_select_entry(reg);
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;
    lss.selected_difficulty = 1;  // medium

    // Click the Easy button under card 2 (level_index=2), but the active
    // level is 0. The render path won't show this button, but if a stale
    // click ever reaches dispatch the handler must no-op — pinned here.
    // Card 2's diff row sits at y = 680 + 120 = 800.
    auto& input = reg.ctx().get<InputState>();
    click_at(input, 220.0f, 825.0f);

    ui_update_system(reg);

    // Selection unchanged.
    CHECK(lss.selected_difficulty == 1);
    // The card-2 click region overlaps the card's rectangle (y=680..880), so
    // the level-card hit-test wins (first hit wins in ui_update_system).
    // Verify the side-effect of the card press: selected_level moves to 2.
    CHECK(lss.selected_level == 2);
}

TEST_CASE("ui_update_system: Level Select click before debounce ignored (#1296)",
          "[ui][ui_update_system][issue1296]") {
    entt::registry reg = make_registry();
    sync_game_phase_tags<GamePhaseLevelSelectTag>(reg);
    screen_lifecycle_system(reg);
    reg.ctx().get<GameState>().phase_timer = 0.0f;
    clear_next_phase_tags(reg);
    reg.ctx().erase<LevelSelectConfirmedTag>();

    auto& input = reg.ctx().get<InputState>();
    click_at(input, 360.0f, 1080.0f);  // StartButton centre

    ui_update_system(reg);

    CHECK_FALSE(reg.ctx().contains<LevelSelectConfirmedTag>());
}

TEST_CASE("ui_update_system: LevelSelectButton on end-screens still emplaces "
          "EndChoiceLevelSelect (#1296 regression guard)",
          "[ui][ui_update_system][issue1296]") {
    entt::registry reg = make_registry();
    sync_game_phase_tags<GamePhaseGameOverTag>(reg);
    screen_lifecycle_system(reg);
    reg.ctx().get<GameState>().phase_timer = constants::GAME_OVER_INPUT_DELAY + 0.05f;
    clear_next_phase_tags(reg);
    reg.ctx().erase<EndChoiceLevelSelect>();

    auto& input = reg.ctx().get<InputState>();
    // Game Over LevelSelect button bounds from game_over.rgl: (220, 935, 280, 50).
    click_at(input, 360.0f, 960.0f);

    ui_update_system(reg);

    CHECK(reg.ctx().contains<EndChoiceLevelSelect>());
    // Must NOT have written LevelSelectState — that path is only valid on
    // the Level Select screen itself.
    auto* lss = reg.ctx().find<LevelSelectState>();
    if (lss != nullptr) {
        CHECK(lss->selected_level == 0);
    }
}
