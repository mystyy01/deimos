#include "state.h"

static int g_window_count = 0;
static int g_focused_window_id = -1;
static int g_split_x[DEIMOS_MAX_WINDOWS];
static int g_split_y[DEIMOS_MAX_WINDOWS];
static int g_split_target_mode[DEIMOS_MAX_WINDOWS];
static int g_split_target_id[DEIMOS_MAX_WINDOWS];

void deimos_wm_init(int default_x, int default_y) {
    g_window_count = 0;
    g_focused_window_id = -1;

    for (int i = 0; i < DEIMOS_MAX_WINDOWS; i++) {
        g_split_x[i] = default_x;
        g_split_y[i] = default_y;
        g_split_target_mode[i] = DEIMOS_SPLIT_TARGET_MOUSE;
        g_split_target_id[i] = -1;
    }
}

int deimos_wm_window_count(void) {
    return g_window_count;
}

void deimos_wm_add_window_split(int x, int y, int target_mode) {
    if (g_window_count < 0 || g_window_count >= DEIMOS_MAX_WINDOWS) {
        return;
    }

    int index = g_window_count;
    g_split_x[index] = x;
    g_split_y[index] = y;
    g_split_target_mode[index] = target_mode;
    if (target_mode == DEIMOS_SPLIT_TARGET_FOCUS) {
        g_split_target_id[index] = g_focused_window_id;
    } else {
        g_split_target_id[index] = -1;
    }

    // Window IDs are assigned in compositor from 1..N.
    g_focused_window_id = index + 1;
    g_window_count++;
}

int deimos_wm_set_focus_window_id(int window_id) {
    int next_focus = g_focused_window_id;

    if (window_id >= 1 && window_id <= g_window_count) {
        next_focus = window_id;
    }

    if (next_focus == g_focused_window_id) {
        return 0;
    }

    g_focused_window_id = next_focus;
    return 1;
}

int deimos_wm_set_split_for_window_id(int window_id, int x, int y) {
    int index = window_id - 1;
    if (index < 0 || index >= g_window_count || index >= DEIMOS_MAX_WINDOWS) {
        return 0;
    }

    if (g_split_x[index] == x && g_split_y[index] == y) {
        return 0;
    }

    g_split_x[index] = x;
    g_split_y[index] = y;
    return 1;
}

int deimos_split_x(int index) {
    if (index < 0 || index >= DEIMOS_MAX_WINDOWS) {
        return 0;
    }
    return g_split_x[index];
}

int deimos_split_y(int index) {
    if (index < 0 || index >= DEIMOS_MAX_WINDOWS) {
        return 0;
    }
    return g_split_y[index];
}

int deimos_split_target_mode(int index) {
    if (index < 0 || index >= DEIMOS_MAX_WINDOWS) {
        return DEIMOS_SPLIT_TARGET_MOUSE;
    }
    return g_split_target_mode[index];
}

int deimos_split_target_id(int index) {
    if (index < 0 || index >= DEIMOS_MAX_WINDOWS) {
        return -1;
    }
    return g_split_target_id[index];
}

int deimos_focus_window_id(void) {
    return g_focused_window_id;
}
