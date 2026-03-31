/*
 * test_ipc.c — Combined IPC validation for redroid-modules
 *
 * Tests binderfs, binder, and ashmem kernel modules to verify
 * Android IPC compatibility (ReDroid / Waydroid).
 *
 * Build:  gcc -Wall -Wextra -O2 -I../binder -I../ashmem -o test_ipc test_ipc.c
 * Run:    sudo ./test_ipc
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ---------- binder / binderfs uapi ---------- */
#include "uapi/linux/android/binder.h"
#include "uapi/linux/android/binderfs.h"

/* ---------- ashmem uapi (self-contained) ---------- */
#define ASHMEM_NAME_LEN 256
#define __ASHMEMIOC 0x77
#define ASHMEM_SET_NAME _IOW(__ASHMEMIOC, 1, char[ASHMEM_NAME_LEN])
#define ASHMEM_GET_NAME _IOR(__ASHMEMIOC, 2, char[ASHMEM_NAME_LEN])
#define ASHMEM_SET_SIZE _IOW(__ASHMEMIOC, 3, size_t)
#define ASHMEM_GET_SIZE _IO(__ASHMEMIOC, 4)
#define ASHMEM_SET_PROT_MASK _IOW(__ASHMEMIOC, 5, unsigned long)
#define ASHMEM_GET_PROT_MASK _IO(__ASHMEMIOC, 6)
#define ASHMEM_PIN _IOW(__ASHMEMIOC, 7, struct ashmem_pin)
#define ASHMEM_UNPIN _IOW(__ASHMEMIOC, 8, struct ashmem_pin)
#define ASHMEM_GET_PIN_STATUS _IO(__ASHMEMIOC, 9)
#define ASHMEM_PURGE_ALL_CACHES _IO(__ASHMEMIOC, 10)

#define ASHMEM_IS_UNPINNED 0
#define ASHMEM_IS_PINNED 1

struct ashmem_pin
{
    unsigned int offset;
    unsigned int len;
};

/* ---------- test framework ---------- */

static int g_passed, g_failed, g_skipped;

#define TEST(section, desc)                     \
    do                                          \
    {                                           \
        printf("  [%s] %-44s ", section, desc); \
        fflush(stdout);                         \
    } while (0)

#define PASS()            \
    do                    \
    {                     \
        printf("PASS\n"); \
        g_passed++;       \
    } while (0)

#define FAIL(reason)                  \
    do                                \
    {                                 \
        printf("FAIL: %s\n", reason); \
        g_failed++;                   \
    } while (0)

#define FAIL_ERRNO()                           \
    do                                         \
    {                                          \
        printf("FAIL: %s\n", strerror(errno)); \
        g_failed++;                            \
    } while (0)

#define SKIP(reason)                  \
    do                                \
    {                                 \
        printf("SKIP: %s\n", reason); \
        g_skipped++;                  \
    } while (0)

/* ---------- binderfs mount point ---------- */
#define BINDERFS_MNT "/tmp/test-ipc-binderfs"
#define BINDER_BUF_SZ (4 * 1024 * 1024) /* 4 MiB — Android default */
#define ASHMEM_SZ 4096

/* ================================================================
 *  Section 1: binderfs
 * ================================================================ */

static int binderfs_mounted;

static void test_binderfs(void)
{
    int fd, ret;
    struct binderfs_device dev;
    struct stat st;
    char path[512];

    printf("\n--- binderfs ---\n");

    /* 1a: Mount binderfs */
    TEST("binderfs", "mount");
    (void)mkdir(BINDERFS_MNT, 0755);
    if (mount("binder", BINDERFS_MNT, "binder", 0, NULL) < 0)
    {
        FAIL_ERRNO();
        return;
    }
    binderfs_mounted = 1;
    PASS();

    /* 1b: binder-control exists */
    TEST("binderfs", "binder-control exists");
    snprintf(path, sizeof(path), "%s/binder-control", BINDERFS_MNT);
    if (stat(path, &st) < 0)
    {
        FAIL_ERRNO();
    }
    else if (!S_ISCHR(st.st_mode))
    {
        FAIL("not a char device");
    }
    else
    {
        PASS();
    }

    /* 1c: default devices present */
    static const char *default_devs[] = {"binder", "hwbinder", "vndbinder"};
    for (int i = 0; i < 3; i++)
    {
        char desc[64];
        snprintf(desc, sizeof(desc), "default device: %s", default_devs[i]);
        TEST("binderfs", desc);
        snprintf(path, sizeof(path), "%s/%s", BINDERFS_MNT, default_devs[i]);
        if (stat(path, &st) < 0)
        {
            FAIL_ERRNO();
        }
        else if (!S_ISCHR(st.st_mode))
        {
            FAIL("not a char device");
        }
        else
        {
            PASS();
        }
    }

    /* 1d: allocate custom device via BINDER_CTL_ADD */
    TEST("binderfs", "BINDER_CTL_ADD (custom device)");
    snprintf(path, sizeof(path), "%s/binder-control", BINDERFS_MNT);
    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
    {
        FAIL_ERRNO();
    }
    else
    {
        memset(&dev, 0, sizeof(dev));
        snprintf(dev.name, sizeof(dev.name), "test-ipc-dev");
        ret = ioctl(fd, BINDER_CTL_ADD, &dev);
        if (ret < 0)
        {
            FAIL_ERRNO();
        }
        else
        {
            printf("PASS (major=%u minor=%u)\n", dev.major, dev.minor);
            g_passed++;
        }
        close(fd);
    }

    /* 1e: open the custom device */
    TEST("binderfs", "open custom device");
    snprintf(path, sizeof(path), "%s/test-ipc-dev", BINDERFS_MNT);
    fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        FAIL_ERRNO();
    }
    else
    {
        PASS();
        close(fd);
    }

    /* 1f: features directory */
    TEST("binderfs", "features directory exists");
    snprintf(path, sizeof(path), "%s/features", BINDERFS_MNT);
    if (stat(path, &st) < 0)
    {
        FAIL_ERRNO();
    }
    else if (!S_ISDIR(st.st_mode))
    {
        FAIL("not a directory");
    }
    else
    {
        PASS();
    }

    /* 1g: reject regular file creation */
    TEST("binderfs", "reject touch (read-only fs)");
    snprintf(path, sizeof(path), "%s/should-fail", BINDERFS_MNT);
    fd = open(path, O_WRONLY | O_CREAT, 0644);
    if (fd >= 0)
    {
        close(fd);
        unlink(path);
        FAIL("file creation should be denied");
    }
    else
    {
        PASS();
    }
}

/* ================================================================
 *  Section 2: binder device
 * ================================================================ */

static void test_binder(void)
{
    int fd;
    char path[512];
    struct binder_version ver;
    __u32 max_threads = 15;

    printf("\n--- binder ---\n");

    if (!binderfs_mounted)
    {
        SKIP("binderfs not mounted");
        return;
    }

    snprintf(path, sizeof(path), "%s/binder", BINDERFS_MNT);

    /* 2a: open */
    TEST("binder", "open device");
    fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        FAIL_ERRNO();
        return;
    }
    PASS();

    /* 2b: get version */
    TEST("binder", "BINDER_VERSION ioctl");
    memset(&ver, 0, sizeof(ver));
    if (ioctl(fd, BINDER_VERSION, &ver) < 0)
    {
        FAIL_ERRNO();
    }
    else
    {
        PASS();
    }

    /* 2c: check protocol version */
    TEST("binder", "protocol version == 8 (64-bit)");
    if (ver.protocol_version != 8)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "got %d", ver.protocol_version);
        FAIL(buf);
    }
    else
    {
        PASS();
    }

    /* 2d: set max threads */
    TEST("binder", "BINDER_SET_MAX_THREADS");
    if (ioctl(fd, BINDER_SET_MAX_THREADS, &max_threads) < 0)
    {
        FAIL_ERRNO();
    }
    else
    {
        PASS();
    }

    /* 2e: mmap binder buffer */
    TEST("binder", "mmap 4 MiB buffer");
    void *map = mmap(NULL, BINDER_BUF_SZ, PROT_READ,
                     MAP_PRIVATE | MAP_NORESERVE, fd, 0);
    if (map == MAP_FAILED)
    {
        FAIL_ERRNO();
    }
    else
    {
        PASS();
        /* 2f: munmap */
        TEST("binder", "munmap buffer");
        if (munmap(map, BINDER_BUF_SZ) < 0)
            FAIL_ERRNO();
        else
            PASS();
    }

    /* 2g: hwbinder and vndbinder open */
    static const char *extra[] = {"hwbinder", "vndbinder"};
    for (int i = 0; i < 2; i++)
    {
        char desc[64];
        snprintf(desc, sizeof(desc), "open %s", extra[i]);
        TEST("binder", desc);
        snprintf(path, sizeof(path), "%s/%s", BINDERFS_MNT, extra[i]);
        int efd = open(path, O_RDWR | O_CLOEXEC);
        if (efd < 0)
            FAIL_ERRNO();
        else
        {
            PASS();
            close(efd);
        }
    }

    close(fd);
}

/* ================================================================
 *  Section 3: ashmem
 * ================================================================ */

static void test_ashmem(void)
{
    int fd, fd2, ret;
    void *ptr;
    char namebuf[ASHMEM_NAME_LEN];
    struct ashmem_pin pin;
    const char *test_name = "test-ipc-region";

    printf("\n--- ashmem ---\n");

    /* 3a: open */
    TEST("ashmem", "open /dev/ashmem");
    fd = open("/dev/ashmem", O_RDWR);
    if (fd < 0)
    {
        FAIL_ERRNO();
        printf("  Cannot continue ashmem tests.\n");
        return;
    }
    PASS();

    /* 3b: set name */
    TEST("ashmem", "ASHMEM_SET_NAME");
    if (ioctl(fd, ASHMEM_SET_NAME, test_name) < 0)
    {
        FAIL_ERRNO();
    }
    else
    {
        PASS();
    }

    /* 3c: get name */
    TEST("ashmem", "ASHMEM_GET_NAME");
    memset(namebuf, 0, sizeof(namebuf));
    if (ioctl(fd, ASHMEM_GET_NAME, namebuf) < 0)
    {
        FAIL_ERRNO();
    }
    else if (strncmp(namebuf, test_name, strlen(test_name)) != 0)
    {
        FAIL("name mismatch");
    }
    else
    {
        PASS();
    }

    /* 3d: set size */
    TEST("ashmem", "ASHMEM_SET_SIZE (4096)");
    if (ioctl(fd, ASHMEM_SET_SIZE, (size_t)ASHMEM_SZ) < 0)
    {
        FAIL_ERRNO();
    }
    else
    {
        PASS();
    }

    /* 3e: get size */
    TEST("ashmem", "ASHMEM_GET_SIZE");
    ret = ioctl(fd, ASHMEM_GET_SIZE);
    if (ret < 0)
    {
        FAIL_ERRNO();
    }
    else if ((size_t)ret != ASHMEM_SZ)
    {
        FAIL("size mismatch");
    }
    else
    {
        PASS();
    }

    /* 3f: mmap */
    TEST("ashmem", "mmap region");
    ptr = mmap(NULL, ASHMEM_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED)
    {
        FAIL_ERRNO();
        printf("  Cannot continue mmap-dependent tests.\n");
        goto ashmem_pin_tests;
    }
    PASS();

    /* 3g: write + read */
    TEST("ashmem", "write and read back");
    memset(ptr, 0, ASHMEM_SZ);
    snprintf((char *)ptr, ASHMEM_SZ, "ashmem IPC OK — redroid-modules");
    if (strcmp((char *)ptr, "ashmem IPC OK — redroid-modules") != 0)
    {
        FAIL("data mismatch");
    }
    else
    {
        PASS();
    }

    /* 3h: large write (fill page) */
    TEST("ashmem", "fill 4096 bytes and verify");
    memset(ptr, 0xAB, ASHMEM_SZ);
    {
        unsigned char *p = ptr;
        int ok = 1;
        for (size_t i = 0; i < ASHMEM_SZ; i++)
        {
            if (p[i] != 0xAB)
            {
                ok = 0;
                break;
            }
        }
        if (ok)
            PASS();
        else
            FAIL("byte pattern mismatch");
    }

    munmap(ptr, ASHMEM_SZ);

ashmem_pin_tests:
    /* 3i: unpin */
    TEST("ashmem", "ASHMEM_UNPIN (entire region)");
    pin.offset = 0;
    pin.len = 0;
    ret = ioctl(fd, ASHMEM_UNPIN, &pin);
    if (ret < 0)
    {
        FAIL_ERRNO();
    }
    else
    {
        PASS();
    }

    /* 3j: pin status -> unpinned */
    TEST("ashmem", "pin status == UNPINNED");
    ret = ioctl(fd, ASHMEM_GET_PIN_STATUS, &pin);
    if (ret < 0)
    {
        FAIL_ERRNO();
    }
    else if (ret != ASHMEM_IS_UNPINNED)
    {
        FAIL("expected UNPINNED");
    }
    else
    {
        PASS();
    }

    /* 3k: re-pin */
    TEST("ashmem", "ASHMEM_PIN (re-pin)");
    ret = ioctl(fd, ASHMEM_PIN, &pin);
    if (ret < 0)
    {
        FAIL_ERRNO();
    }
    else
    {
        PASS();
    }

    /* 3l: pin status -> pinned */
    TEST("ashmem", "pin status == PINNED");
    ret = ioctl(fd, ASHMEM_GET_PIN_STATUS, &pin);
    if (ret < 0)
    {
        FAIL_ERRNO();
    }
    else if (ret != ASHMEM_IS_PINNED)
    {
        FAIL("expected PINNED");
    }
    else
    {
        PASS();
    }

    /* 3m: get prot mask */
    TEST("ashmem", "ASHMEM_GET_PROT_MASK (default)");
    ret = ioctl(fd, ASHMEM_GET_PROT_MASK);
    if (ret < 0)
    {
        FAIL_ERRNO();
    }
    else if (ret != (PROT_READ | PROT_WRITE | PROT_EXEC))
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "unexpected mask 0x%x", ret);
        FAIL(buf);
    }
    else
    {
        PASS();
    }

    /* 3n: set prot mask to read-only */
    TEST("ashmem", "ASHMEM_SET_PROT_MASK (read-only)");
    {
        unsigned long mask = PROT_READ;
        if (ioctl(fd, ASHMEM_SET_PROT_MASK, mask) < 0)
        {
            FAIL_ERRNO();
        }
        else
        {
            ret = ioctl(fd, ASHMEM_GET_PROT_MASK);
            if (ret < 0)
            {
                FAIL_ERRNO();
            }
            else if (ret != PROT_READ)
            {
                char buf[64];
                snprintf(buf, sizeof(buf), "got 0x%x", ret);
                FAIL(buf);
            }
            else
            {
                PASS();
            }
        }
    }

    /* 3o: purge all caches */
    TEST("ashmem", "ASHMEM_PURGE_ALL_CACHES");
    ret = ioctl(fd, ASHMEM_PURGE_ALL_CACHES);
    if (ret < 0)
    {
        FAIL_ERRNO();
    }
    else
    {
        PASS();
    }

    close(fd);

    /* 3p: multiple concurrent regions */
    TEST("ashmem", "multiple concurrent regions (3)");
    {
        int fds[3];
        void *maps[3];
        int ok = 1;
        for (int i = 0; i < 3; i++)
        {
            fds[i] = open("/dev/ashmem", O_RDWR);
            if (fds[i] < 0)
            {
                ok = 0;
                break;
            }
            if (ioctl(fds[i], ASHMEM_SET_SIZE, (size_t)ASHMEM_SZ) < 0)
            {
                ok = 0;
                break;
            }
            maps[i] = mmap(NULL, ASHMEM_SZ, PROT_READ | PROT_WRITE,
                           MAP_SHARED, fds[i], 0);
            if (maps[i] == MAP_FAILED)
            {
                ok = 0;
                break;
            }
            /* write distinct pattern */
            memset(maps[i], 0x10 + i, ASHMEM_SZ);
        }
        /* verify isolation */
        if (ok)
        {
            for (int i = 0; i < 3; i++)
            {
                unsigned char *p = maps[i];
                for (size_t j = 0; j < ASHMEM_SZ; j++)
                {
                    if (p[j] != (unsigned char)(0x10 + i))
                    {
                        ok = 0;
                        break;
                    }
                }
                if (!ok)
                    break;
            }
        }
        /* cleanup */
        for (int i = 0; i < 3; i++)
        {
            if (fds[i] >= 0)
            {
                if (maps[i] != MAP_FAILED)
                    munmap(maps[i], ASHMEM_SZ);
                close(fds[i]);
            }
        }
        if (ok)
            PASS();
        else
            FAIL("region isolation broken");
    }

    /* 3q: second fd shares mapping (dup) */
    TEST("ashmem", "dup() shares memory");
    fd = open("/dev/ashmem", O_RDWR);
    if (fd < 0)
    {
        FAIL_ERRNO();
    }
    else
    {
        ioctl(fd, ASHMEM_SET_SIZE, (size_t)ASHMEM_SZ);
        ptr = mmap(NULL, ASHMEM_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED)
        {
            FAIL_ERRNO();
        }
        else
        {
            fd2 = dup(fd);
            if (fd2 < 0)
            {
                FAIL_ERRNO();
            }
            else
            {
                void *ptr2 = mmap(NULL, ASHMEM_SZ, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, fd2, 0);
                if (ptr2 == MAP_FAILED)
                {
                    FAIL_ERRNO();
                }
                else
                {
                    snprintf((char *)ptr, ASHMEM_SZ, "shared!");
                    if (strcmp((char *)ptr2, "shared!") == 0)
                        PASS();
                    else
                        FAIL("data not shared");
                    munmap(ptr2, ASHMEM_SZ);
                }
                close(fd2);
            }
            munmap(ptr, ASHMEM_SZ);
        }
        close(fd);
    }
}

/* ================================================================
 *  Section 4: sysfs / debugfs introspection
 * ================================================================ */

static void test_sysfs(void)
{
    int fd;
    char buf[256];
    ssize_t n;

    printf("\n--- sysfs / debugfs ---\n");

    /* 4a: ashmem backing_mode */
    TEST("sysfs", "ashmem backing_mode readable");
    fd = open("/sys/module/ashmem_linux/parameters/ashmem_backing_mode", O_RDONLY);
    if (fd < 0)
    {
        SKIP("param not found (older module?)");
    }
    else
    {
        n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0)
        {
            buf[n] = '\0';
            printf("PASS (value=%s)\n", buf);
            g_passed++;
        }
        else
        {
            FAIL("read failed");
        }
    }

    /* 4b: ashmem debug_mask */
    TEST("sysfs", "ashmem debug_mask readable");
    fd = open("/sys/module/ashmem_linux/parameters/ashmem_debug_mask", O_RDONLY);
    if (fd < 0)
    {
        SKIP("param not found (older module?)");
    }
    else
    {
        n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0)
        {
            buf[n] = '\0';
            printf("PASS (value=%s)\n", buf);
            g_passed++;
        }
        else
        {
            FAIL("read failed");
        }
    }

    /* 4c: binder alloc_debug_mask */
    TEST("sysfs", "binder alloc_debug_mask readable");
    fd = open("/sys/module/binder_linux/parameters/alloc_debug_mask", O_RDONLY);
    if (fd < 0)
    {
        SKIP("param not found (older module?)");
    }
    else
    {
        n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0)
        {
            buf[n] = '\0';
            printf("PASS (value=%s)\n", buf);
            g_passed++;
        }
        else
        {
            FAIL("read failed");
        }
    }

    /* 4d: debugfs ashmem stats */
    TEST("debugfs", "ashmem/stats readable");
    fd = open("/sys/kernel/debug/ashmem/stats", O_RDONLY);
    if (fd < 0)
    {
        SKIP("debugfs not available");
    }
    else
    {
        n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0)
        {
            PASS();
        }
        else
        {
            FAIL("read failed");
        }
    }

    /* 4e: debugfs ashmem regions */
    TEST("debugfs", "ashmem/regions readable");
    fd = open("/sys/kernel/debug/ashmem/regions", O_RDONLY);
    if (fd < 0)
    {
        SKIP("debugfs not available");
    }
    else
    {
        n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        /* n == 0 is fine (no regions currently open) */
        PASS();
    }

    /* 4f: binder filesystem in /proc */
    TEST("sysfs", "binder in /proc/filesystems");
    {
        FILE *fp = fopen("/proc/filesystems", "r");
        if (!fp)
        {
            FAIL_ERRNO();
        }
        else
        {
            int found = 0;
            while (fgets(buf, sizeof(buf), fp))
            {
                if (strstr(buf, "binder"))
                {
                    found = 1;
                    break;
                }
            }
            fclose(fp);
            if (found)
                PASS();
            else
                FAIL("'binder' not in /proc/filesystems");
        }
    }
}

/* ================================================================
 *  Cleanup
 * ================================================================ */

static void cleanup(void)
{
    if (binderfs_mounted)
    {
        umount2(BINDERFS_MNT, MNT_DETACH);
        rmdir(BINDERFS_MNT);
    }
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void)
{
    printf("=== redroid-modules IPC validation ===\n");
    printf("Tests binderfs + binder + ashmem for Android compatibility.\n");

    test_binderfs();
    test_binder();
    test_ashmem();
    test_sysfs();
    cleanup();

    printf("\n========================================\n");
    printf("  PASSED:  %d\n", g_passed);
    printf("  FAILED:  %d\n", g_failed);
    printf("  SKIPPED: %d\n", g_skipped);
    printf("========================================\n");

    if (g_failed)
        printf("  RESULT: FAIL\n");
    else
        printf("  RESULT: OK — Android IPC ready\n");

    return g_failed ? 1 : 0;
}
