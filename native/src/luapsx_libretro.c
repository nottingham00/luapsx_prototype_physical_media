#include "luapsx_libretro.h"
#include "luapsx_platform.h"
#include "libretro_min.h"

#include <dlfcn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define LUAPSX_MAX_OPTIONS 128
#define LUAPSX_KEY_MAX 128
#define LUAPSX_VALUE_MAX 256

typedef struct luapsx_option {
    char key[LUAPSX_KEY_MAX];
    char value[LUAPSX_VALUE_MAX];
} luapsx_option_t;

typedef struct luapsx_core {
    void *handle;
    retro_api_version_fn api_version;
    retro_init_fn init;
    retro_deinit_fn deinit;
    retro_get_system_info_fn get_system_info;
    retro_get_system_av_info_fn get_system_av_info;
    retro_set_environment_fn set_environment;
    retro_set_video_refresh_fn set_video_refresh;
    retro_set_audio_sample_fn set_audio_sample;
    retro_set_audio_sample_batch_fn set_audio_sample_batch;
    retro_set_input_poll_fn set_input_poll;
    retro_set_input_state_fn set_input_state;
    retro_set_controller_port_device_fn set_controller_port_device;
    retro_load_game_fn load_game;
    retro_unload_game_fn unload_game;
    retro_run_fn run;
    retro_reset_fn reset;
    retro_get_memory_data_fn get_memory_data;
    retro_get_memory_size_fn get_memory_size;
} luapsx_core_t;

static struct {
    const char *system_dir;
    const char *save_dir;
    enum retro_pixel_format pixel_format;
    bool quit;
    bool variable_update;
    luapsx_option_t options[LUAPSX_MAX_OPTIONS];
    size_t option_count;
    struct retro_disk_control_callback disk;
    bool has_disk_control;
} g_env;

static void frontend_log(enum retro_log_level level, const char *fmt, ...) {
    static const char *names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    const char *name = (level >= RETRO_LOG_DEBUG && level <= RETRO_LOG_ERROR) ? names[level] : "LOG";
    va_list ap;
    fprintf(stderr, "[LuaPSX/core/%s] ", name);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static int mkdir_if_missing(const char *path) {
    if (!path || !*path) return -1;
    if (mkdir(path, 0777) == 0 || errno == EEXIST) return 0;
    return -1;
}

static void parse_default_option(const struct retro_variable *v) {
    if (!v || !v->key || !v->value || g_env.option_count >= LUAPSX_MAX_OPTIONS) return;

    const char *semi = strchr(v->value, ';');
    const char *start = semi ? semi + 1 : v->value;
    while (*start == ' ') ++start;
    const char *end = strchr(start, '|');
    size_t len = end ? (size_t)(end - start) : strlen(start);
    if (len >= LUAPSX_VALUE_MAX) len = LUAPSX_VALUE_MAX - 1;

    luapsx_option_t *opt = &g_env.options[g_env.option_count++];
    snprintf(opt->key, sizeof(opt->key), "%s", v->key);
    memcpy(opt->value, start, len);
    opt->value[len] = '\0';
}

static const char *find_option(const char *key) {
    for (size_t i = 0; i < g_env.option_count; ++i)
        if (!strcmp(g_env.options[i].key, key)) return g_env.options[i].value;
    return NULL;
}

static bool frontend_environment(unsigned cmd, void *data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            if (data) *(bool *)data = true;
            return true;
        case RETRO_ENVIRONMENT_SET_MESSAGE: {
            const struct retro_message *m = (const struct retro_message *)data;
            if (m && m->msg) printf("[LuaPSX/message] %s\n", m->msg);
            return true;
        }
        case RETRO_ENVIRONMENT_SHUTDOWN:
            g_env.quit = true;
            return true;
        case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
            return true;
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
            if (data) *(const char **)data = g_env.system_dir;
            return true;
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            if (!data) return false;
            g_env.pixel_format = *(const enum retro_pixel_format *)data;
            if (g_env.pixel_format != RETRO_PIXEL_FORMAT_0RGB1555 &&
                g_env.pixel_format != RETRO_PIXEL_FORMAT_XRGB8888 &&
                g_env.pixel_format != RETRO_PIXEL_FORMAT_RGB565)
                return false;
            luapsx_platform_set_pixel_format(g_env.pixel_format);
            return true;
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
            return true;
        case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
            if (!data) return false;
            memcpy(&g_env.disk, data, sizeof(g_env.disk));
            g_env.has_disk_control = true;
            return true;
        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            struct retro_variable *v = (struct retro_variable *)data;
            if (!v || !v->key) return false;
            v->value = find_option(v->key);
            return true;
        }
        case RETRO_ENVIRONMENT_SET_VARIABLES: {
            const struct retro_variable *vars = (const struct retro_variable *)data;
            g_env.option_count = 0;
            if (vars) {
                for (; vars->key && g_env.option_count < LUAPSX_MAX_OPTIONS; ++vars)
                    parse_default_option(vars);
            }
            return true;
        }
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
            if (data) *(bool *)data = g_env.variable_update;
            g_env.variable_update = false;
            return true;
        case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES:
            if (data) *(uint64_t *)data = (1ULL << RETRO_DEVICE_JOYPAD) | (1ULL << RETRO_DEVICE_ANALOG);
            return true;
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
            if (data) ((struct retro_log_callback *)data)->log = frontend_log;
            return true;
        case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
            if (data) *(const char **)data = g_env.system_dir;
            return true;
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
            if (data) *(const char **)data = g_env.save_dir;
            return true;
        case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
            return true;
        case RETRO_ENVIRONMENT_GET_LANGUAGE:
            if (data) *(unsigned *)data = RETRO_LANGUAGE_ENGLISH;
            return true;
        case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
            return true;
        default:
            return false;
    }
}

static void frontend_video(const void *data, unsigned width, unsigned height, size_t pitch) {
    luapsx_platform_video(data, width, height, pitch);
}

static void frontend_audio_sample(int16_t left, int16_t right) {
    int16_t pair[2] = {left, right};
    luapsx_platform_audio(pair, 1);
}

static size_t frontend_audio_batch(const int16_t *data, size_t frames) {
    return luapsx_platform_audio(data, frames);
}

static void frontend_input_poll(void) {
    luapsx_platform_poll();
}

static int16_t frontend_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
    return luapsx_platform_input(port, device, index, id);
}

static int build_save_path(const char *game_path, const char *save_dir,
                           char *out, size_t out_size) {
    const char *base = strrchr(game_path, '/');
#ifdef _WIN32
    const char *back = strrchr(game_path, '\\');
    if (!base || (back && back > base)) base = back;
#endif
    base = base ? base + 1 : game_path;
    if (!base || !*base) return -1;

    char name[256];
    if (snprintf(name, sizeof(name), "%s", base) >= (int)sizeof(name)) return -1;
    char *dot = strrchr(name, '.');
    if (dot && dot != name) *dot = '\0';

    int n = snprintf(out, out_size, "%s/%s.srm", save_dir, name);
    return n >= 0 && n < (int)out_size ? 0 : -1;
}

static int load_saveram(luapsx_core_t *core, const char *path) {
    size_t size = core->get_memory_size(RETRO_MEMORY_SAVE_RAM);
    void *data = core->get_memory_data(RETRO_MEMORY_SAVE_RAM);
    if (!size || !data) return 0;

    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("[LuaPSX] new memory card: %s (%zu bytes)\n", path, size);
        return 0;
    }
    size_t got = fread(data, 1, size, f);
    fclose(f);
    printf("[LuaPSX] loaded memory card: %s (%zu/%zu bytes)\n", path, got, size);
    return 0;
}

static int save_saveram(luapsx_core_t *core, const char *path) {
    size_t size = core->get_memory_size(RETRO_MEMORY_SAVE_RAM);
    const void *data = core->get_memory_data(RETRO_MEMORY_SAVE_RAM);
    if (!size || !data || !path || !*path) return 0;

    char tmp[1024];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) return -1;
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    if (fwrite(data, 1, size, f) != size || fflush(f) != 0) {
        fclose(f);
        remove(tmp);
        return -1;
    }
    fclose(f);
    remove(path);
    if (rename(tmp, path) != 0) {
        remove(tmp);
        return -1;
    }
    return 0;
}

static int load_symbol(void *handle, void **out, const char *name) {
    *out = dlsym(handle, name);
    if (!*out) {
        fprintf(stderr, "[LuaPSX] core is missing symbol %s: %s\n", name, dlerror());
        return -1;
    }
    return 0;
}

#define LOADSYM(c, member, name) do { \
    if (load_symbol((c)->handle, (void **)&(c)->member, name) != 0) return -1; \
} while (0)

static int core_open(luapsx_core_t *c, const char *path) {
    memset(c, 0, sizeof(*c));
    c->handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!c->handle) {
        fprintf(stderr, "[LuaPSX] dlopen(%s): %s\n", path, dlerror());
        return -1;
    }

    LOADSYM(c, api_version, "retro_api_version");
    LOADSYM(c, init, "retro_init");
    LOADSYM(c, deinit, "retro_deinit");
    LOADSYM(c, get_system_info, "retro_get_system_info");
    LOADSYM(c, get_system_av_info, "retro_get_system_av_info");
    LOADSYM(c, set_environment, "retro_set_environment");
    LOADSYM(c, set_video_refresh, "retro_set_video_refresh");
    LOADSYM(c, set_audio_sample, "retro_set_audio_sample");
    LOADSYM(c, set_audio_sample_batch, "retro_set_audio_sample_batch");
    LOADSYM(c, set_input_poll, "retro_set_input_poll");
    LOADSYM(c, set_input_state, "retro_set_input_state");
    LOADSYM(c, set_controller_port_device, "retro_set_controller_port_device");
    LOADSYM(c, load_game, "retro_load_game");
    LOADSYM(c, unload_game, "retro_unload_game");
    LOADSYM(c, run, "retro_run");
    LOADSYM(c, reset, "retro_reset");
    LOADSYM(c, get_memory_data, "retro_get_memory_data");
    LOADSYM(c, get_memory_size, "retro_get_memory_size");

    if (c->api_version() != RETRO_API_VERSION) {
        fprintf(stderr, "[LuaPSX] unsupported libretro API version %u\n", c->api_version());
        return -1;
    }
    return 0;
}

static void core_close(luapsx_core_t *c) {
    if (c && c->handle) dlclose(c->handle);
    if (c) memset(c, 0, sizeof(*c));
}

int luapsx_run_libretro(const luapsx_frontend_config_t *cfg) {
    if (!cfg || !cfg->core_path || !cfg->game_path) return -1;

    luapsx_core_t core;
    struct retro_system_info info;
    struct retro_system_av_info av;
    struct retro_game_info game;
    luapsx_platform_config_t platform_cfg;
    int rc = -1;
    bool core_initialized = false;
    bool game_loaded = false;
    bool platform_initialized = false;
    char save_path[1024] = {0};

    memset(&g_env, 0, sizeof(g_env));
    g_env.system_dir = cfg->system_dir ? cfg->system_dir : ".";
    g_env.save_dir = cfg->save_dir ? cfg->save_dir : g_env.system_dir;
    g_env.pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;

    mkdir_if_missing(g_env.system_dir);
    mkdir_if_missing(g_env.save_dir);

    if (core_open(&core, cfg->core_path) != 0) goto cleanup;

    memset(&info, 0, sizeof(info));
    core.get_system_info(&info);
    printf("[LuaPSX] core: %s %s; extensions=%s; fullpath=%s\n",
           info.library_name ? info.library_name : "?",
           info.library_version ? info.library_version : "?",
           info.valid_extensions ? info.valid_extensions : "?",
           info.need_fullpath ? "yes" : "no");

    core.set_environment(frontend_environment);
    core.set_video_refresh(frontend_video);
    core.set_audio_sample(frontend_audio_sample);
    core.set_audio_sample_batch(frontend_audio_batch);
    core.set_input_poll(frontend_input_poll);
    core.set_input_state(frontend_input_state);
    core.init();
    core_initialized = true;
    core.set_controller_port_device(0, RETRO_DEVICE_JOYPAD);

    memset(&game, 0, sizeof(game));
    game.path = cfg->game_path;
    if (!core.load_game(&game)) {
        fprintf(stderr, "[LuaPSX] retro_load_game failed for %s\n", cfg->game_path);
        goto cleanup;
    }
    game_loaded = true;

    if (build_save_path(cfg->game_path, g_env.save_dir,
                        save_path, sizeof(save_path)) == 0)
        load_saveram(&core, save_path);

    memset(&av, 0, sizeof(av));
    core.get_system_av_info(&av);
    printf("[LuaPSX] AV: %ux%u max %ux%u, %.3f fps, %.0f Hz\n",
           av.geometry.base_width, av.geometry.base_height,
           av.geometry.max_width, av.geometry.max_height,
           av.timing.fps, av.timing.sample_rate);

    memset(&platform_cfg, 0, sizeof(platform_cfg));
    platform_cfg.width = av.geometry.base_width ? av.geometry.base_width : 320;
    platform_cfg.height = av.geometry.base_height ? av.geometry.base_height : 240;
    platform_cfg.sample_rate = av.timing.sample_rate > 0 ? av.timing.sample_rate : 44100.0;
    platform_cfg.title = info.library_name ? info.library_name : "LuaPSX";

    if (luapsx_platform_init(&platform_cfg) != 0) {
        fprintf(stderr, "[LuaPSX] platform init failed\n");
        goto cleanup;
    }
    platform_initialized = true;
    luapsx_platform_set_pixel_format(g_env.pixel_format);

    double fps = av.timing.fps > 1.0 ? av.timing.fps : 60.0;
    uint64_t frame_us = (uint64_t)(1000000.0 / fps);
    uint64_t next = luapsx_platform_now_us();
    unsigned frames = 0;

    while (!g_env.quit && !luapsx_platform_should_quit()) {
        core.run();
        ++frames;
        if (save_path[0] && (frames % 300u) == 0u) {
            if (save_saveram(&core, save_path) != 0)
                fprintf(stderr, "[LuaPSX] warning: memory-card flush failed: %s\n", save_path);
        }
        if (cfg->max_frames && frames >= cfg->max_frames) break;

        next += frame_us;
        uint64_t now = luapsx_platform_now_us();
        if (next > now) {
            luapsx_platform_sleep_us(next - now);
        } else if (now - next > frame_us * 5) {
            /* Do not accumulate a huge timing debt after a long stall. */
            next = now;
        }
    }

    printf("[LuaPSX] emulator loop ended after %u frames\n", frames);
    rc = 0;

cleanup:
    if (game_loaded && save_path[0]) {
        if (save_saveram(&core, save_path) != 0)
            fprintf(stderr, "[LuaPSX] warning: final memory-card save failed: %s\n", save_path);
        else
            printf("[LuaPSX] memory card saved: %s\n", save_path);
    }
    if (game_loaded) core.unload_game();
    if (core_initialized) core.deinit();
    if (platform_initialized) luapsx_platform_shutdown();
    core_close(&core);
    return rc;
}
