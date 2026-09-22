#include "luapsx_libretro.h"
#include "psx_cache.h"
#include "psx_disc.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifndef LUAPSX_COPY_TO_DATA
#define LUAPSX_COPY_TO_DATA 1
#endif
#ifndef LUAPSX_RUN_EMULATOR
#define LUAPSX_RUN_EMULATOR 1
#endif
#ifndef LUAPSX_CORE_PATH
#define LUAPSX_CORE_PATH "/data/luapsx/cores/pcsx_rearmed_libretro.so"
#endif

#define DISC_PSX_DIR   "/mnt/disc/PSX"
#define DISC_APP_DIR   "/mnt/disc/LUAPSX"
#define DISC_CORE_DIR  DISC_APP_DIR "/cores"
#define DISC_SYSTEM_DIR DISC_APP_DIR "/system"
#define CACHE_ROOT     "/data/luapsx"
#define CACHE_PSX_DIR  CACHE_ROOT "/PSX"
#define CORE_DIR       CACHE_ROOT "/cores"
#define SYSTEM_DIR     CACHE_ROOT "/system"
#define SAVE_DIR       CACHE_ROOT "/saves"

/*
 * PS5 payload entry point.
 *
 * No exploit is included. This payload expects an existing homebrew/ELF
 * execution environment. BD-J is responsible for boot/discovery; this native
 * stage copies or streams PSX content, validates the image, and starts a
 * libretro PS1 core such as PCSX-ReARMed.
 */

static int file_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int dir_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void stage_runtime_from_disc(void) {
    char error[512];

    if (dir_exists(DISC_CORE_DIR)) {
        printf("[LuaPSX] staging emulator cores from %s\n", DISC_CORE_DIR);
        if (psx_cache_copy_tree(DISC_CORE_DIR, CORE_DIR, error, sizeof(error)) != 0)
            printf("[LuaPSX] warning: core staging failed: %s\n", error);
    }

    if (dir_exists(DISC_SYSTEM_DIR)) {
        printf("[LuaPSX] staging system files from %s\n", DISC_SYSTEM_DIR);
        if (psx_cache_copy_tree(DISC_SYSTEM_DIR, SYSTEM_DIR, error, sizeof(error)) != 0)
            printf("[LuaPSX] warning: system staging failed: %s\n", error);
    }
}

static void prepare_runtime_dirs(void) {
    char error[256];
    const char *dirs[] = {CACHE_ROOT, CORE_DIR, SYSTEM_DIR, SAVE_DIR};
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
        if (psx_cache_ensure_dir(dirs[i], error, sizeof(error)) != 0)
            printf("[LuaPSX] warning: %s\n", error);
    }
}

static int locate_and_validate_game(char *cue, size_t cue_size) {
    const char *source_dir = DISC_PSX_DIR;
    psx_disc_t disc;

    prepare_runtime_dirs();
    stage_runtime_from_disc();

#if LUAPSX_COPY_TO_DATA
    char copy_error[512];
    printf("[LuaPSX] copying %s -> %s\n", DISC_PSX_DIR, CACHE_PSX_DIR);
    if (psx_cache_copy_tree(DISC_PSX_DIR, CACHE_PSX_DIR,
                            copy_error, sizeof(copy_error)) == 0) {
        printf("[LuaPSX] cache copy complete\n");
        source_dir = CACHE_PSX_DIR;
    } else {
        printf("[LuaPSX] cache copy failed: %s\n", copy_error);
        printf("[LuaPSX] falling back to direct Blu-ray streaming\n");
    }
#endif

    if (psx_find_first_cue(source_dir, cue, cue_size) != 0) {
        printf("[LuaPSX] no CUE found in %s\n", source_dir);
        return -1;
    }

    printf("[LuaPSX] validating %s\n", cue);
    if (psx_disc_open(&disc, cue) != 0) {
        printf("[LuaPSX] CUE/BIN validation failed\n");
        return -1;
    }

    printf("[LuaPSX] image OK: %zu tracks, %zu files, %llu raw sectors\n",
           disc.track_count, disc.file_count,
           (unsigned long long)disc.total_sectors);
    psx_disc_close(&disc);
    return 0;
}

static int smoke_test_game(const char *cue) {
    psx_disc_t disc;
    uint8_t sector[PSX_RAW_SECTOR_SIZE];
    if (psx_disc_open(&disc, cue) != 0) return -1;
    if (psx_disc_read_raw2352(&disc, 0, sector) != 0) {
        printf("[LuaPSX] first sector read failed\n");
        psx_disc_close(&disc);
        return -1;
    }
    printf("[LuaPSX] first sector: %02x %02x %02x %02x ...\n",
           sector[0], sector[1], sector[2], sector[3]);
    psx_disc_close(&disc);
    return 0;
}

int payload_main(void *args) {
    (void)args;
    char cue[PSX_PATH_MAX];

    if (locate_and_validate_game(cue, sizeof(cue)) != 0) return -1;

#if LUAPSX_RUN_EMULATOR
    if (file_exists(LUAPSX_CORE_PATH)) {
        luapsx_frontend_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.core_path = LUAPSX_CORE_PATH;
        cfg.game_path = cue;
        cfg.system_dir = SYSTEM_DIR;
        cfg.save_dir = SAVE_DIR;
        cfg.max_frames = 0;

        printf("[LuaPSX] launching core %s\n", cfg.core_path);
        printf("[LuaPSX] content %s\n", cfg.game_path);
        return luapsx_run_libretro(&cfg);
    }

    printf("[LuaPSX] emulator core not installed: %s\n", LUAPSX_CORE_PATH);
    printf("[LuaPSX] disc path is ready; running sector smoke test instead\n");
#endif

    return smoke_test_game(cue);
}
