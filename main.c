#include "rendering/rendering.h"
#include <libsys.h>

extern int deimos_compositor_test_frame_with_count(int window_count);
extern void mt_heap_reset(void);

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
    print("[deimos] press N for new window, X to quit\n");

    while (1) {
        struct user_input_event ev;
        if (input_poll(&ev) == 1 && ev.pressed) {
            if (ev.key == 'x' || ev.key == 'X') {
                break;
            }
            if (ev.key == 'n' || ev.key == 'N') {
                if (test_window_count < 16) {
                    test_window_count++;
                }
            }
        }

        // Reclaim mt-lang temporary allocations from the previous frame.
        mt_heap_reset();
        render_begin_frame(0x101820);
        deimos_compositor_test_frame_with_count(test_window_count);

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
