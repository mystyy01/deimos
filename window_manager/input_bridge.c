#include <libsys.h>
#include <stdint.h>

static struct user_input_event g_ev;
int deimos_input_poll(void) { return input_poll(&g_ev); }
int deimos_input_key(void) { return g_ev.key; }
int deimos_input_mods(void) { return g_ev.modifiers; }
int deimos_input_pressed(void) { return g_ev.pressed; }
int deimos_input_scancode(void) { return g_ev.scancode; }