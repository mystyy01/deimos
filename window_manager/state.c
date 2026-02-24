#include "state.h"

static int g_window_count = 0;
static int g_focused_window_id = -1;
static int g_split_x[DEIMOS_MAX_WINDOWS];
static int g_split_y[DEIMOS_MAX_WINDOWS];
static int g_split_target_mode[DEIMOS_MAX_WINDOWS];
static int g_split_target_id[DEIMOS_MAX_WINDOWS];
static int g_window_floating[DEIMOS_MAX_WINDOWS];
static int g_window_kernel_handle[DEIMOS_MAX_WINDOWS];
static int g_window_app_pid[DEIMOS_MAX_WINDOWS];
static int g_window_float_x[DEIMOS_MAX_WINDOWS];
static int g_window_float_y[DEIMOS_MAX_WINDOWS];
static int g_window_float_w[DEIMOS_MAX_WINDOWS];
static int g_window_float_h[DEIMOS_MAX_WINDOWS];

static int clamp_min(int v, int min_v) {
    if (v < min_v) return min_v;
    return v;
}

void deimos_wm_init(int default_x, int default_y) {
    g_window_count = 0;
    g_focused_window_id = -1;

    for (int i = 0; i < DEIMOS_MAX_WINDOWS; i++) {
        g_split_x[i] = default_x;
        g_split_y[i] = default_y;
        g_split_target_mode[i] = DEIMOS_SPLIT_TARGET_MOUSE;
        g_split_target_id[i] = -1;
        g_window_floating[i] = 0;
        g_window_kernel_handle[i] = -1;
        g_window_app_pid[i] = 0;
        g_window_float_w[i] = 360;
        g_window_float_h[i] = 220;
        g_window_float_x[i] = default_x - (g_window_float_w[i] / 2);
        g_window_float_y[i] = default_y - (g_window_float_h[i] / 2);
    }
}

int deimos_wm_window_count(void) {
    return g_window_count;
}

void deimos_wm_add_window_split(int x, int y, int target_mode) {
    (void)deimos_wm_add_window_launch(x, y, target_mode, 0, 0, 0);
}

int deimos_wm_add_window_launch(int x, int y, int target_mode, int floating, int float_w, int float_h) {
    if (g_window_count < 0 || g_window_count >= DEIMOS_MAX_WINDOWS) {
        return -1;
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

    g_window_floating[index] = floating ? 1 : 0;
    g_window_kernel_handle[index] = -1;
    g_window_app_pid[index] = 0;
    g_window_float_w[index] = clamp_min(float_w, 120);
    g_window_float_h[index] = clamp_min(float_h, 80);
    if (g_window_float_w[index] <= 120 && float_w <= 0) g_window_float_w[index] = 420;
    if (g_window_float_h[index] <= 80 && float_h <= 0) g_window_float_h[index] = 260;
    g_window_float_x[index] = x - (g_window_float_w[index] / 2);
    g_window_float_y[index] = y - (g_window_float_h[index] / 2);

    // Window IDs are assigned in compositor from 1..N.
    g_focused_window_id = index + 1;
    g_window_count++;
    return g_focused_window_id;
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

int deimos_wm_close_focused_window(void) {
    if (g_window_count <= 0) return 0;
    if (g_focused_window_id <= 0 || g_focused_window_id > g_window_count) return 0;

    int index = g_focused_window_id - 1;
    for (int i = index; i < g_window_count - 1; i++) {
        g_split_x[i] = g_split_x[i + 1];
        g_split_y[i] = g_split_y[i + 1];
        g_split_target_mode[i] = g_split_target_mode[i + 1];
        g_split_target_id[i] = g_split_target_id[i + 1];
        g_window_floating[i] = g_window_floating[i + 1];
        g_window_kernel_handle[i] = g_window_kernel_handle[i + 1];
        g_window_app_pid[i] = g_window_app_pid[i + 1];
        g_window_float_x[i] = g_window_float_x[i + 1];
        g_window_float_y[i] = g_window_float_y[i + 1];
        g_window_float_w[i] = g_window_float_w[i + 1];
        g_window_float_h[i] = g_window_float_h[i + 1];
    }

    g_window_count--;
    if (g_window_count <= 0) {
        g_window_count = 0;
        g_focused_window_id = -1;
        return 1;
    }

    if (index >= g_window_count) {
        g_focused_window_id = g_window_count;
    } else {
        g_focused_window_id = index + 1;
    }

    return 1;
}

int deimos_wm_set_split_for_window_id(int window_id, int x, int y) {
    int index = window_id - 1;
    if (index < 0 || index >= g_window_count || index >= DEIMOS_MAX_WINDOWS) {
        return 0;
    }

    if (g_split_x[index] == x && g_split_y[index] == y) {
        if (!g_window_floating[index]) {
            return 0;
        }
    }

    g_split_x[index] = x;
    g_split_y[index] = y;
    if (g_window_floating[index]) {
        int next_x = x - (g_window_float_w[index] / 2);
        int next_y = y - (g_window_float_h[index] / 2);
        if (g_window_float_x[index] == next_x && g_window_float_y[index] == next_y) {
            return 0;
        }
        g_window_float_x[index] = next_x;
        g_window_float_y[index] = next_y;
    }
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

int deimos_window_is_floating(int index) {
    if (index < 0 || index >= DEIMOS_MAX_WINDOWS) return 0;
    return g_window_floating[index];
}

int deimos_window_float_x(int index) {
    if (index < 0 || index >= DEIMOS_MAX_WINDOWS) return 0;
    return g_window_float_x[index];
}

int deimos_window_float_y(int index) {
    if (index < 0 || index >= DEIMOS_MAX_WINDOWS) return 0;
    return g_window_float_y[index];
}

int deimos_window_float_w(int index) {
    if (index < 0 || index >= DEIMOS_MAX_WINDOWS) return 1;
    return g_window_float_w[index];
}

int deimos_window_float_h(int index) {
    if (index < 0 || index >= DEIMOS_MAX_WINDOWS) return 1;
    return g_window_float_h[index];
}

int deimos_focus_window_id(void) {
    return g_focused_window_id;
}

void deimos_wm_set_kernel_handle(int index, int kernel_handle) {
    if (index < 0 || index >= DEIMOS_MAX_WINDOWS) return;
    g_window_kernel_handle[index] = kernel_handle;
}

int deimos_wm_get_kernel_handle(int index) {
    if (index < 0 || index >= DEIMOS_MAX_WINDOWS) return -1;
    return g_window_kernel_handle[index];
}

void deimos_wm_set_app_pid(int index, int pid) {
    if (index < 0 || index >= DEIMOS_MAX_WINDOWS) return;
    g_window_app_pid[index] = pid;
}

int deimos_wm_get_app_pid(int index) {
    if (index < 0 || index >= DEIMOS_MAX_WINDOWS) return 0;
    return g_window_app_pid[index];
}
