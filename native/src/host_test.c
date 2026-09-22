#include "psx_disc.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static void hex16(const uint8_t *p) {
    for (int i = 0; i < 16; ++i) printf("%02x%s", p[i], i == 15 ? "" : " ");
    putchar('\n');
}

int main(int argc, char **argv) {
    psx_disc_t disc;
    uint8_t raw[PSX_RAW_SECTOR_SIZE];
    uint8_t data[PSX_MODE1_DATA_SIZE];

    if (argc != 2) {
        fprintf(stderr, "usage: %s GAME.CUE\n", argv[0]);
        return 2;
    }

    if (psx_disc_open(&disc, argv[1]) != 0) {
        fprintf(stderr, "failed to open cue: %s\n", argv[1]);
        return 1;
    }

    printf("CUE: %s\n", disc.cue_path);
    printf("files: %zu, tracks: %zu, sectors: %" PRIu64 "\n",
           disc.file_count, disc.track_count, disc.total_sectors);

    for (size_t i = 0; i < disc.file_count; ++i) {
        printf("file[%zu] %s size=%" PRIu64 " sectors=%" PRIu64 " first_lba=%" PRIu64 "\n",
               i, disc.files[i].path, disc.files[i].size,
               disc.files[i].sector_count, disc.files[i].first_lba);
    }
    for (size_t i = 0; i < disc.track_count; ++i) {
        const psx_track_t *t = &disc.tracks[i];
        printf("track %02d %-10s file=%d index01=%d lba=%" PRIu64 "\n",
               t->number, psx_track_mode_name(t->mode), t->file_index,
               t->index01_frames, t->absolute_lba);
    }

    if (psx_disc_read_raw2352(&disc, 0, raw) != 0) {
        fprintf(stderr, "raw sector read failed\n");
        psx_disc_close(&disc);
        return 1;
    }
    printf("raw sector 0 first 16 bytes: ");
    hex16(raw);

    if (psx_disc_read_mode1_2048(&disc, 0, data) == 0) {
        printf("mode1 sector 0 user data first 16 bytes: ");
        hex16(data);
    } else {
        printf("sector 0 is not MODE1/2352 user data\n");
    }

    if (disc.total_sectors > 5 && psx_disc_read_raw2352(&disc, 5, raw) == 0) {
        printf("random seek sector 5 first 16 bytes: ");
        hex16(raw);
    }

    psx_disc_close(&disc);
    return 0;
}
