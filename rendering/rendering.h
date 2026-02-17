#ifndef RENDERING_H
#define RENDERING_H

#include <stdint.h>

int render_init(void);

int render_width(void);
int render_height(void);
int render_bpp(void);
int render_pitch(void);

void render_begin_frame(uint32_t clear_colour);
void render_end_frame(void);

void render_clear(uint32_t colour);
void render_putpixel(int x, int y, uint32_t colour);
void render_fill_rect(int x, int y, int w, int h, uint32_t colour);
void render_draw_rect(int x, int y, int w, int h, uint32_t colour);
void render_draw_char(int x, int y, char c, uint32_t colour);
void render_draw_text(int x, int y, const char *text, uint32_t colour);
int render_text_width(const char *text);

// needs font agnostic functions

void render_mark_dirty(int x, int w, int h);
void render_reset_dirty(void);
void render_present_full(void);
void render_present_dirty(void);


#endif
