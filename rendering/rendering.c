#include "rendering.h"
#include "../../apps/libsys.h"

static struct user_fb_info g_fb;
static uint8_t *backbuffer;
static uint32_t g_bytes_per_pixel;
static uint32_t g_pitch;

static const uint8_t GLYPH_SPACE[7] = {0, 0, 0, 0, 0, 0, 0};
static const uint8_t GLYPH_COLON[7] = {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};
static const uint8_t GLYPH_0[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
static const uint8_t GLYPH_1[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
static const uint8_t GLYPH_2[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
static const uint8_t GLYPH_3[7] = {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
static const uint8_t GLYPH_4[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
static const uint8_t GLYPH_5[7] = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E};
static const uint8_t GLYPH_6[7] = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
static const uint8_t GLYPH_7[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
static const uint8_t GLYPH_8[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
static const uint8_t GLYPH_9[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x1C};
static const uint8_t GLYPH_F[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
static const uint8_t GLYPH_P[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
static const uint8_t GLYPH_S[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};

static const uint8_t *render_glyph_for_char(char c) {
    switch (c) {
        case ' ': return GLYPH_SPACE;
        case ':': return GLYPH_COLON;
        case '0': return GLYPH_0;
        case '1': return GLYPH_1;
        case '2': return GLYPH_2;
        case '3': return GLYPH_3;
        case '4': return GLYPH_4;
        case '5': return GLYPH_5;
        case '6': return GLYPH_6;
        case '7': return GLYPH_7;
        case '8': return GLYPH_8;
        case '9': return GLYPH_9;
        case 'F': return GLYPH_F;
        case 'P': return GLYPH_P;
        case 'S': return GLYPH_S;
        default:  return GLYPH_SPACE;
    }
}

static void render_store_pixel(int x, int y, uint32_t colour) {
    uint8_t *p = backbuffer + ((uint32_t)y * g_pitch) + ((uint32_t)x * g_bytes_per_pixel);

    if (g_fb.bpp == 16) {
        uint8_t r = (uint8_t)((colour >> 16) & 0xFF);
        uint8_t g = (uint8_t)((colour >> 8) & 0xFF);
        uint8_t b = (uint8_t)(colour & 0xFF);
        uint16_t rgb565 = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        *(uint16_t *)p = rgb565;
        return;
    }

    if (g_fb.bpp == 24) {
        p[0] = (uint8_t)(colour & 0xFF);         // B
        p[1] = (uint8_t)((colour >> 8) & 0xFF);  // G
        p[2] = (uint8_t)((colour >> 16) & 0xFF); // R
        return;
    }

    // 32bpp path
    *(uint32_t *)p = colour;
}

int render_init(void) {
    print("[deimos] render_init: fb_info\n");
    int rc = fb_info(&g_fb);
    if (rc != 0) {
        print("[deimos] render_init: fb_info failed\n");
        return -1;
    }
    if (g_fb.width == 0 || g_fb.height == 0) {
        print("[deimos] render_init: bad dimensions\n");
        return -1;
    }
    if (g_fb.bpp != 16 && g_fb.bpp != 24 && g_fb.bpp != 32) {
        print("[deimos] render_init: unsupported bpp\n");
        return -1;
    }

    g_bytes_per_pixel = g_fb.bpp / 8;
    g_pitch = g_fb.pitch ? g_fb.pitch : (g_fb.width * g_bytes_per_pixel);
    if (g_bytes_per_pixel == 0 || g_pitch == 0) {
        print("[deimos] render_init: bad pitch/bpp\n");
        return -1;
    }

    print("[deimos] render_init: fb_map\n");
    long addr = fb_map();
    if (!addr) {
        print("[deimos] render_init: fb_map failed\n");
        return -1;
    }
    backbuffer = (uint8_t *)addr;

    print("[deimos] render_init: done\n");
    return 0;
}

int render_width(void)  { return (int)g_fb.width; }
int render_height(void) { return (int)g_fb.height; }
int render_bpp(void)    { return (int)g_fb.bpp; }
int render_pitch(void)  { return (int)g_fb.pitch; }

void render_begin_frame(uint32_t clear_colour) {
    if (!backbuffer) return;

    for (uint32_t y = 0; y < g_fb.height; y++) {
        for (uint32_t x = 0; x < g_fb.width; x++) {
            render_store_pixel((int)x, (int)y, clear_colour);
        }
    }
}

void render_end_frame(void) {
    if (!backbuffer) return;
    fb_present(backbuffer);
}

void render_putpixel(int x, int y, uint32_t colour) {
    if (!backbuffer) return;
    if ((unsigned)x >= g_fb.width || (unsigned)y >= g_fb.height) return;
    render_store_pixel(x, y, colour);
}

void render_clear(uint32_t colour) {
    render_begin_frame(colour);
}

void render_fill_rect(int x, int y, int w, int h, uint32_t colour) {
    if (!backbuffer) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > (int)g_fb.width  ? (int)g_fb.width  : x + w;
    int y1 = y + h > (int)g_fb.height ? (int)g_fb.height : y + h;

    for (int yy = y0; yy < y1; yy++) {
        for (int xx = x0; xx < x1; xx++) {
            render_store_pixel(xx, yy, colour);
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

void render_draw_char(int x, int y, char c, uint32_t colour) {
    const uint8_t *glyph = render_glyph_for_char(c);

    for (int row = 0; row < 7; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 5; col++) {
            if ((bits >> (4 - col)) & 1U) {
                render_putpixel(x + col, y + row, colour);
            }
        }
    }
}

void render_draw_text(int x, int y, const char *text, uint32_t colour) {
    if (!text) return;

    int pen_x = x;
    for (int i = 0; text[i]; i++) {
        render_draw_char(pen_x, y, text[i], colour);
        pen_x += 6; // 5px glyph + 1px spacing
    }
}

int render_text_width(const char *text) {
    if (!text) return 0;

    int len = 0;
    while (text[len]) len++;
    if (len == 0) return 0;
    return (len * 6) - 1;
}

void render_mark_dirty(int x, int w, int h) {
    (void)x; (void)w; (void)h;
}
void render_reset_dirty(void) {}
void render_present_full(void) { render_end_frame(); }
void render_present_dirty(void) { render_end_frame(); }
