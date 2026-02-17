#include "rendering/rendering.h"
#include "../apps/libsys.h"

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

    // First visible bring-up frame.
    render_begin_frame(0x101820);
    print("[deimos] begin_frame ok\n");
    render_fill_rect(40, 40, 120, 70, 0x00AA66);
    print("[deimos] fill_rect ok\n");
    render_draw_rect(40, 40, 120, 70, 0xFFFFFF);
    print("[deimos] draw_rect ok\n");
    render_end_frame();
    print("[deimos] end_frame ok\n");

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
