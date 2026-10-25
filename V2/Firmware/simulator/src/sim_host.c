/* Built in native_sim's host runner, with SDL and the host C library. */
#include <SDL.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include "sim_host.h"
#include "sim_layout.h"

static atomic_int wheel_milli_steps, orientation_milli_steps;

void sim_host_get_clock(unsigned *hour, unsigned *minute)
{
    /* Keep screenshots reproducible; normal runs use the PC's local clock. */
    if (getenv("DIVE_SIM_GALLERY")) {
        *hour = 14;
        *minute = 35;
        return;
    }
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    *hour = (unsigned)local.tm_hour;
    *minute = (unsigned)local.tm_min;
}

static int watch_events(void *unused, SDL_Event *event)
{
    (void)unused;
    if (event->type != SDL_MOUSEWHEEL) {
        return 1;
    }
    SDL_Window *window = SDL_GetWindowFromID(event->wheel.windowID);
    if (!window) {
        return 1;
    }
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    if (width <= 0 || height <= 0) {
        return 1;
    }
    int x, y;
#if SDL_VERSION_ATLEAST(2, 26, 0)
    /* SDL's renderer has already translated these to logical coordinates. */
    x = event->wheel.mouseX;
    y = event->wheel.mouseY;
#else
    SDL_GetMouseState(&x, &y);
    x = x * SIM_WIDTH / width;
    y = y * SIM_HEIGHT / height;
#endif
    bool crown = x >= SIM_PANEL_X && x < SIM_PANEL_X + SIM_CONTROL_WIDTH &&
                 y >= SIM_CROWN_Y && y < SIM_CROWN_Y + SIM_CROWN_HEIGHT;
    bool orientation = x >= SIM_ORIENTATION_X && x < SIM_ORIENTATION_X + SIM_ORIENTATION_WIDTH &&
                       y >= SIM_ORIENTATION_Y && y < SIM_ORIENTATION_Y + SIM_ORIENTATION_HEIGHT;
    if (crown || orientation) {
        int steps = (int)(event->wheel.preciseY * 1000.0f);
        if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) steps = -steps;
        atomic_fetch_add(orientation ? &orientation_milli_steps : &wheel_milli_steps, steps);
    }
    return 1;
}

void sim_host_init(void)
{
    SDL_AddEventWatch(watch_events, NULL);
}

int sim_host_take_wheel(void)
{
    static int remainder;
    remainder += atomic_exchange(&wheel_milli_steps, 0);
    int steps = remainder / 1000;
    remainder %= 1000;
    return steps;
}

int sim_host_check_enabled(void)
{
    return getenv("DIVE_SIM_CHECK_CONTROLS") != NULL;
}

const char *sim_host_capture_path(void)
{
    return getenv("DIVE_SIM_CAPTURE");
}

const char *sim_host_gallery_path(void)
{
    return getenv("DIVE_SIM_GALLERY");
}

/* The helpers below inject real SDL events for the headless interaction check. */
static SDL_Window *find_window(void)
{
    SDL_Window *window = SDL_GetKeyboardFocus();
    if (window) {
        return window;
    }
    /* SDL creates the simulator's only window with the first window ID. */
    return SDL_GetWindowFromID(1);
}

unsigned sim_host_get_zoom(void)
{
    SDL_Window *window = find_window();
    int width, height;
    if (!window) {
        return 2;
    }
    SDL_GetWindowSize(window, &width, &height);
    return (unsigned)(width / SIM_WIDTH);
}

int sim_host_set_zoom(unsigned zoom)
{
    SDL_Window *window = find_window();
    if (!window || (zoom != 1 && zoom != 2)) {
        return -1;
    }
    SDL_SetWindowSize(window, SIM_WIDTH * zoom, SIM_HEIGHT * zoom);
    /* Resolution and LVGL coordinates stay fixed; SDL scales the whole window. */
    return 0;
}

void sim_host_push_wheel(int x, int y, int steps)
{
    SDL_Window *window = find_window();
    if (!window) {
        return;
    }
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_MOUSEWHEEL;
    event.wheel.windowID = SDL_GetWindowID(window);
    event.wheel.y = steps;
    event.wheel.preciseY = (float)steps;
#if SDL_VERSION_ATLEAST(2, 26, 0)
    event.wheel.mouseX = x * width / SIM_WIDTH;
    event.wheel.mouseY = y * height / SIM_HEIGHT;
#else
    SDL_WarpMouseInWindow(window, x * width / SIM_WIDTH, y * height / SIM_HEIGHT);
#endif
    SDL_PushEvent(&event);
}

void sim_host_push_button(int x, int y, int pressed)
{
    SDL_Window *window = find_window();
    if (!window) {
        return;
    }
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_MOUSEMOTION;
    event.motion.windowID = SDL_GetWindowID(window);
    event.motion.x = x * width / SIM_WIDTH;
    event.motion.y = y * height / SIM_HEIGHT;
    SDL_PushEvent(&event);
    SDL_zero(event);
    event.type = pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    event.button.windowID = SDL_GetWindowID(window);
    event.button.button = SDL_BUTTON_LEFT;
    event.button.state = pressed ? SDL_PRESSED : SDL_RELEASED;
    event.button.x = x * width / SIM_WIDTH;
    event.button.y = y * height / SIM_HEIGHT;
    SDL_PushEvent(&event);
}

int sim_host_capture(const char *path)
{
    SDL_Window *window = find_window();
    SDL_Renderer *renderer = window ? SDL_GetRenderer(window) : NULL;
    int width, height;
    if (!renderer || SDL_GetRendererOutputSize(renderer, &width, &height) != 0) {
        return -1;
    }
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
                                                         SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        return -1;
    }
    int result = SDL_RenderReadPixels(renderer, NULL, surface->format->format,
                                      surface->pixels, surface->pitch);
    if (result == 0) {
        result = SDL_SaveBMP(surface, path);
    }
    SDL_FreeSurface(surface);
    return result;
}

int sim_host_water_check_enabled(void) { return getenv("DIVE_SIM_WATER_CHECK") != NULL; }

int sim_host_dive_check_enabled(void) { return getenv("DIVE_SIM_DIVE_CHECK") != NULL; }

int sim_host_take_orientation(void)
{
    static int remainder;
    remainder += atomic_exchange(&orientation_milli_steps, 0);
    int steps = remainder / 1000;
    remainder %= 1000;
    return steps;
}

int sim_host_scuba_check_enabled(void) {return getenv("DIVE_SIM_SCUBA_CHECK") != NULL;}
