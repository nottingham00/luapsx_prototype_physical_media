#include "psx_cache.h"
#include <stdio.h>

int main(int argc, char **argv) {
    char err[512];
    if (argc != 3) {
        fprintf(stderr, "usage: %s SRC_DIR DST_DIR\n", argv[0]);
        return 2;
    }
    if (psx_cache_copy_tree(argv[1], argv[2], err, sizeof(err)) != 0) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    puts("copy OK");
    return 0;
}
