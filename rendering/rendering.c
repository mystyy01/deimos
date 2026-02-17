#include "rendering.h"
#include "../../apps/libsys.h" // should prolly copy over the header file into this so it doesnt rely on the exact file structure of this repo

static struct user_fb_info g_fb;

int render_init(void) {
    int rc = fb_info(&g_fb);
    if (rc != 0) {
        return -1;
    }

    if (g_fb.width == 0 || g_fb.height == 0) {
        return -1;
    }

    return 0;
}
int render_width(void){
    return (int)g_fb.width;
}
int render_height(void){
    return (int)g_fb.height;
}
int render_bpp(void){
    return (int)g_fb.bpp;    
}
int render_pitch(void){
    return (int)g_fb.pitch;
}

void render_begin_frame(uint32_t clear_colour) {
    (void)clear_colour;
    // Full-screen clear via fb_putpixel syscall is too slow for now.
    // TODO(HUMAN): Re-enable once SYS_FB_PRESENT/backbuffer path exists.
}

void render_end_frame(void) {
    // TODO(HUMAN): Present/backbuffer logic will go here.
}

void render_putpixel(int x, int y, uint32_t colour) {
    if (x < 0 || y < 0) return;
    if (x >= render_width() || y >= render_height()) return;
    fb_putpixel(x, y, colour);
}

void render_clear(uint32_t colour) {
    for (int y = 0; y < render_height(); y++) {
        for (int x = 0; x < render_width(); x++) {
            render_putpixel(x, y, colour);
        }
    }
}

void render_fill_rect(int x, int y, int w, int h, uint32_t colour) {
    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            render_putpixel(x + xx, y + yy, colour);
        }
    }
}

void render_draw_rect(int x, int y, int w, int h, uint32_t colour) {
    for (int xx = 0; xx < w; xx++) {
        render_putpixel(x + xx, y, colour);
        render_putpixel(x + xx, y + h - 1, colour);
    }
    for (int yy = 0; yy < h; yy++) {
        render_putpixel(x, y + yy, colour);
        render_putpixel(x + w - 1, y + yy, colour);
    }
}

// needs font agnostic functions

void render_mark_dirty(int x, int w, int h){
    (void)x;
    (void)w;
    (void)h;
}
void render_reset_dirty(void){

}
void render_present_full(void){

}
void render_present_dirty(void){

}
