#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/player.h"
#include "../components/rendering.h"
#include "../components/rhythm.h"
#include "../constants.h"
#include "../util/shape_lane_mapping.h"
#include "../util/shape_tag.h"

namespace {

static bool apply_shape_color(entt::registry& reg, entt::entity entity, Shape shape) {
    const int si = shape_index(shape);
    if (si < 0) {
        TraceLog(LOG_WARNING, "shape_window_system rejected invalid shape %d",
                 static_cast<int>(shape));
        return false;
    }
    const auto& sc = constants::SHAPE_COLORS[si];
    reg.replace<Color>(entity, sc);
    return true;
}

// Phase-transition primitive. Existential processing per #1202/#1204:
// each phase is its own zero-column tag, transitions swap tags, Idle = no tags.
template <typename From, typename To>
void transition_phase(entt::registry& reg, entt::entity e) {
    reg.remove<From>(e);
    reg.emplace<To>(e);
}

template <typename From>
void transition_to_idle(entt::registry& reg, entt::entity e) {
    reg.remove<From>(e);
}

void on_morph_in_failure(entt::registry& reg,
                         entt::entity entity,
                         PlayerShape& /*pshape*/,
                         ShapeWindow& /*swindow*/) {
    set_player_shape_tag(reg, entity, Shape::Hexagon);
    set_target_shape_tag(reg, entity, Shape::Hexagon);
    transition_to_idle<ShapeWindowMorphInTag>(reg, entity);
}

void advance_morph_in(entt::registry& reg,
                      entt::entity entity,
                      PlayerShape& pshape,
                      ShapeWindow& swindow,
                      const SongState& song) {
    const float elapsed = song.song_time - swindow.window_start;
    swindow.window_timer = elapsed;
    const Shape target = current_target_shape(reg, entity);
    if (song.morph_duration <= 0.0f) {
        TraceLog(LOG_WARNING, "shape_window_system completed MorphIn immediately: invalid morph_duration %.3f",
                 song.morph_duration);
        pshape.morph_t = 1.0f;
        swindow.window_start = song.song_time;
        swindow.window_timer = 0.0f;
        if (!apply_shape_color(reg, entity, target)) {
            on_morph_in_failure(reg, entity, pshape, swindow);
            return;
        }
        set_player_shape_tag(reg, entity, target);
        transition_phase<ShapeWindowMorphInTag, ShapeWindowActiveTag>(reg, entity);
        return;
    }

    pshape.morph_t = elapsed / song.morph_duration;
    if (pshape.morph_t >= 1.0f) {
        pshape.morph_t = 1.0f;
        swindow.window_start = swindow.window_start + song.morph_duration;
        swindow.window_timer = 0.0f;
        if (!apply_shape_color(reg, entity, target)) {
            on_morph_in_failure(reg, entity, pshape, swindow);
            return;
        }
        set_player_shape_tag(reg, entity, target);
        transition_phase<ShapeWindowMorphInTag, ShapeWindowActiveTag>(reg, entity);
    }
}

void advance_active(entt::registry& reg,
                    entt::entity entity,
                    PlayerShape& pshape,
                    ShapeWindow& swindow,
                    const SongState& song) {
    const float active_elapsed = song.song_time - swindow.window_start;
    swindow.window_timer = active_elapsed;
    const float effective_duration = song.window_duration;
    if (active_elapsed >= effective_duration) {
        swindow.window_start = swindow.window_start + effective_duration;
        swindow.window_timer = 0.0f;
        pshape.morph_t = 0.0f;
        transition_phase<ShapeWindowActiveTag, ShapeWindowMorphOutTag>(reg, entity);
    }
}

void advance_morph_out(entt::registry& reg,
                       entt::entity entity,
                       PlayerShape& pshape,
                       ShapeWindow& swindow,
                       const SongState& song) {
    const float morph_elapsed = song.song_time - swindow.window_start;
    swindow.window_timer = morph_elapsed;
    if (song.morph_duration <= 0.0f) {
        TraceLog(LOG_WARNING, "shape_window_system completed MorphOut immediately: invalid morph_duration %.3f",
                 song.morph_duration);
        pshape.morph_t = 1.0f;
        set_player_shape_tag(reg, entity, Shape::Hexagon);
        set_target_shape_tag(reg, entity, Shape::Hexagon);
        swindow.window_timer = 0.0f;
        swindow.press_time = -1.0f;
        swindow.graded = false;
        apply_shape_color(reg, entity, Shape::Hexagon);
        transition_to_idle<ShapeWindowMorphOutTag>(reg, entity);
        return;
    }
    pshape.morph_t = morph_elapsed / song.morph_duration;
    if (pshape.morph_t >= 1.0f) {
        pshape.morph_t = 1.0f;
        set_player_shape_tag(reg, entity, Shape::Hexagon);
        set_target_shape_tag(reg, entity, Shape::Hexagon);
        swindow.window_timer = 0.0f;
        swindow.press_time = -1.0f;
        swindow.graded = false;
        apply_shape_color(reg, entity, Shape::Hexagon);
        transition_to_idle<ShapeWindowMorphOutTag>(reg, entity);
    }
}

// Per-tag transforms (Fabian existential processing). Each loop touches only
// entities carrying the matching phase tag; Idle = no phase tag = no transform.
// Entities are snapshotted before the loop body so phase-transition writes
// don't mutate the view mid-iteration.

template <typename PhaseTag>
entt::entity find_player_in_phase(entt::registry& reg) {
    auto view = reg.view<PlayerTag, PlayerShape, ShapeWindow, Color, PhaseTag>();
    for (auto e : view) return e;  // single-player invariant
    return entt::null;
}

template <typename UpdateFn>
void apply_phase_update(entt::registry& reg,
                        entt::entity entity,
                        const SongState& song,
                        UpdateFn update) {
    if (entity == entt::null) return;
    auto& pshape  = reg.get<PlayerShape>(entity);
    auto& swindow = reg.get<ShapeWindow>(entity);
    update(reg, entity, pshape, swindow, song);
}

}  // namespace

void shape_window_activation_system(entt::registry& reg, [[maybe_unused]] float dt) {
    auto* song = reg.ctx().find<SongState>();
    if (!song) return;
    apply_phase_update(reg, find_player_in_phase<ShapeWindowMorphInTag>(reg),
                       *song, advance_morph_in);
}

void shape_window_system(entt::registry& reg, [[maybe_unused]] float dt) {
    auto* song = reg.ctx().find<SongState>();
    if (!song) return;
    // Snapshot phase-membership BEFORE any transform runs. An entity that
    // transitions MorphIn→Active inside its transform must NOT immediately
    // run the Active body in the same call — that would compress two frames
    // of state advancement into one and break the pre-migration switch
    // semantics. Capturing entities up front gives each entity at most one
    // phase advance per call, exactly matching the switch-on-phase contract.
    //
    // Derives window_timer from song_time rather than accumulating dt — keeps
    // windows frame-rate independent and synced to the audio clock per the
    // "only use song position" rule.
    const auto morph_in_e  = find_player_in_phase<ShapeWindowMorphInTag>(reg);
    const auto active_e    = find_player_in_phase<ShapeWindowActiveTag>(reg);
    const auto morph_out_e = find_player_in_phase<ShapeWindowMorphOutTag>(reg);
    apply_phase_update(reg, morph_in_e,  *song, advance_morph_in);
    apply_phase_update(reg, active_e,    *song, advance_active);
    apply_phase_update(reg, morph_out_e, *song, advance_morph_out);
}
