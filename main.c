#include "rendering/rendering.h"
#include <libsys.h>

extern int deimos_compositor_test_frame_with_count(int window_count);
extern void mt_heap_reset(void);

static int g_split_x[16];
static int g_split_y[16];

int deimos_split_x(int index) {
    if (index < 0 || index >= 16) {
        return render_width() / 2;
    }
    return g_split_x[index];
}

int deimos_split_y(int index) {
    if (index < 0 || index >= 16) {
        return render_height() / 2;
    }
    return g_split_y[index];
}

static void add_window_split_point(int *window_count, int x, int y) {
    if (!window_count) return;
    if (*window_count < 0 || *window_count >= 16) return;
    g_split_x[*window_count] = x;
    g_split_y[*window_count] = y;
    (*window_count)++;
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

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    print("[deimos] starting\n");

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
    int test_window_count = 0;
    int mouse_x = render_width() / 2;
    int mouse_y = render_height() / 2;

    for (int i = 0; i < 16; i++) {
        g_split_x[i] = mouse_x;
        g_split_y[i] = mouse_y;
    }

    print("[deimos] N or left-click: new tile, X: quit\n");

    while (1) {
        int should_quit = 0;
        struct user_input_event ev;
        while (input_poll(&ev) == 1) {
            if (ev.type == INPUT_EVENT_MOUSE_MOVE || ev.type == INPUT_EVENT_MOUSE_BUTTON) {
                mouse_x = ev.mouse_x;
                mouse_y = ev.mouse_y;
            }

            if (ev.type == INPUT_EVENT_KEYBOARD && ev.pressed) {
                if (ev.key == 'x' || ev.key == 'X') {
                    should_quit = 1;
                } else if (ev.key == 'n' || ev.key == 'N') {
                    add_window_split_point(&test_window_count, mouse_x, mouse_y);
                }
            }

            if (ev.type == INPUT_EVENT_MOUSE_BUTTON &&
                ev.scancode == 1 &&
                ev.pressed == 1) {
                add_window_split_point(&test_window_count, mouse_x, mouse_y);
            }
        }

        if (should_quit) {
            break;
        }

        // Reclaim mt-lang temporary allocations from the previous frame.
        mt_heap_reset();
        render_begin_frame(0x101820);
        deimos_compositor_test_frame_with_count(test_window_count);

        render_fill_rect(mouse_x - 1, mouse_y - 1, 3, 3, 0xFFFFFF);

        frames_this_second++;
        uint64_t now = ticks();
        if (now - last_fps_tick >= ticks_per_second) {
            fps = frames_this_second;
            frames_this_second = 0;
            last_fps_tick = now;
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

        render_fill_rect(text_x - 3, text_y - 2, text_w + 6, 11, 0x000000);
        render_draw_text(text_x, text_y, fps_text, 0xE6E6E6);

        render_end_frame();
        yield();
    }

    exit(0);
    return 0;
}
