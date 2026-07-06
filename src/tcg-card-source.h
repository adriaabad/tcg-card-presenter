#pragma once

#define TCG_CARD_SOURCE_ID "tcg_card_presenter_source"

#define TCG_CARD_SETTING_CARD_FILE "card_file"
#define TCG_CARD_SETTING_ENTER_MS "enter_ms"
#define TCG_CARD_SETTING_HOLD_MS "hold_ms"
#define TCG_CARD_SETTING_EXIT_MS "exit_ms"
#define TCG_CARD_SETTING_ENTER_ANIMATION "enter_animation"
#define TCG_CARD_SETTING_EXIT_ANIMATION "exit_animation"

#define TCG_CARD_ANIMATION_FADE "fade"
#define TCG_CARD_ANIMATION_SLIDE_LEFT "slide-left"
#define TCG_CARD_ANIMATION_SLIDE_RIGHT "slide-right"
#define TCG_CARD_ANIMATION_SLIDE_UP "slide-up"
#define TCG_CARD_ANIMATION_SLIDE_DOWN "slide-down"
#define TCG_CARD_ANIMATION_ZOOM "zoom"
#define TCG_CARD_ANIMATION_CUT "cut"

void tcg_card_source_register(void);
