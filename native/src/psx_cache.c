#include "psx_cache.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

#define CACHE_PATH_MAX 1024
#define COPY_BUFFER (256 * 1024)

static void set_error(char *out, size_t n, const char *fmt, const char *path) {
    if (!out || n == 0) return;
    snprintf(out, n, fmt, path, strerror(errno));
}

static int ensure_dir_one(const char *path, char *err, size_t errn) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 0;
        errno = ENOTDIR;
        set_error(err, errn, "%s exists but is not a directory: %s", path);
        return -1;
    }
    if (mkdir(path, 0777) == 0 || errno == EEXIST) return 0;
    set_error(err, errn, "mkdir %s failed: %s", path);
    return -1;
}

int psx_cache_ensure_dir(const char *path, char *err, size_t errn) {
    char tmp[CACHE_PATH_MAX];
    size_t len;

    if (!path || !*path) {
        errno = EINVAL;
        if (err && errn) snprintf(err, errn, "invalid directory path");
        return -1;
    }
    if (snprintf(tmp, sizeof(tmp), "%s", path) >= (int)sizeof(tmp)) {
        errno = ENAMETOOLONG;
        if (err && errn) snprintf(err, errn, "directory path too long");
        return -1;
    }

    len = strlen(tmp);
    while (len > 1 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\'))
        tmp[--len] = '\0';

    for (char *p = tmp + 1; *p; ++p) {
        if (*p != '/' && *p != '\\') continue;
        char saved = *p;
        *p = '\0';
        if (*tmp && ensure_dir_one(tmp, err, errn) != 0) return -1;
        *p = saved;
    }
    return ensure_dir_one(tmp, err, errn);
}

static int join_path(char *out, size_t n, const char *a, const char *b) {
    size_t alen = strlen(a);
    int r = (alen && a[alen - 1] == '/')
        ? snprintf(out, n, "%s%s", a, b)
        : snprintf(out, n, "%s/%s", a, b);
    return r >= 0 && r < (int)n ? 0 : -1;
}

static int copy_file(const char *src, const char *dst, char *err, size_t errn) {
    FILE *in = NULL, *out = NULL;
    unsigned char buf[COPY_BUFFER];
    size_t got;

    in = fopen(src, "rb");
    if (!in) {
        set_error(err, errn, "open %s failed: %s", src);
        return -1;
    }
    out = fopen(dst, "wb");
    if (!out) {
        set_error(err, errn, "create %s failed: %s", dst);
        fclose(in);
        return -1;
    }

    while ((got = fread(buf, 1, sizeof(buf), in)) != 0) {
        if (fwrite(buf, 1, got, out) != got) {
            set_error(err, errn, "write %s failed: %s", dst);
            fclose(out);
            fclose(in);
            return -1;
        }
    }
    if (ferror(in)) {
        set_error(err, errn, "read %s failed: %s", src);
        fclose(out);
        fclose(in);
        return -1;
    }

    if (fflush(out) != 0) {
        set_error(err, errn, "flush %s failed: %s", dst);
        fclose(out);
        fclose(in);
        return -1;
    }

    fclose(out);
    fclose(in);
    return 0;
}

static int copy_tree_inner(const char *src_dir, const char *dst_dir,
                           char *err, size_t errn) {
    DIR *dir;
    struct dirent *de;

    if (psx_cache_ensure_dir(dst_dir, err, errn) != 0) return -1;
    dir = opendir(src_dir);
    if (!dir) {
        set_error(err, errn, "opendir %s failed: %s", src_dir);
        return -1;
    }

    while ((de = readdir(dir)) != NULL) {
        char src[CACHE_PATH_MAX], dst[CACHE_PATH_MAX];
        struct stat st;
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        if (join_path(src, sizeof(src), src_dir, de->d_name) != 0 ||
            join_path(dst, sizeof(dst), dst_dir, de->d_name) != 0) {
            errno = ENAMETOOLONG;
            set_error(err, errn, "path too long near %s: %s", de->d_name);
            closedir(dir);
            return -1;
        }
        if (stat(src, &st) != 0) {
            set_error(err, errn, "stat %s failed: %s", src);
            closedir(dir);
            return -1;
        }
        if (S_ISDIR(st.st_mode)) {
            if (copy_tree_inner(src, dst, err, errn) != 0) {
                closedir(dir);
                return -1;
            }
        } else if (S_ISREG(st.st_mode)) {
            if (copy_file(src, dst, err, errn) != 0) {
                closedir(dir);
                return -1;
            }
        }
    }

    closedir(dir);
    return 0;
}

int psx_cache_copy_tree(const char *src_dir, const char *dst_dir,
                        char *error_text, size_t error_text_size) {
    if (!src_dir || !dst_dir) return -1;
    if (error_text && error_text_size) error_text[0] = '\0';
    return copy_tree_inner(src_dir, dst_dir, error_text, error_text_size);
}
