#include "psx_disc.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <strings.h>
#endif

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

static void trim(char *s) {
    char *start = s;
    char *end;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) --end;
    *end = '\0';
}

static int has_suffix_ci(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    if (a < b) return 0;
    return strcasecmp(s + a - b, suffix) == 0;
}

static int is_abs_path(const char *p) {
    if (!p || !*p) return 0;
#ifdef _WIN32
    if (isalpha((unsigned char)p[0]) && p[1] == ':') return 1;
#endif
    return p[0] == '/' || p[0] == '\\';
}

static void dirname_of(const char *path, char *out, size_t n) {
    const char *a = strrchr(path, '/');
    const char *b = strrchr(path, '\\');
    const char *slash = a > b ? a : b;
    if (!slash) {
        snprintf(out, n, ".");
        return;
    }
    size_t len = (size_t)(slash - path);
    if (len == 0) len = 1;
    if (len >= n) len = n - 1;
    memcpy(out, path, len);
    out[len] = '\0';
}

static int join_path(char *out, size_t n, const char *dir, const char *leaf) {
    if (is_abs_path(leaf)) {
        return snprintf(out, n, "%s", leaf) < (int)n ? 0 : -1;
    }
    if (!dir || !*dir || !strcmp(dir, ".")) {
        return snprintf(out, n, "%s", leaf) < (int)n ? 0 : -1;
    }
    size_t dl = strlen(dir);
    char sep = (dl && (dir[dl - 1] == '/' || dir[dl - 1] == '\\')) ? '\0' : '/';
    int r = sep ? snprintf(out, n, "%s%c%s", dir, sep, leaf)
                : snprintf(out, n, "%s%s", dir, leaf);
    return r >= 0 && r < (int)n ? 0 : -1;
}

static int parse_msf(const char *s) {
    int m, sec, f;
    if (sscanf(s, "%d:%d:%d", &m, &sec, &f) != 3) return -1;
    if (m < 0 || sec < 0 || sec >= 60 || f < 0 || f >= 75) return -1;
    return ((m * 60) + sec) * 75 + f;
}

static psx_track_mode_t parse_mode(const char *s) {
    if (!strcasecmp(s, "AUDIO")) return PSX_TRACK_AUDIO;
    if (!strcasecmp(s, "MODE1/2352")) return PSX_TRACK_MODE1_2352;
    if (!strcasecmp(s, "MODE2/2352")) return PSX_TRACK_MODE2_2352;
    return PSX_TRACK_UNKNOWN;
}

const char *psx_track_mode_name(psx_track_mode_t mode) {
    switch (mode) {
        case PSX_TRACK_AUDIO: return "AUDIO";
        case PSX_TRACK_MODE1_2352: return "MODE1/2352";
        case PSX_TRACK_MODE2_2352: return "MODE2/2352";
        default: return "UNKNOWN";
    }
}

static int file_size(const char *path, uint64_t *size) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    if (st.st_size < 0) return -1;
    *size = (uint64_t)st.st_size;
    return 0;
}

static int add_file(psx_disc_t *d, const char *cue_name) {
    if (d->file_count >= PSX_MAX_FILES) return -1;
    psx_cue_file_t *f = &d->files[d->file_count];
    memset(f, 0, sizeof(*f));
    if (join_path(f->path, sizeof(f->path), d->cue_dir, cue_name) != 0) return -1;
    if (file_size(f->path, &f->size) != 0) return -1;
    if (f->size % PSX_RAW_SECTOR_SIZE != 0) return -1;
    f->sector_count = f->size / PSX_RAW_SECTOR_SIZE;
    d->file_count++;
    return (int)d->file_count - 1;
}

static int parse_quoted_filename(const char *line, char *out, size_t n) {
    const char *p = line;
    while (*p && !isspace((unsigned char)*p)) ++p;
    while (*p && isspace((unsigned char)*p)) ++p;
    if (*p == '"') {
        const char *end = strchr(++p, '"');
        if (!end) return -1;
        size_t len = (size_t)(end - p);
        if (len >= n) return -1;
        memcpy(out, p, len);
        out[len] = '\0';
        return 0;
    }
    const char *end = p;
    while (*end && !isspace((unsigned char)*end)) ++end;
    size_t len = (size_t)(end - p);
    if (!len || len >= n) return -1;
    memcpy(out, p, len);
    out[len] = '\0';
    return 0;
}

static int open_files(psx_disc_t *d) {
    uint64_t next_lba = 0;
    for (size_t i = 0; i < d->file_count; ++i) {
        d->files[i].first_lba = next_lba;
        next_lba += d->files[i].sector_count;
        d->files[i].fp = fopen(d->files[i].path, "rb");
        if (!d->files[i].fp) return -1;
    }
    d->total_sectors = next_lba;
    return 0;
}

static void compute_track_lbas(psx_disc_t *d) {
    for (size_t i = 0; i < d->track_count; ++i) {
        psx_track_t *t = &d->tracks[i];
        if (t->file_index >= 0 && (size_t)t->file_index < d->file_count) {
            uint64_t base = d->files[t->file_index].first_lba;
            int index = t->index01_frames >= 0 ? t->index01_frames : 0;
            t->absolute_lba = base + (uint64_t)index;
        }
    }
}

int psx_disc_open(psx_disc_t *d, const char *cue_path) {
    FILE *cue = NULL;
    char line[2048];
    int current_file = -1;
    psx_track_t *current_track = NULL;

    if (!d || !cue_path) return -1;
    memset(d, 0, sizeof(*d));
    for (size_t i = 0; i < PSX_MAX_TRACKS; ++i) {
        d->tracks[i].file_index = -1;
        d->tracks[i].index00_frames = -1;
        d->tracks[i].index01_frames = -1;
    }

    if (snprintf(d->cue_path, sizeof(d->cue_path), "%s", cue_path) >= (int)sizeof(d->cue_path)) return -1;
    dirname_of(cue_path, d->cue_dir, sizeof(d->cue_dir));

    cue = fopen(cue_path, "rb");
    if (!cue) return -1;

    while (fgets(line, sizeof(line), cue)) {
        trim(line);
        if (!line[0] || !strncasecmp(line, "REM ", 4)) continue;

        if (!strncasecmp(line, "FILE ", 5)) {
            char name[PSX_PATH_MAX];
            if (parse_quoted_filename(line, name, sizeof(name)) != 0) goto fail;
            current_file = add_file(d, name);
            if (current_file < 0) goto fail;
            current_track = NULL;
            continue;
        }

        if (!strncasecmp(line, "TRACK ", 6)) {
            int num;
            char mode[64];
            if (current_file < 0 || d->track_count >= PSX_MAX_TRACKS) goto fail;
            if (sscanf(line + 6, "%d %63s", &num, mode) != 2) goto fail;
            current_track = &d->tracks[d->track_count++];
            memset(current_track, 0, sizeof(*current_track));
            current_track->number = num;
            current_track->mode = parse_mode(mode);
            current_track->file_index = current_file;
            current_track->index00_frames = -1;
            current_track->index01_frames = -1;
            continue;
        }

        if (!strncasecmp(line, "INDEX ", 6) && current_track) {
            int idx;
            char msf[32];
            int frames;
            if (sscanf(line + 6, "%d %31s", &idx, msf) != 2) goto fail;
            frames = parse_msf(msf);
            if (frames < 0) goto fail;
            if (idx == 0) current_track->index00_frames = frames;
            if (idx == 1) current_track->index01_frames = frames;
            continue;
        }

        if (!strncasecmp(line, "PREGAP ", 7) && current_track) {
            int frames = parse_msf(line + 7);
            if (frames < 0) goto fail;
            current_track->pregap_frames = frames;
            continue;
        }
    }

    fclose(cue);
    cue = NULL;

    if (d->file_count == 0 || d->track_count == 0) goto fail;
    if (open_files(d) != 0) goto fail;
    compute_track_lbas(d);
    return 0;

fail:
    if (cue) fclose(cue);
    psx_disc_close(d);
    return -1;
}

void psx_disc_close(psx_disc_t *d) {
    if (!d) return;
    for (size_t i = 0; i < d->file_count; ++i) {
        if (d->files[i].fp) fclose(d->files[i].fp);
        d->files[i].fp = NULL;
    }
}

static psx_cue_file_t *file_for_lba(psx_disc_t *d, uint64_t lba, uint64_t *local_lba) {
    for (size_t i = 0; i < d->file_count; ++i) {
        psx_cue_file_t *f = &d->files[i];
        if (lba >= f->first_lba && lba < f->first_lba + f->sector_count) {
            *local_lba = lba - f->first_lba;
            return f;
        }
    }
    return NULL;
}

int psx_disc_read_raw2352(psx_disc_t *d, uint64_t lba,
                          uint8_t out[PSX_RAW_SECTOR_SIZE]) {
    uint64_t local_lba;
    psx_cue_file_t *f;
    if (!d || !out || lba >= d->total_sectors) return -1;
    f = file_for_lba(d, lba, &local_lba);
    if (!f || !f->fp) return -1;
    uint64_t off = local_lba * (uint64_t)PSX_RAW_SECTOR_SIZE;
#if defined(_WIN32)
    if (_fseeki64(f->fp, (__int64)off, SEEK_SET) != 0) return -1;
#else
    if (fseeko(f->fp, (off_t)off, SEEK_SET) != 0) return -1;
#endif
    return fread(out, 1, PSX_RAW_SECTOR_SIZE, f->fp) == PSX_RAW_SECTOR_SIZE ? 0 : -1;
}

const psx_track_t *psx_disc_track_for_lba(const psx_disc_t *d, uint64_t lba) {
    const psx_track_t *best = NULL;
    if (!d) return NULL;
    for (size_t i = 0; i < d->track_count; ++i) {
        if (d->tracks[i].absolute_lba <= lba) best = &d->tracks[i];
        else break;
    }
    return best;
}

int psx_disc_read_mode1_2048(psx_disc_t *d, uint64_t lba,
                             uint8_t out[PSX_MODE1_DATA_SIZE]) {
    uint8_t raw[PSX_RAW_SECTOR_SIZE];
    const psx_track_t *track = psx_disc_track_for_lba(d, lba);
    if (!track || track->mode != PSX_TRACK_MODE1_2352) return -1;
    if (psx_disc_read_raw2352(d, lba, raw) != 0) return -1;

    /* Mode 1 raw sector: 12 sync + 3 address + 1 mode + 2048 data + EDC/ECC. */
    if (raw[15] != 0x01) return -1;
    memcpy(out, raw + 16, PSX_MODE1_DATA_SIZE);
    return 0;
}

int psx_find_first_cue(const char *directory, char *out, size_t out_size) {
    DIR *dir;
    struct dirent *de;
    if (!directory || !out || out_size == 0) return -1;
    dir = opendir(directory);
    if (!dir) return -1;
    while ((de = readdir(dir)) != NULL) {
        if (de->d_name[0] == '.') continue;
        if (!has_suffix_ci(de->d_name, ".cue")) continue;
        int r = snprintf(out, out_size, "%s/%s", directory, de->d_name);
        closedir(dir);
        return r >= 0 && r < (int)out_size ? 0 : -1;
    }
    closedir(dir);
    return -1;
}
