#include "luapsx_platform.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static enum retro_pixel_format g_fmt = RETRO_PIXEL_FORMAT_0RGB1555;
static unsigned g_frames;
static uint64_t g_audio_frames;

int luapsx_platform_init(const luapsx_platform_config_t *cfg) {
    g_frames = 0;
    g_audio_frames = 0;
    printf("[LuaPSX/headless] init %ux%u %.2f Hz %s\n",
           cfg ? cfg->width : 0, cfg ? cfg->height : 0,
           cfg ? cfg->sample_rate : 0.0,
           (cfg && cfg->title) ? cfg->title : "");
    return 0;
}

void luapsx_platform_shutdown(void) {
    printf("[LuaPSX/headless] shutdown: %u video frames, %llu audio frames\n",
           g_frames, (unsigned long long)g_audio_frames);
}

void luapsx_platform_set_pixel_format(enum retro_pixel_format fmt) {
    g_fmt = fmt;
    printf("[LuaPSX/headless] pixel format %d\n", (int)g_fmt);
}

void luapsx_platform_video(const void *data, unsigned width, unsigned height, size_t pitch) {
    (void)pitch;
    if (data) g_frames++;
    if (g_frames == 1)
        printf("[LuaPSX/headless] first frame %ux%u\n", width, height);
}

size_t luapsx_platform_audio(const int16_t *samples, size_t frames) {
    (void)samples;
    g_audio_frames += frames;
    return frames;
}

void luapsx_platform_poll(void) {}
int16_t luapsx_platform_input(unsigned port, unsigned device, unsigned index, unsigned id) {
    (void)port; (void)device; (void)index; (void)id;
    return 0;
}
bool luapsx_platform_should_quit(void) { return false; }

uint64_t luapsx_platform_now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

void luapsx_platform_sleep_us(uint64_t us) {
    struct timespec ts;
    ts.tv_sec = (time_t)(us / 1000000ULL);
    ts.tv_nsec = (long)((us % 1000000ULL) * 1000ULL);
    nanosleep(&ts, NULL);
}
