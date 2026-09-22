#ifndef LUAPSX_PSX_DISC_H
#define LUAPSX_PSX_DISC_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PSX_RAW_SECTOR_SIZE 2352u
#define PSX_MODE1_DATA_SIZE 2048u
#define PSX_MAX_TRACKS 99
#define PSX_MAX_FILES 16
#define PSX_PATH_MAX 1024

typedef enum psx_track_mode {
    PSX_TRACK_UNKNOWN = 0,
    PSX_TRACK_AUDIO,
    PSX_TRACK_MODE1_2352,
    PSX_TRACK_MODE2_2352
} psx_track_mode_t;

typedef struct psx_cue_file {
    char path[PSX_PATH_MAX];
    uint64_t size;
    uint64_t first_lba;
    uint64_t sector_count;
    FILE *fp;
} psx_cue_file_t;

typedef struct psx_track {
    int number;
    psx_track_mode_t mode;
    int file_index;
    int index00_frames;
    int index01_frames;
    int pregap_frames;
    uint64_t absolute_lba;
} psx_track_t;

typedef struct psx_disc {
    char cue_path[PSX_PATH_MAX];
    char cue_dir[PSX_PATH_MAX];
    psx_cue_file_t files[PSX_MAX_FILES];
    size_t file_count;
    psx_track_t tracks[PSX_MAX_TRACKS];
    size_t track_count;
    uint64_t total_sectors;
} psx_disc_t;

int psx_disc_open(psx_disc_t *disc, const char *cue_path);
void psx_disc_close(psx_disc_t *disc);

int psx_disc_read_raw2352(psx_disc_t *disc, uint64_t lba,
                          uint8_t out[PSX_RAW_SECTOR_SIZE]);
int psx_disc_read_mode1_2048(psx_disc_t *disc, uint64_t lba,
                             uint8_t out[PSX_MODE1_DATA_SIZE]);

const psx_track_t *psx_disc_track_for_lba(const psx_disc_t *disc, uint64_t lba);
const char *psx_track_mode_name(psx_track_mode_t mode);

/* Utility used by the PS5 payload to locate the first CUE in a directory. */
int psx_find_first_cue(const char *directory, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif
