#ifndef DEIMOS_WM_STATE_H
#define DEIMOS_WM_STATE_H

#define DEIMOS_MAX_WINDOWS 16

#define DEIMOS_SPLIT_TARGET_MOUSE 0
#define DEIMOS_SPLIT_TARGET_FOCUS 1

void deimos_wm_init(int default_x, int default_y);
int deimos_wm_window_count(void);
void deimos_wm_add_window_split(int x, int y, int target_mode);
int deimos_wm_add_window_launch(int x, int y, int target_mode, int floating, int float_w, int float_h);
int deimos_wm_set_focus_window_id(int window_id);
int deimos_wm_close_focused_window(void);
int deimos_wm_set_split_for_window_id(int window_id, int x, int y);

// Exposed for compositor.mtc extern calls.
int deimos_split_x(int index);
int deimos_split_y(int index);
int deimos_split_target_mode(int index);
int deimos_split_target_id(int index);
int deimos_window_is_floating(int index);
int deimos_window_float_x(int index);
int deimos_window_float_y(int index);
int deimos_window_float_w(int index);
int deimos_window_float_h(int index);
int deimos_focus_window_id(void);

// Per-window kernel handle tracking (for managed app windows)
void deimos_wm_set_kernel_handle(int index, int kernel_handle);
int deimos_wm_get_kernel_handle(int index);
void deimos_wm_set_app_pid(int index, int pid);
int deimos_wm_get_app_pid(int index);

#endif
