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

// Managed app window tracking
struct managed_window {
    int kernel_handle;      // kernel window slot (0-15), -1 = unused
    int deimos_window_id;   // deimos WM window ID (1-based)
    uint32_t *buffer;       // compositor-mapped read-only pointer
    int width;
    int height;
    int active;
};

static struct managed_window g_managed[DEIMOS_MAX_REPORT_WINDOWS];
static int g_managed_count = 0;

struct pending_launch {
    int active;
    int pid;
    int x;
    int y;
    int target_mode;
    int floating;
    int width;
    int height;
};

static struct pending_launch g_pending_launch[DEIMOS_MAX_REPORT_WINDOWS];

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

static int find_managed_by_deimos_id(int deimos_id) {
    for (int i = 0; i < g_managed_count; i++) {
        if (g_managed[i].active && g_managed[i].deimos_window_id == deimos_id)
            return i;
    }
    return -1;
}

static void clear_pending_launches(void) {
    for (int i = 0; i < DEIMOS_MAX_REPORT_WINDOWS; i++) {
        g_pending_launch[i].active = 0;
        g_pending_launch[i].pid = 0;
    }
}

static void remember_pending_launch(int pid, int x, int y,
                                    int target_mode, int floating,
                                    int width, int height) {
    if (pid <= 0) return;

    int slot = -1;
    for (int i = 0; i < DEIMOS_MAX_REPORT_WINDOWS; i++) {
        if (!g_pending_launch[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        slot = 0;
    }

    g_pending_launch[slot].active = 1;
    g_pending_launch[slot].pid = pid;
    g_pending_launch[slot].x = x;
    g_pending_launch[slot].y = y;
    g_pending_launch[slot].target_mode = target_mode;
    g_pending_launch[slot].floating = floating ? 1 : 0;
    g_pending_launch[slot].width = width;
    g_pending_launch[slot].height = height;
}

static int take_pending_launch_for_pid(int pid, struct pending_launch *out) {
    if (pid <= 0 || !out) return 0;
    for (int i = 0; i < DEIMOS_MAX_REPORT_WINDOWS; i++) {
        if (!g_pending_launch[i].active) continue;
        if (g_pending_launch[i].pid != pid) continue;
        *out = g_pending_launch[i];
        g_pending_launch[i].active = 0;
        g_pending_launch[i].pid = 0;
        return 1;
    }
    return 0;
}

static uint32_t *get_managed_buffer_for_window_id(int window_id) {
    int mi = find_managed_by_deimos_id(window_id);
    if (mi < 0) return 0;
    return g_managed[mi].buffer;
}

static int get_managed_width_for_window_id(int window_id) {
    int mi = find_managed_by_deimos_id(window_id);
    if (mi < 0) return 0;
    return g_managed[mi].width;
}

static int get_managed_height_for_window_id(int window_id) {
    int mi = find_managed_by_deimos_id(window_id);
    if (mi < 0) return 0;
    return g_managed[mi].height;
}

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

static void blit_app_buffer_scaled(uint32_t *buf, int buf_w, int buf_h,
                                   int dst_x, int dst_y, int dst_w, int dst_h) {
    if (!buf || buf_w <= 0 || buf_h <= 0 || dst_w <= 0 || dst_h <= 0) return;

    int scr_w = render_width();
    int scr_h = render_height();
    int pitch = render_pitch();
    uint8_t *bb = render_backbuffer();
    if (!bb) return;

    /* 1:1 scale fast path: row-level memcpy when dimensions match */
    if (buf_w == dst_w && buf_h == dst_h) {
        /* Clip y range */
        int y0 = (dst_y < 0) ? 0 : dst_y;
        int y1 = (dst_y + dst_h > scr_h) ? scr_h : (dst_y + dst_h);
        /* Clip x range */
        int x0 = (dst_x < 0) ? 0 : dst_x;
        int x1 = (dst_x + dst_w > scr_w) ? scr_w : (dst_x + dst_w);
        if (x0 >= x1 || y0 >= y1) return;

        int row_pixels = x1 - x0;
        int src_x_off = x0 - dst_x;
        for (int py = y0; py < y1; py++) {
            int sy = py - dst_y;
            uint32_t *src_row = buf + sy * buf_w + src_x_off;
            uint32_t *dst_row = (uint32_t *)(bb + py * pitch) + x0;
            for (int i = 0; i < row_pixels; i++)
                dst_row[i] = src_row[i];
        }
        return;
    }

    for (int dy = 0; dy < dst_h; dy++) {
        int py = dst_y + dy;
        if ((unsigned)py >= (unsigned)scr_h) continue;
        int sy = (dy * buf_h) / dst_h;
        if (sy >= buf_h) sy = buf_h - 1;
        uint32_t *src_row = buf + sy * buf_w;

        /* Clip x range */
        int x0 = dst_x;
        int x1 = dst_x + dst_w;
        if (x0 < 0) x0 = 0;
        if (x1 > scr_w) x1 = scr_w;
        if (x0 >= x1) continue;

        uint32_t *dst_row = (uint32_t *)(bb + py * pitch) + x0;

        /* Bresenham-style stepping for sx */
        int sx = ((x0 - dst_x) * buf_w) / dst_w;
        int sx_err = ((x0 - dst_x) * buf_w) % dst_w;
        for (int px = x0; px < x1; px++) {
            if (sx >= buf_w) sx = buf_w - 1;
            *dst_row++ = src_row[sx];
            sx_err += buf_w;
            if (sx_err >= dst_w) {
                sx += sx_err / dst_w;
                sx_err %= dst_w;
            }
        }
    }
}

static void deimos_draw_window_surface_full(int window_id, int x, int y, int w, int h, int focused) {
    if (window_id <= 0 || window_id > DEIMOS_MAX_REPORT_WINDOWS) return;
    if (w <= 2 || h <= 2) return;

    int inner_x = x + 1;
    int inner_y = y + 1;
    int inner_w = w - 2;
    int inner_h = h - 2;
    if (inner_w <= 0 || inner_h <= 0) return;

    // Check for managed app buffer
    uint32_t *app_buf = get_managed_buffer_for_window_id(window_id);
    if (app_buf) {
        int buf_w = get_managed_width_for_window_id(window_id);
        int buf_h = get_managed_height_for_window_id(window_id);
        blit_app_buffer_scaled(app_buf, buf_w, buf_h, inner_x, inner_y, inner_w, inner_h);
        return;
    }

    // Fallback: placeholder pattern
    init_window_surface(window_id);
    struct deimos_window_surface *s = &g_surfaces[window_id];
    if (!s->initialized) return;

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
                                    int clip_x, int clip_y, int clip_w, int clip_h,
                                    uint32_t *app_buf, int app_w, int app_h) {
    uint32_t border_col = focused ? g_cfg.window_focus_color : g_cfg.window_border_color;
    uint32_t strip_col = focused ? colour_rgb(245, 245, 250) : colour_rgb(28, 32, 40);

    int inner_x = x + 1;
    int inner_y = y + 1;
    int inner_w = w - 2;
    int inner_h = h - 2;
    int strip_h = app_buf ? 0 : ((inner_h > 14) ? 12 : (inner_h / 2));

    int src_w = app_buf ? app_w : DEIMOS_SURFACE_W;
    int src_h = app_buf ? app_h : DEIMOS_SURFACE_H;

    /* Draw borders as fill rects clipped to clip region */
    /* Top border */
    if (clip_y <= y && y < clip_y + clip_h)
        render_fill_rect(clip_x, y, clip_w, 1, border_col);
    /* Bottom border */
    if (clip_y <= y + h - 1 && y + h - 1 < clip_y + clip_h)
        render_fill_rect(clip_x, y + h - 1, clip_w, 1, border_col);
    /* Left border */
    if (clip_x <= x && x < clip_x + clip_w)
        render_fill_rect(x, clip_y, 1, clip_h, border_col);
    /* Right border */
    if (clip_x <= x + w - 1 && x + w - 1 < clip_x + clip_w)
        render_fill_rect(x + w - 1, clip_y, 1, clip_h, border_col);

    if (inner_w <= 0 || inner_h <= 0) return;

    /* Compute content clip region (intersection of clip rect and inner area) */
    int cx0 = clip_x > inner_x ? clip_x : inner_x;
    int cy0 = clip_y > inner_y ? clip_y : inner_y;
    int cx1 = (clip_x + clip_w) < (inner_x + inner_w) ? (clip_x + clip_w) : (inner_x + inner_w);
    int cy1 = (clip_y + clip_h) < (inner_y + inner_h) ? (clip_y + clip_h) : (inner_y + inner_h);
    if (cx0 >= cx1 || cy0 >= cy1) return;

    int scr_w = render_width();
    int pitch = render_pitch();
    uint8_t *bb = render_backbuffer();
    int use_direct = (bb && render_bpp() == 32);

    /* 1:1 app buffer fast path: direct row copy when dimensions match */
    int app_1to1 = (app_buf && src_w == inner_w && src_h == inner_h);

    for (int yy = cy0; yy < cy1; yy++) {
        int in_strip = (!app_buf && strip_h > 0 && yy < (inner_y + strip_h));

        if (use_direct) {
            uint32_t *dst_row = (uint32_t *)(bb + yy * pitch) + cx0;

            if (in_strip) {
                /* Fill with strip color directly */
                for (int xx = cx0; xx < cx1; xx++)
                    *dst_row++ = strip_col;
            } else if (app_1to1) {
                int sy = yy - inner_y;
                uint32_t *src_row = app_buf + sy * app_w + (cx0 - inner_x);
                int count = cx1 - cx0;
                for (int i = 0; i < count; i++)
                    dst_row[i] = src_row[i];
            } else if (app_buf) {
                int sy = ((yy - inner_y) * src_h) / inner_h;
                if (sy >= src_h) sy = src_h - 1;
                uint32_t *src_row = app_buf + sy * app_w;
                /* Bresenham sx stepping */
                int dx0 = cx0 - inner_x;
                int sx = (dx0 * src_w) / inner_w;
                int sx_err = (dx0 * src_w) % inner_w;
                for (int xx = cx0; xx < cx1; xx++) {
                    if (sx >= src_w) sx = src_w - 1;
                    *dst_row++ = src_row[sx];
                    sx_err += src_w;
                    if (sx_err >= inner_w) {
                        sx += sx_err / inner_w;
                        sx_err %= inner_w;
                    }
                }
            } else {
                int sy = ((yy - inner_y) * src_h) / inner_h;
                if (sy >= src_h) sy = src_h - 1;
                uint32_t *src_row = s->pixels + sy * DEIMOS_SURFACE_W;
                int dx0 = cx0 - inner_x;
                int sx = (dx0 * src_w) / inner_w;
                int sx_err = (dx0 * src_w) % inner_w;
                for (int xx = cx0; xx < cx1; xx++) {
                    if (sx >= src_w) sx = src_w - 1;
                    *dst_row++ = src_row[sx];
                    sx_err += src_w;
                    if (sx_err >= inner_w) {
                        sx += sx_err / inner_w;
                        sx_err %= inner_w;
                    }
                }
            }
        } else {
            /* Fallback: per-pixel putpixel */
            int sy = ((yy - inner_y) * src_h) / inner_h;
            if (sy >= src_h) sy = src_h - 1;
            for (int xx = cx0; xx < cx1; xx++) {
                int sx = ((xx - inner_x) * src_w) / inner_w;
                if (sx >= src_w) sx = src_w - 1;

                uint32_t c;
                if (in_strip) {
                    c = strip_col;
                } else if (app_buf) {
                    c = app_buf[sy * app_w + sx];
                } else {
                    c = s->pixels[sy * DEIMOS_SURFACE_W + sx];
                }
                render_putpixel(xx, yy, c);
            }
        }
    }
}

void deimos_draw_window_frame(int window_id, int x, int y, int w, int h, int focused) {
    if (window_id <= 0 || window_id > DEIMOS_MAX_REPORT_WINDOWS) return;
    if (w <= 1 || h <= 1) return;

    uint32_t *app_buf = get_managed_buffer_for_window_id(window_id);
    int app_w = get_managed_width_for_window_id(window_id);
    int app_h = get_managed_height_for_window_id(window_id);

    if (!app_buf) {
        init_window_surface(window_id);
    }
    struct deimos_window_surface *s = &g_surfaces[window_id];

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
        deimos_draw_window_clip(s, x, y, w, h, focused, ix, iy, iw, ih,
                                app_buf, app_w, app_h);
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

static char key_from_set1_scancode(uint8_t scancode) {
    switch (scancode & 0x7F) {
        case 0x01: return 27;   // esc
        case 0x02: return '1';
        case 0x03: return '2';
        case 0x04: return '3';
        case 0x05: return '4';
        case 0x06: return '5';
        case 0x07: return '6';
        case 0x08: return '7';
        case 0x09: return '8';
        case 0x0A: return '9';
        case 0x0B: return '0';
        case 0x0C: return '-';
        case 0x0D: return '=';
        case 0x0F: return '\t';
        case 0x10: return 'q';
        case 0x11: return 'w';
        case 0x12: return 'e';
        case 0x13: return 'r';
        case 0x14: return 't';
        case 0x15: return 'y';
        case 0x16: return 'u';
        case 0x17: return 'i';
        case 0x18: return 'o';
        case 0x19: return 'p';
        case 0x1A: return '[';
        case 0x1B: return ']';
        case 0x1C: return '\n';
        case 0x1E: return 'a';
        case 0x1F: return 's';
        case 0x20: return 'd';
        case 0x21: return 'f';
        case 0x22: return 'g';
        case 0x23: return 'h';
        case 0x24: return 'j';
        case 0x25: return 'k';
        case 0x26: return 'l';
        case 0x27: return ';';
        case 0x28: return '\'';
        case 0x29: return '`';
        case 0x2B: return '#';
        case 0x2C: return 'z';
        case 0x2D: return 'x';
        case 0x2E: return 'c';
        case 0x2F: return 'v';
        case 0x30: return 'b';
        case 0x31: return 'n';
        case 0x32: return 'm';
        case 0x33: return ',';
        case 0x34: return '.';
        case 0x35: return '/';
        case 0x39: return ' ';
        default: return 0;
    }
}

static int bind_key_matches_event(uint8_t bind_key, const struct user_input_event *ev) {
    if (!ev) return 0;

    if (key_matches((char)ev->key, (char)bind_key)) {
        return 1;
    }

    {
        char fallback_key = key_from_set1_scancode(ev->scancode);
        if (fallback_key && key_matches(fallback_key, (char)bind_key)) {
            return 1;
        }
    }

    return 0;
}

static int modifiers_match(uint8_t active_mods, int required_mask) {
    if (required_mask <= 0) return 1;
    return ((active_mods & (uint8_t)required_mask) == (uint8_t)required_mask);
}

struct deimos_launch_options {
    char path[DEIMOS_BIND_ARG_MAX];
    int floating;
    int external;
    int width;
    int height;
};

static int is_space_local(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static char ascii_lower_local(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c + ('a' - 'A'));
    return c;
}

static int str_eq_local_ci(const char *a, const char *b) {
    int i = 0;
    if (!a || !b) return 0;
    while (a[i] && b[i]) {
        if (ascii_lower_local(a[i]) != ascii_lower_local(b[i])) return 0;
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static int parse_positive_int_local(const char *text) {
    int value = 0;
    int i = 0;
    if (!text || !text[0]) return -1;
    while (text[i]) {
        if (text[i] < '0' || text[i] > '9') return -1;
        value = (value * 10) + (text[i] - '0');
        i++;
    }
    return value;
}

static void parse_launch_option_token(struct deimos_launch_options *opts, char *token) {
    if (!opts || !token || !token[0]) return;

    for (int i = 0; token[i]; i++) {
        token[i] = ascii_lower_local(token[i]);
    }

    if (str_eq_local_ci(token, "floating") || str_eq_local_ci(token, "float")) {
        opts->floating = 1;
        return;
    }
    if (str_eq_local_ci(token, "tiled") || str_eq_local_ci(token, "tile")) {
        opts->floating = 0;
        return;
    }
    if (str_eq_local_ci(token, "external") || str_eq_local_ci(token, "spawn")) {
        opts->external = 1;
        return;
    }
    if (str_eq_local_ci(token, "managed") || str_eq_local_ci(token, "windowed")) {
        opts->external = 0;
        return;
    }

    if (token[0] == 'w' && token[1] == '=') {
        int v = parse_positive_int_local(&token[2]);
        if (v > 0) opts->width = v;
        return;
    }
    if (token[0] == 'h' && token[1] == '=') {
        int v = parse_positive_int_local(&token[2]);
        if (v > 0) opts->height = v;
        return;
    }

    if (token[0] == 's' && token[1] == 'i' && token[2] == 'z' && token[3] == 'e' && token[4] == '=') {
        int split = 5;
        while (token[split]) {
            if (token[split] == 'x' || token[split] == 'X') break;
            split++;
        }
        if (token[split] == 'x' || token[split] == 'X') {
            token[split] = '\0';
            int w = parse_positive_int_local(&token[5]);
            int h = parse_positive_int_local(&token[split + 1]);
            if (w > 0) opts->width = w;
            if (h > 0) opts->height = h;
            token[split] = 'x';
        }
    }
}

static int parse_launch_options(const char *arg, struct deimos_launch_options *out) {
    char buf[DEIMOS_BIND_ARG_MAX];
    int bi = 0;
    int i = 0;

    if (!arg || !out) return 0;

    out->path[0] = '\0';
    out->floating = 0;
    out->external = 0;
    out->width = 420;
    out->height = 260;

    while (arg[i] && bi < DEIMOS_BIND_ARG_MAX - 1) {
        buf[bi++] = arg[i++];
    }
    buf[bi] = '\0';

    i = 0;
    while (buf[i] && is_space_local(buf[i])) i++;
    if (!buf[i]) return 0;

    int path_start = i;
    while (buf[i] && !is_space_local(buf[i]) && buf[i] != ',') i++;
    int path_end = i;
    if (path_end <= path_start) return 0;

    int oi = 0;
    for (int p = path_start; p < path_end && oi < DEIMOS_BIND_ARG_MAX - 1; p++) {
        out->path[oi++] = buf[p];
    }
    out->path[oi] = '\0';

    while (buf[i]) {
        while (buf[i] == ',' || is_space_local(buf[i])) i++;
        if (!buf[i]) break;

        int token_start = i;
        while (buf[i] && buf[i] != ',' && !is_space_local(buf[i])) i++;
        char saved = buf[i];
        buf[i] = '\0';
        parse_launch_option_token(out, &buf[token_start]);
        buf[i] = saved;
    }

    return out->path[0] != '\0';
}

static int launch_app_detached(const char *path) {
    if (!path || !path[0]) return -1;

    int pid = fork();
    if (pid < 0) {
        print("[deimos] launch failed: fork\n");
        return -1;
    }

    if (pid == 0) {
        char *argv_local[2];
        argv_local[0] = (char *)path;
        argv_local[1] = 0;
        if (exec(path, argv_local) < 0) {
            print("[deimos] launch failed: exec\n");
        }
        exit(127);
    }

    return pid;
}

static void deactivate_managed_slot(int mi) {
    if (mi < 0 || mi >= DEIMOS_MAX_REPORT_WINDOWS) return;

    g_managed[mi].active = 0;
    g_managed[mi].kernel_handle = -1;
    g_managed[mi].deimos_window_id = 0;
    g_managed[mi].buffer = 0;
    g_managed[mi].width = 0;
    g_managed[mi].height = 0;

    while (g_managed_count > 0 && !g_managed[g_managed_count - 1].active) {
        g_managed_count--;
    }
}

static int close_window_by_deimos_id(int target_id, int terminate_owner) {
    if (target_id <= 0) return 0;

    int mi = find_managed_by_deimos_id(target_id);

    if (terminate_owner) {
        int owner_pid = deimos_wm_get_app_pid(target_id - 1);
        if (owner_pid > 0) {
            if (kill(owner_pid, SIGTERM) < 0) {
                print("[deimos] close failed: kill(SIGTERM)\n");
            }
        }
    }

    (void)deimos_wm_set_focus_window_id(target_id);
    if (!deimos_wm_close_focused_window()) return 0;

    if (mi >= 0) {
        deactivate_managed_slot(mi);
    }

    for (int j = 0; j < g_managed_count; j++) {
        if (g_managed[j].active && g_managed[j].deimos_window_id > target_id) {
            g_managed[j].deimos_window_id--;
        }
    }

    return 1;
}

static int handle_sentence_bind(const struct deimos_bind *bind,
                                int mouse_x, int mouse_y,
                                int *should_quit, int *layout_changed) {
    if (!bind || !should_quit || !layout_changed) return 0;

    if (bind->action == DEIMOS_BIND_ACTION_LAUNCH) {
        struct deimos_launch_options launch;
        if (!parse_launch_options(bind->arg, &launch)) {
            print("[deimos] launch failed: bad bind args\n");
            return 1;
        }

        if (launch.external) {
            (void)launch_app_detached(launch.path);
        } else {
            // Managed window: launch the app, it will call win_create().
            // discover_managed_windows() will pick it up and add to WM layout.
            int pid = launch_app_detached(launch.path);
            if (pid < 0) {
                print("[deimos] launch failed: fork/exec\n");
            } else {
                int mode = g_cfg.keyboard_split_use_focus
                    ? DEIMOS_SPLIT_TARGET_FOCUS
                    : DEIMOS_SPLIT_TARGET_MOUSE;
                remember_pending_launch(pid, mouse_x, mouse_y,
                                        mode, launch.floating,
                                        launch.width, launch.height);
            }
            // Layout change will happen when discover_managed_windows
            // detects the new kernel window created by the app.
        }
        return 1;
    }

    if (bind->action == DEIMOS_BIND_ACTION_NEW_WINDOW) {
        int mode = g_cfg.keyboard_split_use_focus
            ? DEIMOS_SPLIT_TARGET_FOCUS
            : DEIMOS_SPLIT_TARGET_MOUSE;
        deimos_wm_add_window_split(mouse_x, mouse_y, mode);
        *layout_changed = 1;
        return 1;
    }

    if (bind->action == DEIMOS_BIND_ACTION_CLOSE_FOCUSED) {
        int focused = deimos_focus_window_id();
        if (close_window_by_deimos_id(focused, 1)) {
            *layout_changed = 1;
        }
        return 1;
    }

    if (bind->action == DEIMOS_BIND_ACTION_QUIT_DEIMOS) {
        *should_quit = 1;
        return 1;
    }

    return 0;
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

static int find_managed_by_kernel_handle(int khandle) {
    for (int i = 0; i < g_managed_count; i++) {
        if (g_managed[i].active && g_managed[i].kernel_handle == khandle)
            return i;
    }
    return -1;
}

static void discover_managed_windows(int mouse_x, int mouse_y, int *layout_changed) {
    struct user_win_info info;

    // Check for new kernel windows
    for (int slot = 0; slot < 16; slot++) {
        if (win_info(slot, &info) && info.active) {
            if (find_managed_by_kernel_handle(slot) < 0) {
                // New window found -- add to WM, applying pending launch options when present.
                int mode = DEIMOS_SPLIT_TARGET_FOCUS;
                int floating = 0;
                int pos_x = mouse_x;
                int pos_y = mouse_y;
                int target_w = info.width;
                int target_h = info.height;

                struct pending_launch pending;
                if (take_pending_launch_for_pid(info.owner_pid, &pending)) {
                    mode = pending.target_mode;
                    floating = pending.floating;
                    pos_x = pending.x;
                    pos_y = pending.y;
                    if (pending.width > 0) target_w = pending.width;
                    if (pending.height > 0) target_h = pending.height;
                }

                int wm_id = deimos_wm_add_window_launch(pos_x, pos_y, mode, floating,
                                                         target_w, target_h);
                if (wm_id > 0) {
                    // Map buffer for compositing
                    long vaddr = win_map(slot);
                    int mi = -1;
                    for (int j = 0; j < DEIMOS_MAX_REPORT_WINDOWS; j++) {
                        if (!g_managed[j].active) {
                            mi = j;
                            break;
                        }
                    }
                    if (mi >= 0) {
                        g_managed[mi].kernel_handle = slot;
                        g_managed[mi].deimos_window_id = wm_id;
                        g_managed[mi].buffer = (vaddr > 0) ? (uint32_t *)(unsigned long)vaddr : 0;
                        g_managed[mi].width = info.width;
                        g_managed[mi].height = info.height;
                        g_managed[mi].active = 1;
                        if (mi >= g_managed_count) g_managed_count = mi + 1;

                        int idx = wm_id - 1;
                        deimos_wm_set_kernel_handle(idx, slot);
                        deimos_wm_set_app_pid(idx, info.owner_pid);
                    }
                    *layout_changed = 1;
                }
            }
        } else {
            // Slot not active -- check if we had a managed window for it
            int mi = find_managed_by_kernel_handle(slot);
            if (mi >= 0 && g_managed[mi].active) {
                // Window died -- remove from WM
                int target_id = g_managed[mi].deimos_window_id;
                if (target_id > 0) {
                    if (close_window_by_deimos_id(target_id, 0)) {
                        *layout_changed = 1;
                    }
                }
                deactivate_managed_slot(mi);
            }
        }
    }
}

static int get_focused_kernel_handle(void) {
    int focus_id = deimos_focus_window_id();
    if (focus_id <= 0) return -1;
    int idx = focus_id - 1;
    return deimos_wm_get_kernel_handle(idx);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    print("[deimos] starting\n");
    deimos_config_set_defaults(&g_cfg);
    if (deimos_config_load(&g_cfg, "/cfg/deimos.cfg") == 0) {
        print("[deimos] loaded /cfg/deimos.cfg\n");
    } else {
        print("[deimos] using defaults (missing /cfg/deimos.cfg)\n");
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

    for (int i = 0; i < DEIMOS_MAX_REPORT_WINDOWS; i++) {
        g_managed[i].active = 0;
        g_managed[i].kernel_handle = -1;
        g_managed[i].buffer = 0;
    }
    g_managed_count = 0;
    clear_pending_launches();

    deimos_wm_init(mouse_x, mouse_y);
    render_mark_full_dirty();

    print("[deimos] using configured keybinds (see /cfg/deimos.cfg)\n");
    if (g_cfg.bind_count > 0) {
        char count_text[16];
        u32_to_ascii((uint32_t)g_cfg.bind_count, count_text);
        print("[deimos] sentence binds loaded: ");
        print(count_text);
        print("\n");
    }

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
                int bind_handled = 0;
                for (int bi = 0; bi < g_cfg.bind_count; bi++) {
                    struct deimos_bind *b = &g_cfg.binds[bi];
                    if (!modifiers_match(ev.modifiers, b->modifiers)) continue;
                    if (!bind_key_matches_event((uint8_t)b->key, &ev)) continue;
                    if (handle_sentence_bind(b, mouse_x, mouse_y, &should_quit, &layout_changed)) {
                        bind_handled = 1;
                        break;
                    }
                }

                if (!bind_handled) {
                    if (key_matches((char)ev.key, g_cfg.key_quit)) {
                        should_quit = 1;
                    } else if (key_matches((char)ev.key, g_cfg.key_new_window)) {
                        int mode = g_cfg.keyboard_split_use_focus
                            ? DEIMOS_SPLIT_TARGET_FOCUS
                            : DEIMOS_SPLIT_TARGET_MOUSE;
                        deimos_wm_add_window_split(mouse_x, mouse_y, mode);
                        layout_changed = 1;
                    } else {
                        // Route keyboard to focused managed window
                        int kh = get_focused_kernel_handle();
                        if (kh >= 0) {
                            struct user_input_event route_ev;
                            route_ev.type = ev.type;
                            route_ev.key = ev.key;
                            route_ev.modifiers = ev.modifiers;
                            route_ev.pressed = ev.pressed;
                            route_ev.scancode = ev.scancode;
                            route_ev.mouse_buttons = ev.mouse_buttons;
                            route_ev.mouse_x = ev.mouse_x;
                            route_ev.mouse_y = ev.mouse_y;
                            win_send(kh, &route_ev);
                        }
                    }
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

        // Discover kernel-managed windows (apps that called win_create)
        discover_managed_windows(mouse_x, mouse_y, &layout_changed);

        // Check for dirty managed windows (app called win_present)
        for (int mi = 0; mi < g_managed_count; mi++) {
            if (!g_managed[mi].active) continue;
            struct user_win_info winfo;
            if (win_info(g_managed[mi].kernel_handle, &winfo) && winfo.dirty) {
                struct deimos_window_rect *wr = find_rect_by_id(
                    g_prev_window_rects, g_prev_window_rect_count,
                    g_managed[mi].deimos_window_id);
                if (wr) {
                    mark_rect_dirty(wr);
                }
            }
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

        if ((presented_frames % 6000U) == 0U) {
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
