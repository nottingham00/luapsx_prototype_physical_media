#include "elfldr_client.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

int luapsx_send_elf_to_loader(const char *elf_path, const char *host, int port) {
    int fd = -1;
    int sock = -1;
    int rc = -1;
    struct stat st;
    struct sockaddr_in addr;
    unsigned char buf[64 * 1024];

    if (!elf_path || !host || port <= 0 || port > 65535) return -1;
    fd = open(elf_path, O_RDONLY);
    if (fd < 0) {
        printf("[LuaPSX/title] open %s failed: %s\n", elf_path, strerror(errno));
        goto done;
    }
    if (fstat(fd, &st) != 0 || st.st_size <= 0) {
        printf("[LuaPSX/title] invalid ELF: %s\n", elf_path);
        goto done;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) goto done;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = inet_addr(host);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        printf("[LuaPSX/title] elfldr %s:%d unavailable: %s\n",
               host, port, strerror(errno));
        goto done;
    }

    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n == 0) break;
        if (n < 0) goto done;
        size_t off = 0;
        while (off < (size_t)n) {
            ssize_t sent = send(sock, buf + off, (size_t)n - off, 0);
            if (sent <= 0) goto done;
            off += (size_t)sent;
        }
    }

    printf("[LuaPSX/title] sent %lld bytes to elfldr\n",
           (long long)st.st_size);
    rc = 0;

done:
    if (sock >= 0) close(sock);
    if (fd >= 0) close(fd);
    return rc;
}
