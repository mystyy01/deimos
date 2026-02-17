#include "compositor.hpp"

/*
PHOBOS Compositor + Window Manager Skeleton - WHAT THIS FILE NEEDS TO DO

===========================================================
1. Window Class (represents a single window)
-----------------------------------------------------------
- Track window position (x, y), size (width, height)
- Track logical properties:
    - zIndex (stacking order)
    - focused (is it active?)
    - visible (hidden/minimized)
- Track rendering info:
    - buffer pointer (pixel data)
    - decorations info (titlebar, borders, colors)
- Track client/app reference:
    - clientHandle (pointer to PHOBOS app or Wayland-like surface)
- Methods:
    - move(x, y)
    - resize(width, height)
    - optionally: toggle focus, toggle visibility
- Notes: The window itself does NOT draw anything, just holds info.

*/

class Window{
public:
    // fields and constructor
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    int zIndex = 0;
    bool focused = false;
    bool visible = true;
    Window() = default;
    Window(int xpos, int ypos, int width, int height){
        x = xpos;
        y = ypos;
        w = width;
        h = height;
    };
    // methods
    void move(int toX, int toY){
        x = toX;
        y = toY;
    };
    void resize(int width, int height){
        w = width;
        h = height;
    };
    void show(){
        visible = true;
    };
    void hide(){
        visible = false;
    };
};

/*
===========================================================
3. Compositor Class (responsible for drawing)
-----------------------------------------------------------
- Stores list of windows (manual array or fixed-size array if no STL)
- Responsible for:
    - rendering windows in z-index order
    - drawing decorations (borders, titlebars, shadows)
    - handling raw framebuffer / GPU buffers
    - presenting the final frame to the screen
    - optionally: effects like transparency, fade, animations
- Methods:
    - addWindow(Window* win)
    - removeWindow(Window* win)
    - render()  // main draw function
    - drawWindow(Window* win) // helper to draw individual window + decorations
    - optionally: drawDecorations(Window* win), drawShadow(Window* win)
- Notes:
    - Compositor may sort windows by zIndex before drawing
    - Reads window fields set by WM to know where/how to draw
    - Could handle input forwarding (or leave that to WM)
*/

class Compositor{
public:
    Window windows[MAX_WINDOWS]; // think this needs to be on the heap

    // methods
    void add_window(Window* win){
        
    };

};

/*
===========================================================
4. Input Handling
-----------------------------------------------------------
- Capture keyboard/mouse events (low-level from kernel or libinput)
- Forward input to focused window:
    - WM decides which window is focused
    - Compositor routes events to that window’s clientHandle
- Optional global shortcuts (e.g., switch workspace, close window)
- Notes:
    - This can be part of WM or a separate InputManager class
    - Keep logic modular: compositor only knows where events go

===========================================================
5. Frame / Rendering Loop
-----------------------------------------------------------
- Initialize framebuffer / GPU
- Main loop:
    - poll input events
    - update WM state (focus, move, resize, layout)
    - compositor.render() to draw all windows
    - present the frame
- Optional:
    - damage tracking: only redraw areas that changed
    - smooth animations / effects

===========================================================
6. Decorations & Effects (Compositor responsibility)
-----------------------------------------------------------
- Draw titlebars/borders based on window.focused
- Shadows, transparency, fade-in/out
- Optional animations: resizing, moving, tiling

===========================================================
7. Window Lifecycle
-----------------------------------------------------------
- CreateWindow / DestroyWindow functions
- Notify WM and Compositor about new/destroyed windows
- Update internal arrays or lists
- Make sure zIndex and focus is updated when windows are created/destroyed

===========================================================
8. Modular Notes
-----------------------------------------------------------
- Keep WM logic separate from rendering logic:
    - WM decides **what happens**
    - Compositor decides **how it looks**
- Window class is shared between them
- Input can go through WM, compositor, or both
- Everything should be STL-free if desired:
    - Use fixed-size arrays or manual memory management
- Start simple:
    - Draw rectangles as windows first
    - Add focus, stacking, move/resize
    - Add decorations
    - Add effects later

===========================================================
9. Optional Features to Add After Core Works
-----------------------------------------------------------
- Multiple workspaces / virtual desktops
- Tiling layouts
- Mouse-based window move/resize
- Animations (fade, slide, scale)
- Transparency, shadows
- Wayland or networked client support

===========================================================
10. Suggested Development Flow
-----------------------------------------------------------
1. Make a Window class and simple array of windows
2. Make Compositor class that draws rectangles
3. Make WindowManager class that can focus/move/resize windows
4. Integrate input: move focus, move windows with keys
5. Add decorations
6. Add z-index sorting
7. Add effects/animations
8. Expand to multiple workspaces, tiling layouts
9. Optionally hook in real clients or Wayland buffers
*/
