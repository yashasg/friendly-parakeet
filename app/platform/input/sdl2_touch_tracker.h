#pragma once

#include "../../constants.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "runtime/runtime_api.h"

namespace platform::input {

class Sdl2TouchTracker {
public:
    static constexpr std::size_t kMaxTouches = 2;

    void reset() {
        slots_ = {};
        detected_gesture_ = GESTURE_NONE;
        last_gesture_timestamp_ms_ = 0;
        gestures_enabled_ = false;
    }

    void configure_gameplay_gestures() {
        gestures_enabled_ = true;
        detected_gesture_ = GESTURE_NONE;
        last_gesture_timestamp_ms_ = 0;
    }

    void on_finger_down(std::int64_t finger_id, float x, float y, std::uint32_t timestamp_ms) {
        TouchSlot* slot = find_slot(finger_id);
        if (!slot) {
            slot = first_free_slot();
        }
        if (!slot) return;
        slot->active = true;
        slot->finger_id = finger_id;
        slot->start_x = x;
        slot->start_y = y;
        slot->curr_x = x;
        slot->curr_y = y;
        slot->down_timestamp_ms = timestamp_ms;
    }

    void on_finger_motion(std::int64_t finger_id, float x, float y, std::uint32_t /*timestamp_ms*/) {
        TouchSlot* slot = find_slot(finger_id);
        if (!slot || !slot->active) return;
        slot->curr_x = x;
        slot->curr_y = y;
    }

    void on_finger_up(std::int64_t finger_id, float x, float y, std::uint32_t timestamp_ms) {
        TouchSlot* slot = find_slot(finger_id);
        if (!slot || !slot->active) return;
        slot->curr_x = x;
        slot->curr_y = y;

        if (gestures_enabled_) {
            detected_gesture_ = classify_gesture(*slot, timestamp_ms);
            last_gesture_timestamp_ms_ = timestamp_ms;
        }

        *slot = {};
    }

    [[nodiscard]] int touch_point_count() const {
        int count = 0;
        for (const auto& slot : slots_) {
            if (slot.active) ++count;
        }
        return count;
    }

    [[nodiscard]] bool touch_position(int index, float& x, float& y) const {
        if (index < 0) return false;
        int active_index = 0;
        for (const auto& slot : slots_) {
            if (!slot.active) continue;
            if (active_index == index) {
                x = slot.curr_x;
                y = slot.curr_y;
                return true;
            }
            ++active_index;
        }
        return false;
    }

    [[nodiscard]] int consume_detected_gesture() {
        const int gesture = detected_gesture_;
        detected_gesture_ = GESTURE_NONE;
        return gesture;
    }

    [[nodiscard]] std::uint32_t last_gesture_timestamp_ms() const {
        return last_gesture_timestamp_ms_;
    }

private:
    struct TouchSlot {
        bool active = false;
        std::int64_t finger_id = 0;
        float start_x = 0.0f;
        float start_y = 0.0f;
        float curr_x = 0.0f;
        float curr_y = 0.0f;
        std::uint32_t down_timestamp_ms = 0;
    };

    [[nodiscard]] TouchSlot* find_slot(std::int64_t finger_id) {
        for (auto& slot : slots_) {
            if (slot.active && slot.finger_id == finger_id) return &slot;
        }
        return nullptr;
    }

    [[nodiscard]] TouchSlot* first_free_slot() {
        for (auto& slot : slots_) {
            if (!slot.active) return &slot;
        }
        return nullptr;
    }

    [[nodiscard]] static int classify_gesture(const TouchSlot& slot, std::uint32_t release_timestamp_ms) {
        const float dx = slot.curr_x - slot.start_x;
        const float dy = slot.curr_y - slot.start_y;
        const float distance = std::sqrt((dx * dx) + (dy * dy));
        const std::uint32_t duration_ms =
            (release_timestamp_ms >= slot.down_timestamp_ms)
            ? (release_timestamp_ms - slot.down_timestamp_ms)
            : 0;
        const float duration_sec = static_cast<float>(duration_ms) / 1000.0f;

        if (distance >= constants::MIN_SWIPE_DIST &&
            duration_sec <= constants::MAX_SWIPE_TIME) {
            if (std::fabs(dx) >= std::fabs(dy)) {
                return (dx >= 0.0f) ? GESTURE_SWIPE_RIGHT : GESTURE_SWIPE_LEFT;
            }
            return (dy >= 0.0f) ? GESTURE_SWIPE_DOWN : GESTURE_SWIPE_UP;
        }
        return GESTURE_TAP;
    }

    std::array<TouchSlot, kMaxTouches> slots_{};
    int detected_gesture_ = GESTURE_NONE;
    std::uint32_t last_gesture_timestamp_ms_ = 0;
    bool gestures_enabled_ = false;
};

}  // namespace platform::input
