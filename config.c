#include "config.h"
#include <libsys.h>

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static char to_lower_ascii(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

static int str_eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static void trim_in_place(char *text) {
    if (!text) return;

    int start = 0;
    while (text[start] && is_space(text[start])) {
        start++;
    }

    int end = start;
    while (text[end]) {
        end++;
    }
    while (end > start && is_space(text[end - 1])) {
        end--;
    }

    int out = 0;
    for (int i = start; i < end; i++) {
        text[out++] = text[i];
    }
    text[out] = '\0';
}

static int parse_hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = to_lower_ascii(c);
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return -1;
}

static int parse_u32(const char *text, uint32_t *out_value) {
    if (!text || !out_value) return 0;

    int base = 10;
    int idx = 0;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        idx = 2;
    }

    if (!text[idx]) return 0;

    uint32_t value = 0;
    while (text[idx]) {
        int digit;
        if (base == 16) {
            digit = parse_hex_digit(text[idx]);
        } else {
            if (text[idx] < '0' || text[idx] > '9') return 0;
            digit = text[idx] - '0';
        }
        if (digit < 0 || digit >= base) return 0;
        value = (uint32_t)(value * (uint32_t)base + (uint32_t)digit);
        idx++;
    }

    *out_value = value;
    return 1;
}

static int parse_bool(const char *text, int *out_value) {
    if (!text || !out_value) return 0;
    if (str_eq(text, "1") || str_eq(text, "true") || str_eq(text, "yes") || str_eq(text, "on")) {
        *out_value = 1;
        return 1;
    }
    if (str_eq(text, "0") || str_eq(text, "false") || str_eq(text, "no") || str_eq(text, "off")) {
        *out_value = 0;
        return 1;
    }
    return 0;
}

static int parse_split_force_mode(const char *text, int *out_mode) {
    if (!text || !out_mode) return 0;
    if (str_eq(text, "auto")) {
        *out_mode = 0;
        return 1;
    }
    if (str_eq(text, "vertical")) {
        *out_mode = 1;
        return 1;
    }
    if (str_eq(text, "horizontal")) {
        *out_mode = 2;
        return 1;
    }
    return 0;
}

static int parse_drag_modifier(const char *text, int *out_mask) {
    if (!text || !out_mask) return 0;
    if (str_eq(text, "none")) {
        *out_mask = 0;
        return 1;
    }
    if (str_eq(text, "shift")) {
        *out_mask = MOD_SHIFT;
        return 1;
    }
    if (str_eq(text, "ctrl") || str_eq(text, "control")) {
        *out_mask = MOD_CTRL;
        return 1;
    }
    if (str_eq(text, "alt")) {
        *out_mask = MOD_ALT;
        return 1;
    }
    if (str_eq(text, "super") || str_eq(text, "win") || str_eq(text, "meta")) {
        *out_mask = MOD_SUPER;
        return 1;
    }
    return 0;
}

static int parse_drag_preview_mode(const char *text, int *out_mode) {
    if (!text || !out_mode) return 0;
    if (str_eq(text, "full")) {
        *out_mode = 0;
        return 1;
    }
    if (str_eq(text, "outline")) {
        *out_mode = 1;
        return 1;
    }
    return 0;
}

static int parse_key(const char *text, char *out_key) {
    if (!text || !out_key) return 0;
    if (!text[0]) return 0;

    if (text[0] == '\'' && text[1] && text[2] == '\'') {
        *out_key = text[1];
        return 1;
    }
    if (text[0] == '"' && text[1] && text[2] == '"') {
        *out_key = text[1];
        return 1;
    }

    uint32_t numeric_key = 0;
    if (parse_u32(text, &numeric_key)) {
        *out_key = (char)(numeric_key & 0xFF);
        return 1;
    }

    *out_key = text[0];
    return 1;
}

void deimos_config_set_defaults(struct deimos_config *cfg) {
    if (!cfg) return;

    cfg->key_new_window = 'n';
    cfg->key_quit = 'x';
    cfg->mouse_new_window = 0;
    cfg->mouse_focus_follows_hover = 1;
    cfg->keyboard_split_use_focus = 1;
    cfg->drag_modifier_mask = MOD_SUPER;
    cfg->drag_preview_mode = 0;

    cfg->background_color = 0x101820;
    cfg->cursor_color = 0xFFFFFF;
    cfg->fps_fg_color = 0xE6E6E6;
    cfg->fps_bg_color = 0x000000;
    cfg->window_border_color = 0x00AA66;
    cfg->window_focus_color = 0xFFFFFF;

    cfg->window_gap = 6;
    cfg->split_vertical_bias_percent = 160;
    cfg->split_force_mode = 0;
}

int deimos_config_load(struct deimos_config *cfg, const char *path) {
    if (!cfg || !path) return -1;

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    char buf[2048];
    int used = 0;
    while (used < (int)sizeof(buf) - 1) {
        int n = read(fd, &buf[used], (int)sizeof(buf) - 1 - used);
        if (n <= 0) break;
        used += n;
    }
    close(fd);
    buf[used] = '\0';

    int i = 0;
    while (i < used) {
        int line_start = i;
        while (i < used && buf[i] != '\n') i++;
        int line_end = i;
        if (i < used && buf[i] == '\n') i++;

        buf[line_end] = '\0';
        char *line = &buf[line_start];
        trim_in_place(line);
        if (!line[0] || line[0] == '#') continue;

        int eq = 0;
        while (line[eq] && line[eq] != '=') eq++;
        if (line[eq] != '=') continue;

        line[eq] = '\0';
        char *key = line;
        char *value = &line[eq + 1];
        trim_in_place(key);
        trim_in_place(value);
        if (!key[0] || !value[0]) continue;

        uint32_t u32_value = 0;
        int int_value = 0;
        char key_value = 0;

        if (str_eq(key, "key_new_window")) {
            if (parse_key(value, &key_value)) cfg->key_new_window = key_value;
        } else if (str_eq(key, "key_quit")) {
            if (parse_key(value, &key_value)) cfg->key_quit = key_value;
        } else if (str_eq(key, "mouse_new_window")) {
            if (parse_bool(value, &int_value)) cfg->mouse_new_window = int_value;
        } else if (str_eq(key, "mouse_focus_follows_hover")) {
            if (parse_bool(value, &int_value)) cfg->mouse_focus_follows_hover = int_value;
        } else if (str_eq(key, "keyboard_split_use_focus")) {
            if (parse_bool(value, &int_value)) cfg->keyboard_split_use_focus = int_value;
        } else if (str_eq(key, "drag_modifier")) {
            if (parse_drag_modifier(value, &int_value)) cfg->drag_modifier_mask = int_value;
        } else if (str_eq(key, "drag_preview_mode")) {
            if (parse_drag_preview_mode(value, &int_value)) cfg->drag_preview_mode = int_value;
        } else if (str_eq(key, "background_color")) {
            if (parse_u32(value, &u32_value)) cfg->background_color = u32_value;
        } else if (str_eq(key, "cursor_color")) {
            if (parse_u32(value, &u32_value)) cfg->cursor_color = u32_value;
        } else if (str_eq(key, "fps_fg_color")) {
            if (parse_u32(value, &u32_value)) cfg->fps_fg_color = u32_value;
        } else if (str_eq(key, "fps_bg_color")) {
            if (parse_u32(value, &u32_value)) cfg->fps_bg_color = u32_value;
        } else if (str_eq(key, "window_border_color")) {
            if (parse_u32(value, &u32_value)) cfg->window_border_color = u32_value;
        } else if (str_eq(key, "window_focus_color")) {
            if (parse_u32(value, &u32_value)) cfg->window_focus_color = u32_value;
        } else if (str_eq(key, "window_gap")) {
            if (parse_u32(value, &u32_value)) cfg->window_gap = (int)u32_value;
        } else if (str_eq(key, "split_vertical_bias_percent")) {
            if (parse_u32(value, &u32_value)) cfg->split_vertical_bias_percent = (int)u32_value;
        } else if (str_eq(key, "split_force_mode")) {
            if (parse_split_force_mode(value, &int_value)) cfg->split_force_mode = int_value;
        }
    }

    if (cfg->window_gap < 0) cfg->window_gap = 0;
    if (cfg->window_gap > 64) cfg->window_gap = 64;
    if (cfg->split_vertical_bias_percent < 50) cfg->split_vertical_bias_percent = 50;
    if (cfg->split_vertical_bias_percent > 400) cfg->split_vertical_bias_percent = 400;
    if (cfg->split_force_mode < 0 || cfg->split_force_mode > 2) cfg->split_force_mode = 0;
    if (cfg->drag_modifier_mask < 0 || cfg->drag_modifier_mask > (MOD_SHIFT | MOD_CTRL | MOD_ALT | MOD_SUPER)) {
        cfg->drag_modifier_mask = MOD_SUPER;
    }
    if (cfg->drag_preview_mode < 0 || cfg->drag_preview_mode > 1) {
        cfg->drag_preview_mode = 0;
    }

    return 0;
}
