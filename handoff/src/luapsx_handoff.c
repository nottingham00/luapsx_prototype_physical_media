#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef LUAPSX_TITLE_ID
#define LUAPSX_TITLE_ID "LPSX00001"
#endif

#define DISC_PLAYER_TITLE_ID "NPXS40140"
#define DISC_PAYLOAD_PATH "/mnt/disc/LUAPSX/luapsx-ps5.elf"
#define DATA_ROOT "/data/luapsx"
#define DATA_PAYLOAD_PATH DATA_ROOT "/luapsx-ps5.elf"

typedef struct app_launch_ctx {
    uint32_t structsize;
    uint32_t user_id;
    uint32_t app_opt;
    uint64_t crash_report;
    uint32_t check_flag;
} app_launch_ctx_t;

extern int sceUserServiceInitialize(void *);
extern int sceUserServiceTerminate(void);
extern int sceUserServiceGetForegroundUser(uint32_t *user_id);
extern int sceSystemServiceLaunchApp(const char *title_id, const char **argv,
                                     app_launch_ctx_t *ctx);
extern int sceLncUtilGetAppIdOfRunningBigApp(void);
extern int sceLncUtilGetAppTitleId(uint32_t app_id, char *title_id);
extern int sceLncUtilSuspendApp(uint32_t app_id);
extern int sceLncUtilKillApp(uint32_t app_id);
extern int sceShellCoreUtilNavigateToGoHome(void);

static int ensure_dir(const char *path) {
    if (mkdir(path, 0755) == 0 || errno == EEXIST) return 0;
    perror(path);
    return -1;
}

static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }

    unsigned char buf[64 * 1024];
    size_t n;
    int rc = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) != 0) {
        if (fwrite(buf, 1, n, out) != n) {
            rc = -1;
            break;
        }
    }
    if (ferror(in)) rc = -1;
    fclose(out);
    fclose(in);
    return rc;
}

static void stage_luapsx_payload(void) {
    struct stat st;
    if (stat(DISC_PAYLOAD_PATH, &st) != 0 || !S_ISREG(st.st_mode)) {
        printf("[LuaPSX/handoff] payload not embedded on disc: %s\n",
               DISC_PAYLOAD_PATH);
        return;
    }
    if (ensure_dir(DATA_ROOT) != 0) return;
    if (copy_file(DISC_PAYLOAD_PATH, DATA_PAYLOAD_PATH) == 0) {
        chmod(DATA_PAYLOAD_PATH, 0755);
        printf("[LuaPSX/handoff] staged %s\n", DATA_PAYLOAD_PATH);
    } else {
        printf("[LuaPSX/handoff] warning: failed to stage emulator payload\n");
    }
}

static void close_disc_player_if_running(void) {
    int app_id = sceLncUtilGetAppIdOfRunningBigApp();
    if (app_id < 0) return;

    char title_id[16];
    memset(title_id, 0, sizeof(title_id));
    if (sceLncUtilGetAppTitleId((uint32_t)app_id, title_id) != 0) return;
    if (strcmp(title_id, DISC_PLAYER_TITLE_ID) != 0) return;

    printf("[LuaPSX/handoff] closing Disc Player (%s)\n", title_id);
    (void)sceLncUtilSuspendApp((uint32_t)app_id);
    sleep(1);
    (void)sceLncUtilKillApp((uint32_t)app_id);
    sleep(1);
}

int main(void) {
    uint32_t user_id = 0;
    const char *argv[] = { NULL };
    app_launch_ctx_t ctx;
    int rc;

    printf("[LuaPSX/handoff] disc -> Games handoff starting\n");
    stage_luapsx_payload();
    close_disc_player_if_running();

    memset(&ctx, 0, sizeof(ctx));
    ctx.structsize = sizeof(ctx);

    if (sceUserServiceInitialize(NULL) == 0) {
        (void)sceUserServiceGetForegroundUser(&user_id);
        ctx.user_id = user_id;
    }

    printf("[LuaPSX/handoff] launching Games title %s for user 0x%08x\n",
           LUAPSX_TITLE_ID, user_id);
    rc = sceSystemServiceLaunchApp(LUAPSX_TITLE_ID, argv, &ctx);
    if (rc != 0) {
        printf("[LuaPSX/handoff] launch failed: 0x%08x; returning Home\n",
               (unsigned)rc);
        (void)sceShellCoreUtilNavigateToGoHome();
    }

    (void)sceUserServiceTerminate();
    return rc;
}
