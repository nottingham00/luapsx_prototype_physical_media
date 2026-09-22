#ifndef LUAPSX_ELFLDR_CLIENT_H
#define LUAPSX_ELFLDR_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

int luapsx_send_elf_to_loader(const char *elf_path, const char *host, int port);

#ifdef __cplusplus
}
#endif
#endif
