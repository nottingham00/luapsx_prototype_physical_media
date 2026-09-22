#ifndef LUAPSX_PSX_CACHE_H
#define LUAPSX_PSX_CACHE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Create a directory and any missing parents. */
int psx_cache_ensure_dir(const char *path,
                         char *error_text, size_t error_text_size);

/* Recursively copy a directory tree. Existing files are overwritten. */
int psx_cache_copy_tree(const char *src_dir, const char *dst_dir,
                        char *error_text, size_t error_text_size);

#ifdef __cplusplus
}
#endif

#endif
