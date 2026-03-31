#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#define ASHMEM_NAME_LEN 256
#define ASHMEM_NAME_DEF "dev/ashmem"
#define ASHMEM_NOT_PURGED 0
#define ASHMEM_WAS_PURGED 1
#define ASHMEM_IS_UNPINNED 0
#define ASHMEM_IS_PINNED 1

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

struct ashmem_pin
{
    unsigned int offset;
    unsigned int len;
};

static int passed, failed;

#define TEST(desc) printf("  TEST: %s ... ", desc)
#define PASS()            \
    do                    \
    {                     \
        printf("PASS\n"); \
        passed++;         \
    } while (0)
#define FAIL(reason)                  \
    do                                \
    {                                 \
        printf("FAIL: %s\n", reason); \
        failed++;                     \
    } while (0)

int main(void)
{
    int fd, ret;
    const char *name = "my_ashmem_test";
    const size_t size = 4096;
    void *ptr;
    char namebuf[ASHMEM_NAME_LEN];
    struct ashmem_pin pin;

    printf("=== ashmem test suite ===\n\n");

    /* --- Test 1: open --- */
    TEST("open /dev/ashmem");
    fd = open("/dev/ashmem", O_RDWR);
    if (fd < 0)
    {
        FAIL(strerror(errno));
        printf("\nCannot continue without /dev/ashmem.\n");
        return 1;
    }
    PASS();

    /* --- Test 2: set/get name --- */
    TEST("set name");
    if (ioctl(fd, ASHMEM_SET_NAME, name) < 0)
    {
        FAIL(strerror(errno));
    }
    else
    {
        PASS();
    }

    TEST("get name");
    memset(namebuf, 0, sizeof(namebuf));
    if (ioctl(fd, ASHMEM_GET_NAME, namebuf) < 0)
    {
        FAIL(strerror(errno));
    }
    else if (strncmp(namebuf, name, strlen(name)) != 0)
    {
        FAIL("name mismatch");
    }
    else
    {
        PASS();
    }

    /* --- Test 3: set/get size --- */
    TEST("set size");
    if (ioctl(fd, ASHMEM_SET_SIZE, size) < 0)
    {
        FAIL(strerror(errno));
    }
    else
    {
        PASS();
    }

    TEST("get size");
    ret = ioctl(fd, ASHMEM_GET_SIZE);
    if (ret < 0)
    {
        FAIL(strerror(errno));
    }
    else if ((size_t)ret != size)
    {
        FAIL("size mismatch");
    }
    else
    {
        PASS();
    }

    /* --- Test 4: mmap and read/write --- */
    TEST("mmap");
    ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED)
    {
        FAIL(strerror(errno));
        printf("\nCannot continue without mmap.\n");
        close(fd);
        goto summary;
    }
    PASS();

    TEST("write and read back");
    memset(ptr, 0, size);
    snprintf((char *)ptr, size, "Hello Ashmem!");
    if (strcmp((char *)ptr, "Hello Ashmem!") != 0)
    {
        FAIL("data mismatch");
    }
    else
    {
        PASS();
    }

    /* --- Test 5: pin/unpin --- */
    TEST("unpin region");
    pin.offset = 0;
    pin.len = 0; /* 0 = entire region */
    ret = ioctl(fd, ASHMEM_UNPIN, &pin);
    if (ret < 0)
    {
        FAIL(strerror(errno));
    }
    else
    {
        PASS();
    }

    TEST("get pin status (should be unpinned)");
    ret = ioctl(fd, ASHMEM_GET_PIN_STATUS, &pin);
    if (ret < 0)
    {
        FAIL(strerror(errno));
    }
    else if (ret != ASHMEM_IS_UNPINNED)
    {
        FAIL("expected UNPINNED");
    }
    else
    {
        PASS();
    }

    TEST("re-pin region");
    ret = ioctl(fd, ASHMEM_PIN, &pin);
    if (ret < 0)
    {
        FAIL(strerror(errno));
    }
    else
    {
        PASS();
    }

    TEST("get pin status (should be pinned)");
    ret = ioctl(fd, ASHMEM_GET_PIN_STATUS, &pin);
    if (ret < 0)
    {
        FAIL(strerror(errno));
    }
    else if (ret != ASHMEM_IS_PINNED)
    {
        FAIL("expected PINNED");
    }
    else
    {
        PASS();
    }

    /* --- Test 6: protection mask --- */
    TEST("get prot mask (default)");
    ret = ioctl(fd, ASHMEM_GET_PROT_MASK);
    if (ret < 0)
    {
        FAIL(strerror(errno));
    }
    else
    {
        PASS();
    }

    /* --- cleanup --- */
    munmap(ptr, size);
    close(fd);

summary:
    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed ? 1 : 0;
}
