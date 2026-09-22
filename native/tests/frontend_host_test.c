#include "luapsx_libretro.h"
#include <stdio.h>
#include <sys/stat.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <core.so> <game.cue>\n", argv[0]);
        return 2;
    }
    luapsx_frontend_config_t cfg = {
        .core_path = argv[1],
        .game_path = argv[2],
        .system_dir = "./system",
        .save_dir = "./saves",
        .max_frames = 6,
        .headless = 1
    };
    if (luapsx_run_libretro(&cfg) != 0) return 1;

    struct stat st;
    if (stat("./saves/GAME.srm", &st) != 0 || st.st_size != 128 * 1024) {
        fprintf(stderr, "save RAM test failed\n");
        return 1;
    }
    printf("[LuaPSX/test] save RAM: OK (%lld bytes)\n", (long long)st.st_size);
    return 0;
}
