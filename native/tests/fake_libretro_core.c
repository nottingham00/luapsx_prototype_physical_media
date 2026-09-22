#include "libretro_min.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static retro_environment_t env_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static uint32_t framebuf[320 * 240];
static unsigned frame_no;
static uint8_t saveram[128 * 1024];

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_set_environment(retro_environment_t cb) {
    env_cb = cb;
    static const struct retro_variable vars[] = {
        {"fake_option", "Fake option; enabled|disabled"},
        {NULL, NULL}
    };
    if (env_cb) env_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void *)vars);
}
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }
void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }

void retro_init(void) {
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    if (env_cb) env_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
}
void retro_deinit(void) {}
void retro_reset(void) { frame_no = 0; }

void retro_get_system_info(struct retro_system_info *info) {
    memset(info, 0, sizeof(*info));
    info->library_name = "LuaPSX Fake Core";
    info->library_version = "0.1";
    info->valid_extensions = "cue|bin";
    info->need_fullpath = true;
}
void retro_get_system_av_info(struct retro_system_av_info *info) {
    memset(info, 0, sizeof(*info));
    info->geometry.base_width = 320;
    info->geometry.base_height = 240;
    info->geometry.max_width = 320;
    info->geometry.max_height = 240;
    info->geometry.aspect_ratio = 4.0f / 3.0f;
    info->timing.fps = 60.0;
    info->timing.sample_rate = 44100.0;
}

bool retro_load_game(const struct retro_game_info *game) {
    if (!game || !game->path) return false;
    FILE *f = fopen(game->path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}
void retro_unload_game(void) {}
void *retro_get_memory_data(unsigned id) { return id == RETRO_MEMORY_SAVE_RAM ? saveram : NULL; }
size_t retro_get_memory_size(unsigned id) { return id == RETRO_MEMORY_SAVE_RAM ? sizeof(saveram) : 0; }

void retro_run(void) {
    if (input_poll_cb) input_poll_cb();
    if (input_state_cb) (void)input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);

    for (unsigned y = 0; y < 240; ++y) {
        for (unsigned x = 0; x < 320; ++x) {
            uint32_t r = (x + frame_no) & 255;
            uint32_t g = (y + frame_no) & 255;
            uint32_t b = (x + y + frame_no) & 255;
            framebuf[y * 320 + x] = (r << 16) | (g << 8) | b;
        }
    }
    if (video_cb) video_cb(framebuf, 320, 240, 320 * sizeof(uint32_t));

    int16_t audio[735 * 2];
    for (unsigned i = 0; i < 735; ++i) {
        int16_t s = (int16_t)(((i + frame_no * 735) % 100) * 100 - 5000);
        audio[i * 2] = s;
        audio[i * 2 + 1] = s;
    }
    if (audio_batch_cb) audio_batch_cb(audio, 735);
    else if (audio_cb) for (unsigned i = 0; i < 735; ++i) audio_cb(audio[i*2], audio[i*2+1]);
    saveram[0] = (uint8_t)(frame_no & 0xff);
    ++frame_no;
}
