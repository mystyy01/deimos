#include "config.h"
#include "rendering/rendering.h"
#include "window_manager/state.h"
#include <libsys.h>

extern int deimos_compositor_test_frame_with_count(int window_count);
extern void mt_heap_reset(void);

static struct deimos_config g_cfg;

#define DEIMOS_MAX_REPORT_WINDOWS 16
#define DEIMOS_SURFACE_W 48
#define DEIMOS_SURFACE_H 32

struct deimos_window_rect {
    int id;
    int x;
    int y;
    int w;
    int h;
    int valid;
};

static struct deimos_window_rect g_prev_window_rects[DEIMOS_MAX_REPORT_WINDOWS];
static struct deimos_window_rect g_curr_window_rects[DEIMOS_MAX_REPORT_WINDOWS];
static int g_prev_window_rect_count;
static int g_curr_window_rect_count;
static int g_should_draw_windows = 1;
static int g_drag_active = 0;
static int g_drag_window_id = -1;

struct deimos_window_surface {
    int initialized;
    uint32_t pixels[DEIMOS_SURFACE_W * DEIMOS_SURFACE_H];
};

static struct deimos_window_surface g_surfaces[DEIMOS_MAX_REPORT_WINDOWS + 1];

static uint32_t colour_rgb(int r, int g, int b) {
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void init_window_surface(int window_id) {
    if (window_id <= 0 || window_id > DEIMOS_MAX_REPORT_WINDOWS) return;

    struct deimos_window_surface *s = &g_surfaces[window_id];
    if (s->initialized) return;

    int base_r = 40 + ((window_id * 53) % 120);
    int base_g = 60 + ((window_id * 31) % 120);
    int base_b = 80 + ((window_id * 19) % 120);

    for (int y = 0; y < DEIMOS_SURFACE_H; y++) {
        for (int x = 0; x < DEIMOS_SURFACE_W; x++) {
            int idx = y * DEIMOS_SURFACE_W + x;
            int checker = (((x / 4) + (y / 4)) & 1) ? 10 : -6;
            int glow = (x * 18) / DEIMOS_SURFACE_W;
            int shade = (y * 20) / DEIMOS_SURFACE_H;
            int r = base_r + glow + checker;
            int g = base_g + (shade / 2) + checker;
            int b = base_b + shade - checker;
            s->pixels[idx] = colour_rgb(r, g, b);
        }
    }

    // Add a small deterministic accent tile so each surface feels distinct.
    int tile_x = 4 + ((window_id * 7) % 20);
    int tile_y = 4 + ((window_id * 5) % 12);
    for (int y = tile_y; y < tile_y + 8 && y < DEIMOS_SURFACE_H; y++) {
        for (int x = tile_x; x < tile_x + 14 && x < DEIMOS_SURFACE_W; x++) {
            s->pixels[y * DEIMOS_SURFACE_W + x] = colour_rgb(230, 230, 240);
        }
    }

    s->initialized = 1;
}

static void deimos_draw_window_surface_full(int window_id, int x, int y, int w, int h, int focused) {
    if (window_id <= 0 || window_id > DEIMOS_MAX_REPORT_WINDOWS) return;
    if (w <= 2 || h <= 2) return;

    init_window_surface(window_id);
    struct deimos_window_surface *s = &g_surfaces[window_id];
    if (!s->initialized) return;

    int inner_x = x + 1;
    int inner_y = y + 1;
    int inner_w = w - 2;
    int inner_h = h - 2;
    if (inner_w <= 0 || inner_h <= 0) return;

    for (int sy = 0; sy < DEIMOS_SURFACE_H; sy++) {
        int y0 = inner_y + (sy * inner_h) / DEIMOS_SURFACE_H;
        int y1 = inner_y + ((sy + 1) * inner_h) / DEIMOS_SURFACE_H;
        if (y1 <= y0) continue;

        for (int sx = 0; sx < DEIMOS_SURFACE_W; sx++) {
            int x0 = inner_x + (sx * inner_w) / DEIMOS_SURFACE_W;
            int x1 = inner_x + ((sx + 1) * inner_w) / DEIMOS_SURFACE_W;
            if (x1 <= x0) continue;

            uint32_t c = s->pixels[sy * DEIMOS_SURFACE_W + sx];
            render_fill_rect(x0, y0, x1 - x0, y1 - y0, c);
        }
    }

    // Simple title strip to make content feel like a real surface.
    int strip_h = (inner_h > 14) ? 12 : (inner_h / 2);
    if (strip_h > 0) {
        uint32_t strip_col = focused ? colour_rgb(245, 245, 250) : colour_rgb(28, 32, 40);
        render_fill_rect(inner_x, inner_y, inner_w, strip_h, strip_col);
    }
}

static int clip_intersection(int ax, int ay, int aw, int ah,
                             int bx, int by, int bw, int bh,
                             int *ox, int *oy, int *ow, int *oh) {
    int x0 = (ax > bx) ? ax : bx;
    int y0 = (ay > by) ? ay : by;
    int x1 = ((ax + aw) < (bx + bw)) ? (ax + aw) : (bx + bw);
    int y1 = ((ay + ah) < (by + bh)) ? (ay + ah) : (by + bh);
    if (x1 <= x0 || y1 <= y0) return 0;
    *ox = x0;
    *oy = y0;
    *ow = x1 - x0;
    *oh = y1 - y0;
    return 1;
}

static void deimos_draw_window_clip(struct deimos_window_surface *s,
                                    int x, int y, int w, int h, int focused,
                                    int clip_x, int clip_y, int clip_w, int clip_h) {
    uint32_t border_col = focused ? g_cfg.window_focus_color : g_cfg.window_border_color;
    uint32_t strip_col = focused ? colour_rgb(245, 245, 250) : colour_rgb(28, 32, 40);

    int inner_x = x + 1;
    int inner_y = y + 1;
    int inner_w = w - 2;
    int inner_h = h - 2;
    int strip_h = (inner_h > 14) ? 12 : (inner_h / 2);

    int y_end = clip_y + clip_h;
    int x_end = clip_x + clip_w;
    for (int yy = clip_y; yy < y_end; yy++) {
        for (int xx = clip_x; xx < x_end; xx++) {
            int on_border = (xx == x) || (xx == (x + w - 1)) || (yy == y) || (yy == (y + h - 1));
            if (on_border) {
                render_putpixel(xx, yy, border_col);
                continue;
            }

            if (xx < inner_x || yy < inner_y || xx >= (inner_x + inner_w) || yy >= (inner_y + inner_h)) {
                continue;
            }

            int sx = ((xx - inner_x) * DEIMOS_SURFACE_W) / inner_w;
            int sy = ((yy - inner_y) * DEIMOS_SURFACE_H) / inner_h;
            if (sx < 0) sx = 0;
            if (sy < 0) sy = 0;
            if (sx >= DEIMOS_SURFACE_W) sx = DEIMOS_SURFACE_W - 1;
            if (sy >= DEIMOS_SURFACE_H) sy = DEIMOS_SURFACE_H - 1;

            uint32_t c = s->pixels[sy * DEIMOS_SURFACE_W + sx];
            if (strip_h > 0 && yy < (inner_y + strip_h)) {
                c = strip_col;
            }
            render_putpixel(xx, yy, c);
        }
    }
}

void deimos_draw_window_frame(int window_id, int x, int y, int w, int h, int focused) {
    if (window_id <= 0 || window_id > DEIMOS_MAX_REPORT_WINDOWS) return;
    if (w <= 1 || h <= 1) return;

    init_window_surface(window_id);
    struct deimos_window_surface *s = &g_surfaces[window_id];
    if (!s->initialized) return;

    if (render_is_full_dirty()) {
        deimos_draw_window_surface_full(window_id, x, y, w, h, focused);
        render_draw_rect(x, y, w, h, focused ? g_cfg.window_focus_color : g_cfg.window_border_color);
        return;
    }

    int dirty_count = render_dirty_count();
    for (int i = 0; i < dirty_count; i++) {
        int rx = render_dirty_x(i);
        int ry = render_dirty_y(i);
        int rw = render_dirty_w(i);
        int rh = render_dirty_h(i);

        int ix, iy, iw, ih;
        if (!clip_intersection(x, y, w, h, rx, ry, rw, rh, &ix, &iy, &iw, &ih)) {
            continue;
        }
        deimos_draw_window_clip(s, x, y, w, h, focused, ix, iy, iw, ih);
    }
}

int deimos_theme_window_color(void) {
    return (int)g_cfg.window_border_color;
}

int deimos_theme_window_focus_color(void) {
    return (int)g_cfg.window_focus_color;
}

int deimos_theme_gap(void) {
    return g_cfg.window_gap;
}

int deimos_split_vertical_bias_percent(void) {
    return g_cfg.split_vertical_bias_percent;
}

int deimos_split_force_mode(void) {
    return g_cfg.split_force_mode;
}

int deimos_should_draw_windows(void) {
    return g_should_draw_windows;
}

int deimos_should_draw_layout_window(int window_id) {
    if (g_drag_active && g_drag_window_id > 0 && window_id == g_drag_window_id) {
        return 0;
    }
    return 1;
}

void deimos_begin_window_report(void) {
    g_curr_window_rect_count = 0;
    for (int i = 0; i < DEIMOS_MAX_REPORT_WINDOWS; i++) {
        g_curr_window_rects[i].valid = 0;
    }
}

void deimos_report_window_rect(int index, int id, int x, int y, int w, int h) {
    if (index < 0 || index >= DEIMOS_MAX_REPORT_WINDOWS) {
        return;
    }

    g_curr_window_rects[index].id = id;
    g_curr_window_rects[index].x = x;
    g_curr_window_rects[index].y = y;
    g_curr_window_rects[index].w = w;
    g_curr_window_rects[index].h = h;
    g_curr_window_rects[index].valid = 1;

    if (index + 1 > g_curr_window_rect_count) {
        g_curr_window_rect_count = index + 1;
    }
}

static int u32_to_ascii(uint32_t value, char *out) {
    char tmp[10];
    int n = 0;

    if (value == 0) {
        out[0] = '0';
        out[1] = '\0';
        return 1;
    }

    while (value > 0 && n < 10) {
        tmp[n++] = (char)('0' + (value % 10));
        value /= 10;
    }

    for (int i = 0; i < n; i++) {
        out[i] = tmp[n - 1 - i];
    }
    out[n] = '\0';
    return n;
}

static int key_matches(char input_key, char bind_key) {
    if (input_key == bind_key) return 1;
    if (bind_key >= 'a' && bind_key <= 'z') {
        return input_key == (char)(bind_key - ('a' - 'A'));
    }
    if (bind_key >= 'A' && bind_key <= 'Z') {
        return input_key == (char)(bind_key + ('a' - 'A'));
    }
    return 0;
}

static int modifiers_match(uint8_t active_mods, int required_mask) {
    if (required_mask <= 0) return 1;
    return ((active_mods & (uint8_t)required_mask) == (uint8_t)required_mask);
}

static struct deimos_window_rect *find_rect_by_id(struct deimos_window_rect *rects, int count, int id) {
    for (int i = 0; i < count; i++) {
        if (rects[i].valid && rects[i].id == id) {
            return &rects[i];
        }
    }
    return 0;
}

static int rect_equals(const struct deimos_window_rect *a, const struct deimos_window_rect *b) {
    if (!a || !b) return 0;
    return (a->x == b->x) && (a->y == b->y) && (a->w == b->w) && (a->h == b->h);
}

static int point_in_rect(const struct deimos_window_rect *r, int x, int y) {
    if (!r || !r->valid) return 0;
    if (r->w <= 0 || r->h <= 0) return 0;
    if (x < r->x || y < r->y) return 0;
    if (x >= (r->x + r->w) || y >= (r->y + r->h)) return 0;
    return 1;
}

static void mark_rect_dirty(const struct deimos_window_rect *r) {
    if (!r || !r->valid) return;
    render_mark_dirty_rect(r->x, r->y, r->w, r->h);
}

static void mark_focus_visual_dirty(const struct deimos_window_rect *r) {
    if (!r || !r->valid) return;
    if (r->w <= 0 || r->h <= 0) return;

    // Border pixels.
    render_mark_dirty_rect(r->x, r->y, r->w, 1);
    render_mark_dirty_rect(r->x, r->y + r->h - 1, r->w, 1);
    render_mark_dirty_rect(r->x, r->y, 1, r->h);
    render_mark_dirty_rect(r->x + r->w - 1, r->y, 1, r->h);

    // Title strip inside the surface (focus tint change).
    int inner_w = r->w - 2;
    int inner_h = r->h - 2;
    if (inner_w > 0 && inner_h > 0) {
        int strip_h = (inner_h > 14) ? 12 : (inner_h / 2);
        if (strip_h > 0) {
            render_mark_dirty_rect(r->x + 1, r->y + 1, inner_w, strip_h);
        }
    }
}

static void mark_drag_preview_dirty(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (g_cfg.drag_preview_mode == 1) {
        render_mark_dirty_rect(x, y, w, 1);
        render_mark_dirty_rect(x, y + h - 1, w, 1);
        render_mark_dirty_rect(x, y, 1, h);
        render_mark_dirty_rect(x + w - 1, y, 1, h);
        return;
    }
    render_mark_dirty_rect(x, y, w, h);
}

static int find_hovered_window_id(int mouse_x, int mouse_y) {
    for (int i = g_prev_window_rect_count - 1; i >= 0; i--) {
        if (point_in_rect(&g_prev_window_rects[i], mouse_x, mouse_y)) {
            return g_prev_window_rects[i].id;
        }
    }
    return -1;
}

static void mark_focus_change_dirty(int old_focus_id, int new_focus_id) {
    if (old_focus_id == new_focus_id) return;

    struct deimos_window_rect *old_rect = find_rect_by_id(g_prev_window_rects, g_prev_window_rect_count, old_focus_id);
    struct deimos_window_rect *new_rect = find_rect_by_id(g_prev_window_rects, g_prev_window_rect_count, new_focus_id);
    mark_focus_visual_dirty(old_rect);
    mark_focus_visual_dirty(new_rect);
}

static void mark_window_layout_dirty_from_reports(void) {
    for (int i = 0; i < g_prev_window_rect_count; i++) {
        if (!g_prev_window_rects[i].valid) continue;
        struct deimos_window_rect *curr = find_rect_by_id(g_curr_window_rects, g_curr_window_rect_count, g_prev_window_rects[i].id);
        if (!curr) {
            mark_rect_dirty(&g_prev_window_rects[i]);
            continue;
        }
        if (!rect_equals(&g_prev_window_rects[i], curr)) {
            mark_rect_dirty(&g_prev_window_rects[i]);
            mark_rect_dirty(curr);
        }
    }

    for (int i = 0; i < g_curr_window_rect_count; i++) {
        if (!g_curr_window_rects[i].valid) continue;
        struct deimos_window_rect *prev = find_rect_by_id(g_prev_window_rects, g_prev_window_rect_count, g_curr_window_rects[i].id);
        if (!prev) {
            mark_rect_dirty(&g_curr_window_rects[i]);
        }
    }
}

static void copy_current_reports_to_previous(void) {
    g_prev_window_rect_count = g_curr_window_rect_count;
    for (int i = 0; i < DEIMOS_MAX_REPORT_WINDOWS; i++) {
        g_prev_window_rects[i] = g_curr_window_rects[i];
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    print("[deimos] starting\n");
    deimos_config_set_defaults(&g_cfg);
    if (deimos_config_load(&g_cfg, "/cfg/deimos.conf") == 0) {
        print("[deimos] loaded /cfg/deimos.conf\n");
    } else {
        print("[deimos] using defaults (missing /cfg/deimos.conf)\n");
    }

    int rc = render_init();
    if (rc != 0) {
        print("[deimos] render_init FAILED\n");
        exit(1);
        return 1;
    }
    print("[deimos] render_init ok\n");

    const uint32_t ticks_per_second = 100;
    uint64_t last_fps_tick = ticks();
    uint32_t frames_this_second = 0;
    uint32_t fps = 0;
    uint32_t presented_frames = 0;

    int mouse_x = render_width() / 2;
    int mouse_y = render_height() / 2;
    int prev_mouse_x = mouse_x;
    int prev_mouse_y = mouse_y;
    int cursor_valid = 0;

    int fps_box_x = 0;
    int fps_box_y = 0;
    int fps_box_w = 0;
    int fps_box_h = 0;
    int fps_box_valid = 0;

    int prev_window_count = -1;
    int drag_offset_x = 0;
    int drag_offset_y = 0;
    int drag_preview_x = 0;
    int drag_preview_y = 0;
    int drag_preview_w = 0;
    int drag_preview_h = 0;
    int drag_preview_valid = 0;

    deimos_wm_init(mouse_x, mouse_y);
    render_mark_full_dirty();

    print("[deimos] using configured keybinds (see /cfg/deimos.conf)\n");

    while (1) {
        int should_quit = 0;
        int layout_changed = 0;
        int fps_changed = 0;
        int drag_preview_update_needed = 0;

        struct user_input_event ev;
        while (input_poll(&ev) == 1) {
            if (ev.type == INPUT_EVENT_MOUSE_MOVE || ev.type == INPUT_EVENT_MOUSE_BUTTON) {
                mouse_x = ev.mouse_x;
                mouse_y = ev.mouse_y;
            }

            if (ev.type == INPUT_EVENT_KEYBOARD && ev.pressed) {
                if (key_matches((char)ev.key, g_cfg.key_quit)) {
                    should_quit = 1;
                } else if (key_matches((char)ev.key, g_cfg.key_new_window)) {
                    int mode = g_cfg.keyboard_split_use_focus
                        ? DEIMOS_SPLIT_TARGET_FOCUS
                        : DEIMOS_SPLIT_TARGET_MOUSE;
                    deimos_wm_add_window_split(mouse_x, mouse_y, mode);
                    layout_changed = 1;
                }
            }

            if (ev.type == INPUT_EVENT_MOUSE_BUTTON && ev.scancode == 1) {
                if (ev.pressed == 0) {
                    if (g_drag_active && g_drag_window_id > 0) {
                        if (drag_preview_valid) {
                            mark_drag_preview_dirty(drag_preview_x, drag_preview_y, drag_preview_w, drag_preview_h);
                        }
                        if (deimos_wm_set_split_for_window_id(g_drag_window_id, mouse_x, mouse_y)) {
                            layout_changed = 1;
                        }
                    }
                    g_drag_active = 0;
                    g_drag_window_id = -1;
                    drag_preview_valid = 0;
                } else {
                    int hovered_window_id = find_hovered_window_id(mouse_x, mouse_y);

                    if (hovered_window_id > 0) {
                        int old_focus_id = deimos_focus_window_id();
                        if (deimos_wm_set_focus_window_id(hovered_window_id)) {
                            int new_focus_id = deimos_focus_window_id();
                            mark_focus_change_dirty(old_focus_id, new_focus_id);
                        }
                    } else if (g_cfg.mouse_new_window) {
                        deimos_wm_add_window_split(mouse_x, mouse_y, DEIMOS_SPLIT_TARGET_MOUSE);
                        layout_changed = 1;
                    }

                    if (hovered_window_id > 0 && modifiers_match(ev.modifiers, g_cfg.drag_modifier_mask)) {
                        struct deimos_window_rect *hovered_rect =
                            find_rect_by_id(g_prev_window_rects, g_prev_window_rect_count, hovered_window_id);
                        if (hovered_rect && hovered_rect->valid) {
                            int ox = mouse_x - hovered_rect->x;
                            int oy = mouse_y - hovered_rect->y;
                            if (ox < 0) ox = 0;
                            if (oy < 0) oy = 0;
                            if (ox >= hovered_rect->w) ox = hovered_rect->w - 1;
                            if (oy >= hovered_rect->h) oy = hovered_rect->h - 1;

                            drag_offset_x = ox;
                            drag_offset_y = oy;
                            drag_preview_w = hovered_rect->w;
                            drag_preview_h = hovered_rect->h;
                            drag_preview_x = hovered_rect->x;
                            drag_preview_y = hovered_rect->y;
                            drag_preview_valid = 1;

                            g_drag_active = 1;
                            g_drag_window_id = hovered_window_id;

                            // Original layout window disappears while dragging; preview appears.
                            render_mark_dirty_rect(hovered_rect->x, hovered_rect->y, hovered_rect->w, hovered_rect->h);
                            mark_drag_preview_dirty(drag_preview_x, drag_preview_y, drag_preview_w, drag_preview_h);
                        } else {
                            g_drag_active = 0;
                            g_drag_window_id = -1;
                            drag_preview_valid = 0;
                        }
                    } else {
                        g_drag_active = 0;
                        g_drag_window_id = -1;
                        drag_preview_valid = 0;
                    }
                }
            }

            if ((ev.type == INPUT_EVENT_MOUSE_MOVE || ev.type == INPUT_EVENT_MOUSE_BUTTON) &&
                g_drag_active && g_drag_window_id > 0) {
                if ((ev.mouse_buttons & 1U) == 0) {
                    if (drag_preview_valid) {
                        mark_drag_preview_dirty(drag_preview_x, drag_preview_y, drag_preview_w, drag_preview_h);
                    }
                    if (deimos_wm_set_split_for_window_id(g_drag_window_id, mouse_x, mouse_y)) {
                        layout_changed = 1;
                    }
                    g_drag_active = 0;
                    g_drag_window_id = -1;
                    drag_preview_valid = 0;
                } else if (drag_preview_valid) {
                    drag_preview_update_needed = 1;
                }
            }
        }

        if (should_quit) {
            break;
        }

        int window_count = deimos_wm_window_count();
        if (window_count != prev_window_count) {
            prev_window_count = window_count;
            layout_changed = 1;
        }

        if (g_drag_active && drag_preview_valid && drag_preview_update_needed) {
            int next_x = mouse_x - drag_offset_x;
            int next_y = mouse_y - drag_offset_y;
            if (next_x < 0) next_x = 0;
            if (next_y < 0) next_y = 0;
            if (next_x + drag_preview_w > render_width()) {
                next_x = render_width() - drag_preview_w;
            }
            if (next_y + drag_preview_h > render_height()) {
                next_y = render_height() - drag_preview_h;
            }
            if (next_x < 0) next_x = 0;
            if (next_y < 0) next_y = 0;

            if (next_x != drag_preview_x || next_y != drag_preview_y) {
                mark_drag_preview_dirty(drag_preview_x, drag_preview_y, drag_preview_w, drag_preview_h);
                drag_preview_x = next_x;
                drag_preview_y = next_y;
                mark_drag_preview_dirty(drag_preview_x, drag_preview_y, drag_preview_w, drag_preview_h);
            }
        }

        if (!cursor_valid) {
            render_mark_dirty_rect(mouse_x - 1, mouse_y - 1, 3, 3);
            prev_mouse_x = mouse_x;
            prev_mouse_y = mouse_y;
            cursor_valid = 1;
        } else if (mouse_x != prev_mouse_x || mouse_y != prev_mouse_y) {
            render_mark_dirty_rect(prev_mouse_x - 1, prev_mouse_y - 1, 3, 3);
            render_mark_dirty_rect(mouse_x - 1, mouse_y - 1, 3, 3);
            prev_mouse_x = mouse_x;
            prev_mouse_y = mouse_y;
        }

        frames_this_second++;
        uint64_t now = ticks();
        if (now - last_fps_tick >= ticks_per_second) {
            fps = frames_this_second;
            frames_this_second = 0;
            last_fps_tick = now;
            fps_changed = 1;
        }

        char fps_text[20];
        fps_text[0] = 'F';
        fps_text[1] = 'P';
        fps_text[2] = 'S';
        fps_text[3] = ':';
        fps_text[4] = ' ';
        int len = 5 + u32_to_ascii(fps, &fps_text[5]);
        fps_text[len] = '\0';

        int text_w = render_text_width(fps_text);
        int text_x = render_width() - text_w - 8;
        if (text_x < 0) text_x = 0;
        int text_y = 8;
        int next_fps_box_x = text_x - 3;
        int next_fps_box_y = text_y - 2;
        int next_fps_box_w = text_w + 6;
        int next_fps_box_h = 11;

        if (!fps_box_valid) {
            render_mark_dirty_rect(next_fps_box_x, next_fps_box_y, next_fps_box_w, next_fps_box_h);
            fps_box_valid = 1;
        } else if (fps_changed ||
                   next_fps_box_x != fps_box_x ||
                   next_fps_box_y != fps_box_y ||
                   next_fps_box_w != fps_box_w ||
                   next_fps_box_h != fps_box_h) {
            render_mark_dirty_rect(fps_box_x, fps_box_y, fps_box_w, fps_box_h);
            render_mark_dirty_rect(next_fps_box_x, next_fps_box_y, next_fps_box_w, next_fps_box_h);
        }
        fps_box_x = next_fps_box_x;
        fps_box_y = next_fps_box_y;
        fps_box_w = next_fps_box_w;
        fps_box_h = next_fps_box_h;

        if (layout_changed) {
            g_should_draw_windows = 0;
            deimos_begin_window_report();
            mt_heap_reset();
            deimos_compositor_test_frame_with_count(window_count);
            mark_window_layout_dirty_from_reports();
            copy_current_reports_to_previous();
        }

        if (g_cfg.mouse_focus_follows_hover && !g_drag_active) {
            int hovered_window_id = find_hovered_window_id(mouse_x, mouse_y);
            if (hovered_window_id > 0) {
                int old_focus_id = deimos_focus_window_id();
                if (deimos_wm_set_focus_window_id(hovered_window_id)) {
                    int new_focus_id = deimos_focus_window_id();
                    mark_focus_change_dirty(old_focus_id, new_focus_id);
                }
            }
        }

        if ((presented_frames % 180U) == 0U) {
            render_mark_full_dirty();
        }

        if (render_has_dirty()) {
            g_should_draw_windows = 1;
            deimos_begin_window_report();
            mt_heap_reset();
            render_begin_frame(g_cfg.background_color);
            deimos_compositor_test_frame_with_count(window_count);

            if (g_drag_active && g_drag_window_id > 0 && drag_preview_valid) {
                if (g_cfg.drag_preview_mode == 1) {
                    render_draw_rect(drag_preview_x, drag_preview_y, drag_preview_w, drag_preview_h, g_cfg.window_focus_color);
                } else {
                    deimos_draw_window_surface_full(
                        g_drag_window_id,
                        drag_preview_x,
                        drag_preview_y,
                        drag_preview_w,
                        drag_preview_h,
                        1
                    );
                    render_draw_rect(drag_preview_x, drag_preview_y, drag_preview_w, drag_preview_h, g_cfg.window_focus_color);
                }
            }

            render_fill_rect(mouse_x - 1, mouse_y - 1, 3, 3, g_cfg.cursor_color);
            render_fill_rect(text_x - 3, text_y - 2, text_w + 6, 11, g_cfg.fps_bg_color);
            render_draw_text(text_x, text_y, fps_text, g_cfg.fps_fg_color);

            render_present_dirty();
            render_reset_dirty();
            copy_current_reports_to_previous();
            presented_frames++;
        }

        yield();
    }

    exit(0);
    return 0;
}
