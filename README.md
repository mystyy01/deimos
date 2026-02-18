# DEIMOS

Desktop Environment and Interface for Multitasking on Screens.

DEIMOS is the graphics/window-manager app layer that runs on top of PHOBOS syscall ABI.

## Layout

- `main.c` - current app entry/render loop bootstrap
- `rendering/` - framebuffer rendering helpers
- `window_manager/` - WM logic and input bridge
- `compositor/` - compositor work (C++/mt-lang experiments)

## Build

```bash
make all
```

Build and stage into superproject apps folder:

```bash
make stage APPS_DIR=../apps
```

Compile mt-lang modules only:

```bash
make mtc
```

## ABI / Includes

Use:

```c
#include <libsys.h>
```

The header is provided from `../phobos-kernel/uapi` through the Makefile include path.

## Notes

- `window_manager/input_bridge.c` exposes C wrappers (for mt-lang consumption) around input syscalls.
- Runtime config is loaded from `/cfg/deimos.conf` (in `testfs/cfg/deimos.conf` in this repo).
  Add new keybind/theme options there first, then consume them in code.
- In normal workflow, run DEIMOS through the superproject (`make run` at repo root).
