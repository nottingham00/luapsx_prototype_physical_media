#ifndef LUAPSX_LIBRETRO_H
#define LUAPSX_LIBRETRO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct luapsx_frontend_config {
    const char *core_path;
    const char *game_path;
    const char *system_dir;
    const char *save_dir;
    unsigned max_frames; /* 0 = until user exits */
    int headless;
} luapsx_frontend_config_t;

int luapsx_run_libretro(const luapsx_frontend_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif
