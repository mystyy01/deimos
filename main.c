#include "rendering/rendering.h"
#include "../apps/libsys.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    if (render_init() != 0) {
        print("deimos: render_init failed\n");
        exit(1);
        return 1;
    }

    // First visible bring-up frame.
    render_begin_frame(0x101820);
    render_fill_rect(100, 100, 300, 150, 0x00AA66);
    render_draw_rect(100, 100, 300, 150, 0xFFFFFF);
    render_end_frame();

    print("deimos: frame drawn (press q to quit)\n");

    while (1) {
        struct user_input_event ev;
        if (input_poll(&ev) == 1 && ev.pressed) {
            if (ev.key == 'q' || ev.key == 'Q') {
                break;
            }
        }
        yield();
    }

    exit(0);
    return 0;
}
