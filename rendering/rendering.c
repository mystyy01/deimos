#include "rendering.h"
#include "../../apps/libsys.h"

static struct user_fb_info g_fb;
static uint32_t *backbuffer;

int render_init(void) {
    int rc = fb_info(&g_fb);
    if (rc != 0) return -1;
    if (g_fb.width == 0 || g_fb.height == 0) return -1;

    long addr = fb_map();
    if (!addr) return -1;
    backbuffer = (uint32_t *)addr;

    return 0;
}

int render_width(void)  { return (int)g_fb.width; }
int render_height(void) { return (int)g_fb.height; }
int render_bpp(void)    { return (int)g_fb.bpp; }
int render_pitch(void)  { return (int)g_fb.pitch; }

void render_begin_frame(uint32_t clear_colour) {
    int total = g_fb.width * g_fb.height;
    for (int i = 0; i < total; i++) {
        backbuffer[i] = clear_colour;
    }
}

void render_end_frame(void) {
    fb_present(backbuffer);
}

void render_putpixel(int x, int y, uint32_t colour) {
    if ((unsigned)x >= g_fb.width || (unsigned)y >= g_fb.height) return;
    backbuffer[y * g_fb.width + x] = colour;
}

void render_clear(uint32_t colour) {
    render_begin_frame(colour);
}

void render_fill_rect(int x, int y, int w, int h, uint32_t colour) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > (int)g_fb.width  ? (int)g_fb.width  : x + w;
    int y1 = y + h > (int)g_fb.height ? (int)g_fb.height : y + h;

    for (int yy = y0; yy < y1; yy++) {
        uint32_t *row = backbuffer + yy * g_fb.width + x0;
        int count = x1 - x0;
        for (int i = 0; i < count; i++) {
            row[i] = colour;
        }
    }
}

void render_draw_rect(int x, int y, int w, int h, uint32_t colour) {
    // Top and bottom edges
    for (int xx = 0; xx < w; xx++) {
        render_putpixel(x + xx, y, colour);
        render_putpixel(x + xx, y + h - 1, colour);
    }
    // Left and right edges
    for (int yy = 0; yy < h; yy++) {
        render_putpixel(x, y + yy, colour);
        render_putpixel(x + w - 1, y + yy, colour);
    }
}

void render_mark_dirty(int x, int w, int h) {
    (void)x; (void)w; (void)h;
}
void render_reset_dirty(void) {}
void render_present_full(void) { render_end_frame(); }
void render_present_dirty(void) { render_end_frame(); }
