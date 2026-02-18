#ifndef DEIMOS_WM_STATE_H
#define DEIMOS_WM_STATE_H

#define DEIMOS_MAX_WINDOWS 16

#define DEIMOS_SPLIT_TARGET_MOUSE 0
#define DEIMOS_SPLIT_TARGET_FOCUS 1

void deimos_wm_init(int default_x, int default_y);
int deimos_wm_window_count(void);
void deimos_wm_add_window_split(int x, int y, int target_mode);
int deimos_wm_set_focus_window_id(int window_id);
int deimos_wm_set_split_for_window_id(int window_id, int x, int y);

// Exposed for compositor.mtc extern calls.
int deimos_split_x(int index);
int deimos_split_y(int index);
int deimos_split_target_mode(int index);
int deimos_split_target_id(int index);
int deimos_focus_window_id(void);

#endif
