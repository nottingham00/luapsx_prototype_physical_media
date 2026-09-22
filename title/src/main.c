#include "elfldr_client.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef LUAPSX_ELF_PATH
#define LUAPSX_ELF_PATH "/data/luapsx/luapsx-ps5.elf"
#endif
#ifndef LUAPSX_DISC_ELF_PATH
#define LUAPSX_DISC_ELF_PATH "/mnt/disc/LUAPSX/luapsx-ps5.elf"
#endif
#ifndef LUAPSX_ELFLDR_PORT
#define LUAPSX_ELFLDR_PORT 9021
#endif

static int file_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/*
 * Real-title entry point for the permanent Games-area LuaPSX tile.
 *
 * The title is deliberately tiny. The BD-J handoff launches this title after
 * Disc Player closes. The title then forwards the staged LuaPSX emulator ELF
 * to the already-running local elfldr. That keeps the Games tile permanent
 * while the emulator itself can continue using the ps5-payload-sdk + SDL2
 * build during development.
 */
int main(void) {
    const char *payload = LUAPSX_ELF_PATH;

    printf("[LuaPSX/title] LuaPSX Games tile started\n");
    printf("[LuaPSX/title] disc content: /mnt/disc/PSX\n");

    if (!file_exists(payload)) {
        if (file_exists(LUAPSX_DISC_ELF_PATH)) {
            payload = LUAPSX_DISC_ELF_PATH;
            printf("[LuaPSX/title] using emulator ELF directly from disc\n");
        } else {
            printf("[LuaPSX/title] no emulator ELF found\n");
            return 2;
        }
    }

    /* Give ShellUI / the disc handoff a short moment to finish its transition. */
    usleep(250000);

    if (luapsx_send_elf_to_loader(payload, "127.0.0.1", LUAPSX_ELFLDR_PORT) != 0) {
        printf("[LuaPSX/title] failed to start emulator; is elfldr running?\n");
        return 3;
    }

    /* elfldr may replace/terminate this title once it has received the ELF. */
    sleep(1);
    return 0;
}
