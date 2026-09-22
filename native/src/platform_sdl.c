#include "luapsx_platform.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *g_window;
static SDL_Renderer *g_renderer;
static SDL_Texture *g_texture;
static SDL_AudioDeviceID g_audio;
static SDL_GameController *g_pad;
static enum retro_pixel_format g_fmt = RETRO_PIXEL_FORMAT_0RGB1555;
static bool g_quit;
static uint32_t *g_pixels;
static size_t g_pixel_capacity;
static unsigned g_tex_w, g_tex_h;

static int recreate_texture(unsigned w, unsigned h) {
    if (g_texture && g_tex_w == w && g_tex_h == h) return 0;
    if (g_texture) SDL_DestroyTexture(g_texture);
    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING, (int)w, (int)h);
    if (!g_texture) {
        printf("[LuaPSX/SDL] SDL_CreateTexture: %s\n", SDL_GetError());
        return -1;
    }
    g_tex_w = w;
    g_tex_h = h;
    size_t need = (size_t)w * h;
    if (need > g_pixel_capacity) {
        uint32_t *p = (uint32_t *)realloc(g_pixels, need * sizeof(uint32_t));
        if (!p) return -1;
        g_pixels = p;
        g_pixel_capacity = need;
    }
    return 0;
}

static void open_first_controller(void) {
    int n = SDL_NumJoysticks();
    for (int i = 0; i < n; ++i) {
        if (!SDL_IsGameController(i)) continue;
        g_pad = SDL_GameControllerOpen(i);
        if (g_pad) {
            printf("[LuaPSX/SDL] controller: %s\n", SDL_GameControllerName(g_pad));
            return;
        }
    }
}

int luapsx_platform_init(const luapsx_platform_config_t *cfg) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        printf("[LuaPSX/SDL] SDL_Init: %s\n", SDL_GetError());
        return -1;
    }

    g_window = SDL_CreateWindow(cfg && cfg->title ? cfg->title : "LuaPSX",
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!g_window) return -1;

    g_renderer = SDL_CreateRenderer(g_window, -1,
                                    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer)
        g_renderer = SDL_CreateRenderer(g_window, -1, 0);
    if (!g_renderer) return -1;

    SDL_RenderSetLogicalSize(g_renderer, 1280, 720);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = cfg && cfg->sample_rate > 0 ? (int)cfg->sample_rate : 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    g_audio = SDL_OpenAudioDevice(NULL, 0, &want, &have,
                                  SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (g_audio) {
        printf("[LuaPSX/SDL] audio %d Hz %d ch\n", have.freq, have.channels);
        SDL_PauseAudioDevice(g_audio, 0);
    } else {
        printf("[LuaPSX/SDL] audio unavailable: %s\n", SDL_GetError());
    }

    open_first_controller();
    g_quit = false;
    return 0;
}

void luapsx_platform_shutdown(void) {
    if (g_audio) SDL_CloseAudioDevice(g_audio);
    if (g_pad) SDL_GameControllerClose(g_pad);
    if (g_texture) SDL_DestroyTexture(g_texture);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    free(g_pixels);
    g_pixels = NULL;
    g_pixel_capacity = 0;
    g_audio = 0; g_pad = NULL; g_texture = NULL; g_renderer = NULL; g_window = NULL;
    SDL_Quit();
}

void luapsx_platform_set_pixel_format(enum retro_pixel_format fmt) {
    g_fmt = fmt;
}

static inline uint32_t rgb565_to_argb(uint16_t p) {
    uint32_t r = (p >> 11) & 0x1f;
    uint32_t g = (p >> 5) & 0x3f;
    uint32_t b = p & 0x1f;
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
    return 0xff000000u | (r << 16) | (g << 8) | b;
}

static inline uint32_t rgb1555_to_argb(uint16_t p) {
    uint32_t r = (p >> 10) & 0x1f;
    uint32_t g = (p >> 5) & 0x1f;
    uint32_t b = p & 0x1f;
    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);
    return 0xff000000u | (r << 16) | (g << 8) | b;
}

void luapsx_platform_video(const void *data, unsigned width, unsigned height, size_t pitch) {
    if (!data || !g_renderer || recreate_texture(width, height) != 0) return;

    if (g_fmt == RETRO_PIXEL_FORMAT_XRGB8888) {
        for (unsigned y = 0; y < height; ++y) {
            const uint32_t *src = (const uint32_t *)((const uint8_t *)data + y * pitch);
            uint32_t *dst = g_pixels + (size_t)y * width;
            for (unsigned x = 0; x < width; ++x) dst[x] = 0xff000000u | src[x];
        }
    } else {
        for (unsigned y = 0; y < height; ++y) {
            const uint16_t *src = (const uint16_t *)((const uint8_t *)data + y * pitch);
            uint32_t *dst = g_pixels + (size_t)y * width;
            for (unsigned x = 0; x < width; ++x)
                dst[x] = g_fmt == RETRO_PIXEL_FORMAT_RGB565 ?
                         rgb565_to_argb(src[x]) : rgb1555_to_argb(src[x]);
        }
    }

    SDL_UpdateTexture(g_texture, NULL, g_pixels, (int)(width * sizeof(uint32_t)));

    int ww = 1280, wh = 720;
    SDL_GetRendererOutputSize(g_renderer, &ww, &wh);
    double aspect = height ? (double)width / (double)height : 4.0 / 3.0;
    int dw = ww;
    int dh = (int)(dw / aspect);
    if (dh > wh) { dh = wh; dw = (int)(dh * aspect); }
    SDL_Rect dst = { (ww - dw) / 2, (wh - dh) / 2, dw, dh };

    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, &dst);
    SDL_RenderPresent(g_renderer);
}

size_t luapsx_platform_audio(const int16_t *samples, size_t frames) {
    if (g_audio && samples && frames) {
        /* Keep latency bounded if the emulator momentarily outruns the device. */
        uint32_t queued = SDL_GetQueuedAudioSize(g_audio);
        if (queued > 44100u * 2u * sizeof(int16_t) / 3u)
            SDL_ClearQueuedAudio(g_audio);
        SDL_QueueAudio(g_audio, samples, (uint32_t)(frames * 2 * sizeof(int16_t)));
    }
    return frames;
}

void luapsx_platform_poll(void) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) g_quit = true;
        if (ev.type == SDL_CONTROLLERDEVICEADDED && !g_pad)
            open_first_controller();
        if (ev.type == SDL_CONTROLLERDEVICEREMOVED && g_pad) {
            SDL_JoystickID current = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_pad));
            if (current == ev.cdevice.which) {
                SDL_GameControllerClose(g_pad); g_pad = NULL; open_first_controller();
            }
        }
        if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) g_quit = true;
    }
}

static int button(SDL_GameControllerButton b) {
    return g_pad ? SDL_GameControllerGetButton(g_pad, b) : 0;
}

static int key(SDL_Scancode k) {
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    return keys ? keys[k] : 0;
}

static uint16_t joypad_mask(void) {
    uint16_t m = 0;
#define SETBIT(id, cond) do { if (cond) m |= (uint16_t)(1u << (id)); } while (0)
    SETBIT(RETRO_DEVICE_ID_JOYPAD_B,      button(SDL_CONTROLLER_BUTTON_A) || key(SDL_SCANCODE_Z));
    SETBIT(RETRO_DEVICE_ID_JOYPAD_Y,      button(SDL_CONTROLLER_BUTTON_X) || key(SDL_SCANCODE_A));
    SETBIT(RETRO_DEVICE_ID_JOYPAD_SELECT, button(SDL_CONTROLLER_BUTTON_BACK) || key(SDL_SCANCODE_RSHIFT));
    SETBIT(RETRO_DEVICE_ID_JOYPAD_START,  button(SDL_CONTROLLER_BUTTON_START) || key(SDL_SCANCODE_RETURN));
    SETBIT(RETRO_DEVICE_ID_JOYPAD_UP,     button(SDL_CONTROLLER_BUTTON_DPAD_UP) || key(SDL_SCANCODE_UP));
    SETBIT(RETRO_DEVICE_ID_JOYPAD_DOWN,   button(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || key(SDL_SCANCODE_DOWN));
    SETBIT(RETRO_DEVICE_ID_JOYPAD_LEFT,   button(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || key(SDL_SCANCODE_LEFT));
    SETBIT(RETRO_DEVICE_ID_JOYPAD_RIGHT,  button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || key(SDL_SCANCODE_RIGHT));
    SETBIT(RETRO_DEVICE_ID_JOYPAD_A,      button(SDL_CONTROLLER_BUTTON_B) || key(SDL_SCANCODE_X));
    SETBIT(RETRO_DEVICE_ID_JOYPAD_X,      button(SDL_CONTROLLER_BUTTON_Y) || key(SDL_SCANCODE_S));
    SETBIT(RETRO_DEVICE_ID_JOYPAD_L,      button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) || key(SDL_SCANCODE_Q));
    SETBIT(RETRO_DEVICE_ID_JOYPAD_R,      button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) || key(SDL_SCANCODE_W));
    SETBIT(RETRO_DEVICE_ID_JOYPAD_L2,     g_pad && SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 8000);
    SETBIT(RETRO_DEVICE_ID_JOYPAD_R2,     g_pad && SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 8000);
    SETBIT(RETRO_DEVICE_ID_JOYPAD_L3,     button(SDL_CONTROLLER_BUTTON_LEFTSTICK));
    SETBIT(RETRO_DEVICE_ID_JOYPAD_R3,     button(SDL_CONTROLLER_BUTTON_RIGHTSTICK));
#undef SETBIT
    return m;
}

int16_t luapsx_platform_input(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port != 0) return 0;
    unsigned base = device & RETRO_DEVICE_MASK;
    if (base == RETRO_DEVICE_JOYPAD) {
        uint16_t mask = joypad_mask();
        if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return (int16_t)mask;
        return id < 16 ? ((mask & (1u << id)) ? 1 : 0) : 0;
    }
    if (base == RETRO_DEVICE_ANALOG && g_pad) {
        SDL_GameControllerAxis axis;
        if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT)
            axis = id == RETRO_DEVICE_ID_ANALOG_X ? SDL_CONTROLLER_AXIS_LEFTX : SDL_CONTROLLER_AXIS_LEFTY;
        else
            axis = id == RETRO_DEVICE_ID_ANALOG_X ? SDL_CONTROLLER_AXIS_RIGHTX : SDL_CONTROLLER_AXIS_RIGHTY;
        return SDL_GameControllerGetAxis(g_pad, axis);
    }
    return 0;
}

bool luapsx_platform_should_quit(void) { return g_quit; }
uint64_t luapsx_platform_now_us(void) { return SDL_GetPerformanceCounter() * 1000000ULL / SDL_GetPerformanceFrequency(); }
void luapsx_platform_sleep_us(uint64_t us) { if (us >= 1000) SDL_Delay((Uint32)(us / 1000)); }
