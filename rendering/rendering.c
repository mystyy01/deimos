#include "rendering.h"
#include "../../apps/libsys.h"

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
    
}
int render_pitch(void){
    
}

void render_begin_frame(uint32_t clear_colour){
    
}
void render_end_frame(void){
    
}

void render_clear(uint32_t colour){
    
}
void render_putpixel(int x, int y, uint32_t colour){
    
}
void render_fill_rect(int x, int y, int w, int h, uint32_t colour){
    
}
void render_draw_rect(int x, int y, int w, int h, uint32_t colour){
    
}

// needs font agnostic functions

void render_mark_dirty(int x, int w, int h){
    
}
void render_reset_dirty(void){
    
}
void render_present_full(void){
    
}