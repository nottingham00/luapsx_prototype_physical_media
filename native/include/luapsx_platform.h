#ifndef LUAPSX_PLATFORM_H
#define LUAPSX_PLATFORM_H

#include "libretro_min.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct luapsx_platform_config {
    unsigned width;
    unsigned height;
    double sample_rate;
    const char *title;
} luapsx_platform_config_t;

int luapsx_platform_init(const luapsx_platform_config_t *cfg);
void luapsx_platform_shutdown(void);
void luapsx_platform_set_pixel_format(enum retro_pixel_format fmt);
void luapsx_platform_video(const void *data, unsigned width, unsigned height, size_t pitch);
size_t luapsx_platform_audio(const int16_t *samples, size_t frames);
void luapsx_platform_poll(void);
int16_t luapsx_platform_input(unsigned port, unsigned device, unsigned index, unsigned id);
bool luapsx_platform_should_quit(void);
uint64_t luapsx_platform_now_us(void);
void luapsx_platform_sleep_us(uint64_t us);

#ifdef __cplusplus
}
#endif

#endif
