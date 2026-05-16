// Level Select screen controller - renders the static rguilayout heading and Start button.
// Dynamic card/difficulty rendering to be ported to rguilayout in future work.

#include "../../components/game_state.h"
#include "../../util/level_content_config.h"
#include "../../systems/input.h"
#include "../../constants.h"
#include "screen_controller_base.h"
#include <entt/entt.hpp>
#include <cstdio>

#include <raygui.h>
#include "../generated/level_select_screen_layout.h"

namespace {

using LevelSelectController = RGuiScreenController<LevelSelectScreenLayoutState,
                                                    &LevelSelectScreenLayout_Init,
                                                    &LevelSelectScreenLayout_Render>;

// Level card rendering constants
constexpr float CARD_X = 110.0f;
constexpr float CARD_W = 500.0f;
constexpr float CARD_H = 200.0f;
constexpr float CARD_START_Y = 200.0f;
constexpr float CARD_GAP = 40.0f;
constexpr float CARD_CORNER_RADIUS = 0.1f;

constexpr float TITLE_OFFSET_X = 30.0f;
constexpr float TITLE_OFFSET_Y = 30.0f;

constexpr float DIFF_Y_OFFSET = 120.0f;
constexpr float DIFF_BTN_W = 120.0f;
constexpr float DIFF_BTN_H = 50.0f;
constexpr float DIFF_BTN_GAP = 30.0f;
constexpr float DIFF_START_X = CARD_X + 50.0f;

// Keep style colors immutable to avoid hidden mutable globals.
constexpr Color SELECTED_BG = {40, 40, 60, 255};
constexpr Color UNSELECTED_BG = {20, 20, 30, 255};
constexpr Color SELECTED_BORDER = {100, 150, 255, 255};
constexpr Color UNSELECTED_BORDER = {60, 60, 80, 255};

// Difficulty-button active-state emphasis (#469). The selected difficulty
// is painted *after* GuiButton with a thick border + inset selection bar
// because raygui's BUTTON style otherwise overdraws any background drawn
// before GuiButton(). Two non-color cues satisfy A11Y/2.1.3.
constexpr Color DIFF_ACTIVE_BG = {80, 120, 200, 255};
constexpr Color DIFF_ACTIVE_BORDER = {120, 180, 255, 255};

Rectangle level_card_rect(int index) {
    return {CARD_X, CARD_START_Y + static_cast<float>(index) * (CARD_H + CARD_GAP), CARD_W, CARD_H};
}

Rectangle difficulty_button_rect(int index, int selected_level) {
    const float card_y = CARD_START_Y + static_cast<float>(selected_level) * (CARD_H + CARD_GAP);
    const float diff_y = card_y + DIFF_Y_OFFSET;
    return {
        DIFF_START_X + static_cast<float>(index) * (DIFF_BTN_W + DIFF_BTN_GAP),
        diff_y,
        DIFF_BTN_W,
        DIFF_BTN_H
    };
}

void handle_level_card_pointer_input(LevelSelectState& lss, const InputState& input) {
    Vector2 pointer = {};
    if (!(input.click || input.touch_up)) return;
    pointer = {input.end_x, input.end_y};

    for (int i = 0; i < content_config::LEVEL_COUNT; ++i) {
        if (CheckCollisionPointRec(pointer, level_card_rect(i))) {
            lss.selected_level = i;
            return;
        }
    }
}

} // anonymous namespace

void init_level_select_screen_ui() {
    // Controller state is registry-owned and initialized lazily in render.
}

void render_level_select_screen_ui(entt::registry& reg) {
    auto& controller = screen_controller<LevelSelectController>(reg);
    auto& lss = reg.ctx().get<LevelSelectState>();
    const auto& input = reg.ctx().get<InputState>();
    auto& state = controller.state();
    auto& gs = reg.ctx().get<GameState>();
    handle_level_card_pointer_input(lss, input);
    
    int saved_text_size = GuiGetStyle(DEFAULT, TEXT_SIZE);
    
    // Runtime override: Render heading at readable size (generated layout has 48px height)
    GuiSetStyle(DEFAULT, TEXT_SIZE, 40);
    GuiLabel(Rectangle{ state.Anchor01.x + 180, state.Anchor01.y + 80, 360, 60 }, "SELECT LEVEL");
    
    // Draw level cards
    for (int i = 0; i < content_config::LEVEL_COUNT; ++i) {
        float cy = CARD_START_Y + static_cast<float>(i) * (CARD_H + CARD_GAP);
        bool selected = (i == lss.selected_level);
        Color bg = selected ? SELECTED_BG : UNSELECTED_BG;
        Color border = selected ? SELECTED_BORDER : UNSELECTED_BORDER;
        
        DrawRectangleRounded(
            {CARD_X, cy, CARD_W, CARD_H},
            CARD_CORNER_RADIUS, 4, bg);
        DrawRectangleRoundedLinesEx(
            {CARD_X, cy, CARD_W, CARD_H},
            CARD_CORNER_RADIUS, 4, 2.0f, border);
        
        // Draw level title
        GuiSetStyle(DEFAULT, TEXT_SIZE, 32);
        GuiSetAlpha(selected ? 1.0f : 0.6f);
        GuiLabel(Rectangle{CARD_X + TITLE_OFFSET_X, cy + TITLE_OFFSET_Y, 400, 40},
                 content_config::LEVELS[i].title);
        GuiSetAlpha(1.0f);
        
        // Draw track number
        char track_num[4];
        std::snprintf(track_num, sizeof(track_num), "%d", i + 1);
        GuiSetStyle(DEFAULT, TEXT_SIZE, 32);
        GuiSetAlpha(0.4f);
        GuiLabel(Rectangle{CARD_X + CARD_W - 60.0f, cy + TITLE_OFFSET_Y, 50, 40}, track_num);
        GuiSetAlpha(1.0f);
        
        // Draw difficulty buttons for selected card
        if (selected) {
            GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
            for (int dd = 0; dd < content_config::DIFFICULTY_COUNT; ++dd) {
                const Rectangle diff_rect = difficulty_button_rect(dd, lss.selected_level);
                const bool active = (dd == lss.selected_difficulty);

                if (GuiButton(diff_rect, content_config::DIFFICULTY_NAMES[dd])) {
                    lss.selected_difficulty = dd;
                }

                // Active-state emphasis painted *after* GuiButton so it is
                // not overdrawn by raygui's BUTTON style (#469). Two
                // non-color cues per A11Y/2.1.3: thick highlight outline
                // and an inset bottom selection bar.
                if (active) {
                    DrawRectangleRoundedLinesEx(diff_rect, 0.1f, 4, 3.0f, DIFF_ACTIVE_BORDER);
                    constexpr float bar_inset_x = 6.0f;
                    constexpr float bar_height   = 3.0f;
                    constexpr float bar_offset_y = 6.0f;
                    const Rectangle sel_bar = {
                        diff_rect.x + bar_inset_x,
                        diff_rect.y + diff_rect.height - bar_offset_y,
                        diff_rect.width - bar_inset_x * 2.0f,
                        bar_height
                    };
                    DrawRectangleRec(sel_bar, DIFF_ACTIVE_BG);
                }
            }
        }
    }
    
    // Render Start button manually at bottom
    GuiSetStyle(DEFAULT, TEXT_SIZE, 24);
    state.StartButtonPressed = GuiButton(
        Rectangle{ state.Anchor01.x + 210, state.Anchor01.y + 1050, 300, 60 }, "START");
    
    GuiSetStyle(DEFAULT, TEXT_SIZE, saved_text_size);

    if (state.StartButtonPressed && gs.phase_timer > constants::UI_ENTRY_DEBOUNCE) {
        reg.ctx().insert_or_assign(LevelSelectConfirmedTag{});
    }
}
