#ifndef DEIMOS_CONFIG_H
#define DEIMOS_CONFIG_H

#include <stdint.h>

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
};

void deimos_config_set_defaults(struct deimos_config *cfg);
int deimos_config_load(struct deimos_config *cfg, const char *path);

#endif
