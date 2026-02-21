#ifndef DEIMOS_CONFIG_H
#define DEIMOS_CONFIG_H

#include <stdint.h>

#define DEIMOS_MAX_BINDS 32
#define DEIMOS_BIND_ARG_MAX 128

enum deimos_bind_action {
    DEIMOS_BIND_ACTION_NONE = 0,
    DEIMOS_BIND_ACTION_LAUNCH = 1,
    DEIMOS_BIND_ACTION_NEW_WINDOW = 2,
    DEIMOS_BIND_ACTION_CLOSE_FOCUSED = 3,
    DEIMOS_BIND_ACTION_QUIT_DEIMOS = 4
};

struct deimos_bind {
    uint8_t key;
    uint8_t modifiers;
    uint8_t action;
    char arg[DEIMOS_BIND_ARG_MAX];
};

struct deimos_config {
    char key_new_window;
    char key_quit;
    int mouse_new_window;
    int mouse_focus_follows_hover;
    int keyboard_split_use_focus;
    int drag_modifier_mask;
    int drag_preview_mode; // 0=full, 1=outline

    uint32_t background_color;
    uint32_t cursor_color;
    uint32_t fps_fg_color;
    uint32_t fps_bg_color;
    uint32_t window_border_color;
    uint32_t window_focus_color;

    int window_gap;
    int split_vertical_bias_percent;
    int split_force_mode; // 0=auto, 1=vertical(left/right), 2=horizontal(top/bottom)

    int bind_count;
    struct deimos_bind binds[DEIMOS_MAX_BINDS];
};

void deimos_config_set_defaults(struct deimos_config *cfg);
int deimos_config_load(struct deimos_config *cfg, const char *path);

#endif
