#include "elfldr_client.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_PORT 19021
#define TEST_SIZE 196733

static volatile size_t g_received;

static void *server_thread(void *unused) {
    (void)unused;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(TEST_PORT);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(s, (struct sockaddr *)&a, sizeof(a)) != 0) return NULL;
    if (listen(s, 1) != 0) return NULL;
    int c = accept(s, NULL, NULL);
    if (c >= 0) {
        unsigned char b[8192];
        ssize_t n;
        while ((n = recv(c, b, sizeof(b), 0)) > 0) g_received += (size_t)n;
        close(c);
    }
    close(s);
    return NULL;
}

int main(void) {
    const char *path = "tests/test_payload.elf";
    FILE *f = fopen(path, "wb");
    if (!f) return 1;
    for (size_t i = 0; i < TEST_SIZE; ++i) fputc((int)(i & 0xff), f);
    fclose(f);

    pthread_t t;
    if (pthread_create(&t, NULL, server_thread, NULL) != 0) return 2;
    usleep(100000);

    int rc = luapsx_send_elf_to_loader(path, "127.0.0.1", TEST_PORT);
    pthread_join(t, NULL);
    unlink(path);

    if (rc != 0 || g_received != TEST_SIZE) {
        fprintf(stderr, "elfldr client test failed: rc=%d bytes=%zu\n", rc, g_received);
        return 3;
    }
    printf("[LuaPSX/test] elfldr client: OK (%zu bytes)\n", g_received);
    return 0;
}
