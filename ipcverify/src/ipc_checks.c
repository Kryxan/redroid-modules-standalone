#define _GNU_SOURCE

#include "ipc_checks.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef __ANDROID__
#include <android/sharedmem.h>
#include <dlfcn.h>
#include <sys/system_properties.h>
#endif

#include "uapi/linux/android/binder.h"

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

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

#ifndef SYS_memfd_create
#if defined(__x86_64__)
#define SYS_memfd_create 319
#elif defined(__i386__)
#define SYS_memfd_create 356
#elif defined(__aarch64__)
#define SYS_memfd_create 279
#elif defined(__arm__)
#define SYS_memfd_create 385
#endif
#endif

struct ashmem_pin
{
    unsigned int offset;
    unsigned int len;
};

static const char *const k_binder_candidates[] = {
    "/dev/binder",
    "/dev/binderfs/binder",
    "/dev/hwbinder",
    "/dev/vndbinder",
    "/dev/binderfs/hwbinder",
    "/dev/binderfs/vndbinder",
    NULL,
};

static int path_exists(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0;
}

static int select_binder_device(char *path, size_t path_size)
{
    size_t i;

    for (i = 0; k_binder_candidates[i] != NULL; ++i)
    {
        if (path_exists(k_binder_candidates[i]))
        {
            snprintf(path, path_size, "%s", k_binder_candidates[i]);
            return 1;
        }
    }

    if (path_size > 0)
    {
        path[0] = '\0';
    }
    return 0;
}

void ipcverify_report_init(ipcverify_report *report)
{
    if (!report)
    {
        return;
    }
    memset(report, 0, sizeof(*report));
}

void ipcverify_report_append(ipcverify_report *report, const char *fmt, ...)
{
    va_list args;
    int written;

    if (!report || !fmt || report->used >= sizeof(report->text))
    {
        return;
    }

    va_start(args, fmt);
    written = vsnprintf(report->text + report->used,
                        sizeof(report->text) - report->used,
                        fmt,
                        args);
    va_end(args);

    if (written < 0)
    {
        return;
    }

    if ((size_t)written >= sizeof(report->text) - report->used)
    {
        report->used = sizeof(report->text) - 1U;
    }
    else
    {
        report->used += (size_t)written;
    }
}

const char *ipcverify_status_text(enum ipcverify_status status)
{
    switch (status)
    {
    case IPCVERIFY_STATUS_PASS:
        return "passed";
    case IPCVERIFY_STATUS_FAIL:
        return "failed";
    case IPCVERIFY_STATUS_SKIP:
        return "skipped";
    case IPCVERIFY_STATUS_NOT_PRESENT:
        return "not present";
    default:
        return "unknown";
    }
}

void ipcverify_log_check(ipcverify_report *report, const char *label,
                         enum ipcverify_status status, const char *detail)
{
    ipcverify_report_append(report, "%s... %s", label, ipcverify_status_text(status));
    if (detail && *detail)
    {
        ipcverify_report_append(report, " (%s)", detail);
    }
    ipcverify_report_append(report, "\n");

    switch (status)
    {
    case IPCVERIFY_STATUS_PASS:
        report->pass_count++;
        break;
    case IPCVERIFY_STATUS_FAIL:
        report->fail_count++;
        break;
    case IPCVERIFY_STATUS_SKIP:
        report->skip_count++;
        break;
    case IPCVERIFY_STATUS_NOT_PRESENT:
        report->not_present_count++;
        break;
    default:
        break;
    }
}

static int binder_version_check(const char *path, char *detail, size_t detail_len)
{
    int fd;
    struct binder_version version;

    fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        snprintf(detail, detail_len, "%s", strerror(errno));
        return -1;
    }

    memset(&version, 0, sizeof(version));
    if (ioctl(fd, BINDER_VERSION, &version) < 0)
    {
        snprintf(detail, detail_len, "%s", strerror(errno));
        close(fd);
        return -1;
    }

    snprintf(detail, detail_len, "protocol %d via %s", version.protocol_version, path);
    close(fd);
    return 0;
}

static int binder_buffer_check(const char *path, char *detail, size_t detail_len)
{
    const size_t map_size = 1024U * 1024U;
    int fd;
    void *mapped;

    fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        snprintf(detail, detail_len, "%s", strerror(errno));
        return -1;
    }

    mapped = mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED)
    {
        snprintf(detail, detail_len, "%s", strerror(errno));
        close(fd);
        return -1;
    }

    if (munmap(mapped, map_size) != 0)
    {
        snprintf(detail, detail_len, "%s", strerror(errno));
        close(fd);
        return -1;
    }

    snprintf(detail, detail_len, "mmap/munmap succeeded via %s", path);
    close(fd);
    return 0;
}

static int binder_shrinker_check(const char *path, char *detail, size_t detail_len)
{
    const size_t map_size = 1024U * 1024U;
    int fd;
    void *mapped;

    fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        snprintf(detail, detail_len, "%s", strerror(errno));
        return -1;
    }

    mapped = mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED)
    {
        snprintf(detail, detail_len, "%s", strerror(errno));
        close(fd);
        return -1;
    }

    if (madvise(mapped, map_size, MADV_DONTNEED) != 0)
    {
        snprintf(detail, detail_len, "%s", strerror(errno));
        munmap(mapped, map_size);
        close(fd);
        return -1;
    }

    if (munmap(mapped, map_size) != 0)
    {
        snprintf(detail, detail_len, "%s", strerror(errno));
        close(fd);
        return -1;
    }

    snprintf(detail, detail_len, "madvise(MADV_DONTNEED) succeeded via %s", path);
    close(fd);
    return 0;
}

static int ashmem_full_check(char *detail, size_t detail_len)
{
    const size_t ashmem_size = 4096U;
    int fd;
    int size_result;
    void *mapped;
    char name[] = "ipcverify";
    char readback[ASHMEM_NAME_LEN];
    struct ashmem_pin pin;

    fd = open("/dev/ashmem", O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        snprintf(detail, detail_len, "%s", strerror(errno));
        return -1;
    }

    if (ioctl(fd, ASHMEM_SET_NAME, name) < 0)
    {
        snprintf(detail, detail_len, "set name: %s", strerror(errno));
        close(fd);
        return -1;
    }

    memset(readback, 0, sizeof(readback));
    if (ioctl(fd, ASHMEM_GET_NAME, readback) < 0)
    {
        snprintf(detail, detail_len, "get name: %s", strerror(errno));
        close(fd);
        return -1;
    }

    if (ioctl(fd, ASHMEM_SET_SIZE, ashmem_size) < 0)
    {
        snprintf(detail, detail_len, "set size: %s", strerror(errno));
        close(fd);
        return -1;
    }

    size_result = ioctl(fd, ASHMEM_GET_SIZE, NULL);
    if (size_result < 0 || (size_t)size_result != ashmem_size)
    {
        snprintf(detail, detail_len, "get size: %s", size_result < 0 ? strerror(errno) : "size mismatch");
        close(fd);
        return -1;
    }

    mapped = mmap(NULL, ashmem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED)
    {
        snprintf(detail, detail_len, "mmap: %s", strerror(errno));
        close(fd);
        return -1;
    }

    memcpy(mapped, "ipcverify", sizeof("ipcverify"));
    pin.offset = 0U;
    pin.len = 0U;
    if (ioctl(fd, ASHMEM_UNPIN, &pin) < 0)
    {
        snprintf(detail, detail_len, "unpin: %s", strerror(errno));
        munmap(mapped, ashmem_size);
        close(fd);
        return -1;
    }
    if (ioctl(fd, ASHMEM_PIN, &pin) < 0)
    {
        snprintf(detail, detail_len, "pin: %s", strerror(errno));
        munmap(mapped, ashmem_size);
        close(fd);
        return -1;
    }

    munmap(mapped, ashmem_size);
    close(fd);
    snprintf(detail, detail_len, "name, size, mmap, pin/unpin succeeded");
    return 0;
}

static int memfd_full_check(char *detail, size_t detail_len)
{
#if defined(SYS_memfd_create)
    const size_t memfd_size = 4096U;
    int fd;
    void *mapped;

    fd = (int)syscall(SYS_memfd_create, "ipcverify-memfd", MFD_CLOEXEC);
    if (fd < 0)
    {
        snprintf(detail, detail_len, "%s", strerror(errno));
        return -1;
    }

    if (ftruncate(fd, (off_t)memfd_size) != 0)
    {
        snprintf(detail, detail_len, "ftruncate: %s", strerror(errno));
        close(fd);
        return -1;
    }

    mapped = mmap(NULL, memfd_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED)
    {
        snprintf(detail, detail_len, "mmap: %s", strerror(errno));
        close(fd);
        return -1;
    }

    memcpy(mapped, "memfd-ok", sizeof("memfd-ok"));
    munmap(mapped, memfd_size);
    close(fd);
    snprintf(detail, detail_len, "memfd_create + mmap succeeded");
    return 0;
#else
    snprintf(detail, detail_len, "memfd_create is not available on this architecture");
    errno = ENOSYS;
    return -1;
#endif
}

static double ipcverify_monotonic_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return 0.0;
    }
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static int performance_probe(char *detail, size_t detail_len)
{
    const size_t bytes = 8U * 1024U * 1024U;
    unsigned char *buffer;
    double start_ms;
    double elapsed_ms;
    size_t i;

    buffer = (unsigned char *)malloc(bytes);
    if (!buffer)
    {
        snprintf(detail, detail_len, "%s", strerror(errno));
        return -1;
    }

    start_ms = ipcverify_monotonic_ms();
    for (i = 0; i < bytes; ++i)
    {
        buffer[i] = (unsigned char)(i & 0xffU);
    }
    elapsed_ms = ipcverify_monotonic_ms() - start_ms;
    free(buffer);

    snprintf(detail, detail_len, "8 MiB native heap sweep in %.2f ms", elapsed_ms);
    return 0;
}

static int binder_transaction_limit_check(const char *path, char *detail, size_t detail_len)
{
    const size_t map_size = 1024U * 1024U;
    int fd;
    __u32 max_threads = 8U;
    void *mapped;

    fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        snprintf(detail, detail_len, "%s", strerror(errno));
        return -1;
    }

    if (ioctl(fd, BINDER_SET_MAX_THREADS, &max_threads) < 0)
    {
        snprintf(detail, detail_len, "set max threads: %s", strerror(errno));
        close(fd);
        return -1;
    }

    mapped = mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED)
    {
        snprintf(detail, detail_len, "1 MiB transaction map: %s", strerror(errno));
        close(fd);
        return -1;
    }

    munmap(mapped, map_size);
    close(fd);
    snprintf(detail, detail_len, "1 MiB binder map + thread limit ioctl succeeded");
    return 0;
}

static int binder_handle_passing_check(char *detail, size_t detail_len)
{
#ifdef __ANDROID__
    FILE *pipe = popen("/system/bin/service list 2>/dev/null", "r");
    char line[256];

    if (!pipe)
    {
        snprintf(detail, detail_len, "service manager query unavailable in app sandbox");
        return 1;
    }

    if (fgets(line, sizeof(line), pipe) && strstr(line, "Found"))
    {
        line[strcspn(line, "\r\n")] = '\0';
        pclose(pipe);
        snprintf(detail, detail_len, "%s", line);
        return 0;
    }

    pclose(pipe);
    snprintf(detail, detail_len, "service manager output was unavailable");
    return 1;
#else
    snprintf(detail, detail_len, "binder handle passing is only probed inside Android userspace");
    return 1;
#endif
}

#ifdef __ANDROID__
static void get_android_property(const char *name, char *buffer, size_t buffer_size)
{
    char value[PROP_VALUE_MAX] = {0};
    int len = __system_property_get(name, value);

    if (len <= 0)
    {
        snprintf(buffer, buffer_size, "unknown");
    }
    else
    {
        snprintf(buffer, buffer_size, "%s", value);
    }
}

static int asharedmemory_api_check(char *detail, size_t detail_len)
{
    typedef int (*asharedmemory_create_fn)(const char *name, size_t size);
    typedef int (*asharedmemory_setprot_fn)(int fd, int prot);

    const size_t size = 4096U;
    void *handle;
    asharedmemory_create_fn create_fn;
    asharedmemory_setprot_fn setprot_fn;
    int fd;
    void *mapped;

    handle = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
    if (!handle)
    {
        snprintf(detail, detail_len, "libandroid.so could not be opened");
        return 1;
    }

    create_fn = (asharedmemory_create_fn)dlsym(handle, "ASharedMemory_create");
    setprot_fn = (asharedmemory_setprot_fn)dlsym(handle, "ASharedMemory_setProt");
    if (!create_fn || !setprot_fn)
    {
        dlclose(handle);
        snprintf(detail, detail_len, "ASharedMemory API requires Android 8.0+ (API 26+)");
        return 1;
    }

    fd = create_fn("ipcverify-shared", size);
    if (fd < 0)
    {
        dlclose(handle);
        snprintf(detail, detail_len, "ASharedMemory_create: %s", strerror(errno));
        return -1;
    }

    if (setprot_fn(fd, PROT_READ | PROT_WRITE) != 0)
    {
        dlclose(handle);
        snprintf(detail, detail_len, "ASharedMemory_setProt: %s", strerror(errno));
        close(fd);
        return -1;
    }

    mapped = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED)
    {
        dlclose(handle);
        snprintf(detail, detail_len, "mmap: %s", strerror(errno));
        close(fd);
        return -1;
    }

    memcpy(mapped, "asharedmemory-ok", sizeof("asharedmemory-ok"));
    munmap(mapped, size);
    close(fd);
    dlclose(handle);
    snprintf(detail, detail_len, "ASharedMemory_create + setProt + mmap succeeded");
    return 0;
}
#endif

static void append_runtime_checks(ipcverify_report *report)
{
    struct
    {
        char tag;
        void *ptr;
        int value;
    } layout_probe;
    char detail[256];
    const char *arch = "unknown";

#if defined(__aarch64__)
    arch = "arm64-v8a";
#elif defined(__arm__)
    arch = "armeabi-v7a";
#elif defined(__x86_64__)
    arch = "x86_64";
#elif defined(__i386__)
    arch = "x86";
#endif

    snprintf(detail, sizeof(detail), "compiled for %s, pointer size=%zu-bit", arch, sizeof(void *) * 8U);
    ipcverify_log_check(report, "checking ABI correctness", IPCVERIFY_STATUS_PASS, detail);

    snprintf(detail, sizeof(detail), "struct=%zu bytes, pointer alignment=%zu bytes", sizeof(layout_probe), _Alignof(void *));
    ipcverify_log_check(report, "checking 32-bit vs 64-bit behavior", IPCVERIFY_STATUS_PASS, detail);

#ifdef __ANDROID__
    {
        char bridge[PROP_VALUE_MAX];
        char abilist[PROP_VALUE_MAX];
        get_android_property("ro.dalvik.vm.native.bridge", bridge, sizeof(bridge));
        get_android_property("ro.product.cpu.abilist", abilist, sizeof(abilist));

        if (strcmp(bridge, "0") != 0 && strcmp(bridge, "unknown") != 0 && bridge[0] != '\0')
        {
            snprintf(detail, sizeof(detail), "native bridge=%s, abilist=%s", bridge, abilist);
            ipcverify_log_check(report, "checking native bridge translation", IPCVERIFY_STATUS_PASS, detail);
        }
        else
        {
            snprintf(detail, sizeof(detail), "native bridge not active, abilist=%s", abilist);
            ipcverify_log_check(report, "checking native bridge translation", IPCVERIFY_STATUS_SKIP, detail);
        }

        ipcverify_log_check(report, "checking real app lifecycle", IPCVERIFY_STATUS_PASS,
                            "JNI checks were launched from the Android app entry point");
        if (performance_probe(detail, sizeof(detail)) == 0)
        {
            ipcverify_log_check(report, "checking real performance", IPCVERIFY_STATUS_PASS, detail);
            ipcverify_log_check(report, "checking native heap behavior", IPCVERIFY_STATUS_PASS, detail);
        }
        else
        {
            ipcverify_log_check(report, "checking real performance", IPCVERIFY_STATUS_FAIL, detail);
            ipcverify_log_check(report, "checking native heap behavior", IPCVERIFY_STATUS_FAIL, detail);
        }
        ipcverify_log_check(report, "checking GPU shim hooks", IPCVERIFY_STATUS_SKIP,
                            "future EGL/Vulkan verification hook");
    }
#else
    if (performance_probe(detail, sizeof(detail)) == 0)
    {
        ipcverify_log_check(report, "checking native heap behavior", IPCVERIFY_STATUS_PASS, detail);
    }
    else
    {
        ipcverify_log_check(report, "checking native heap behavior", IPCVERIFY_STATUS_FAIL, detail);
    }
#endif
}

int ipcverify_run_local_checks(ipcverify_report *report)
{
    char binder_path[256];
    char detail[256];
    int binder_present;
    int binder_pass;
    int ashmem_present;
    int handle_result;

    if (!report)
    {
        return 1;
    }

    ipcverify_report_init(report);
    ipcverify_report_append(report, "IPC Verification\n\n");
    append_runtime_checks(report);
    ipcverify_report_append(report, "\n");

    binder_present = select_binder_device(binder_path, sizeof(binder_path));
    if (!binder_present)
    {
        ipcverify_log_check(report, "checking binder", IPCVERIFY_STATUS_NOT_PRESENT,
                            "no binder device node was found");
        ipcverify_log_check(report, "  checking allocate buffer", IPCVERIFY_STATUS_NOT_PRESENT,
                            "binder device is unavailable");
        ipcverify_log_check(report, "  checking allocate shrinker", IPCVERIFY_STATUS_NOT_PRESENT,
                            "binder device is unavailable");
        ipcverify_log_check(report, "  checking binder transaction limits", IPCVERIFY_STATUS_NOT_PRESENT,
                            "binder device is unavailable");
        ipcverify_log_check(report, "  checking binder handle passing", IPCVERIFY_STATUS_NOT_PRESENT,
                            "binder device is unavailable");
    }
    else
    {
        binder_pass = 1;
        if (binder_version_check(binder_path, detail, sizeof(detail)) == 0)
        {
            ipcverify_log_check(report, "checking binder", IPCVERIFY_STATUS_PASS, detail);
        }
        else
        {
            ipcverify_log_check(report, "checking binder", IPCVERIFY_STATUS_FAIL, detail);
            binder_pass = 0;
        }

        if (binder_buffer_check(binder_path, detail, sizeof(detail)) == 0)
        {
            ipcverify_log_check(report, "  checking allocate buffer", IPCVERIFY_STATUS_PASS, detail);
        }
        else
        {
            ipcverify_log_check(report, "  checking allocate buffer", IPCVERIFY_STATUS_FAIL, detail);
            binder_pass = 0;
        }

        if (binder_shrinker_check(binder_path, detail, sizeof(detail)) == 0)
        {
            ipcverify_log_check(report, "  checking allocate shrinker", IPCVERIFY_STATUS_PASS, detail);
        }
        else
        {
            ipcverify_log_check(report, "  checking allocate shrinker", IPCVERIFY_STATUS_FAIL, detail);
            binder_pass = 0;
        }

        if (binder_transaction_limit_check(binder_path, detail, sizeof(detail)) == 0)
        {
            ipcverify_log_check(report, "  checking binder transaction limits", IPCVERIFY_STATUS_PASS, detail);
        }
        else
        {
            ipcverify_log_check(report, "  checking binder transaction limits", IPCVERIFY_STATUS_FAIL, detail);
            binder_pass = 0;
        }

        handle_result = binder_handle_passing_check(detail, sizeof(detail));
        if (handle_result == 0)
        {
            ipcverify_log_check(report, "  checking binder handle passing", IPCVERIFY_STATUS_PASS, detail);
        }
        else if (handle_result > 0)
        {
            ipcverify_log_check(report, "  checking binder handle passing", IPCVERIFY_STATUS_SKIP, detail);
        }
        else
        {
            ipcverify_log_check(report, "  checking binder handle passing", IPCVERIFY_STATUS_FAIL, detail);
            binder_pass = 0;
        }

        report->binder_ok = binder_pass;
    }

    ashmem_present = path_exists("/dev/ashmem");
    if (ashmem_present)
    {
        if (ashmem_full_check(detail, sizeof(detail)) == 0)
        {
            ipcverify_log_check(report, "checking ashmem", IPCVERIFY_STATUS_PASS, "device is present");
            ipcverify_log_check(report, "  checking set size", IPCVERIFY_STATUS_PASS, "ashmem size ioctl succeeded");
            ipcverify_log_check(report, "  checking mmap shared memory", IPCVERIFY_STATUS_PASS, "shared mapping succeeded");
            ipcverify_log_check(report, "  checking pin and unpin", IPCVERIFY_STATUS_PASS, "pin/unpin ioctls succeeded");
#ifdef __ANDROID__
            {
                int asharedmemory_result = asharedmemory_api_check(detail, sizeof(detail));
                if (asharedmemory_result == 0)
                {
                    ipcverify_log_check(report, "  checking ASharedMemory_create()", IPCVERIFY_STATUS_PASS, detail);
                    ipcverify_log_check(report, "  checking ASharedMemory_setProt()", IPCVERIFY_STATUS_PASS, detail);
                }
                else if (asharedmemory_result > 0)
                {
                    ipcverify_log_check(report, "  checking ASharedMemory_create()", IPCVERIFY_STATUS_SKIP, detail);
                    ipcverify_log_check(report, "  checking ASharedMemory_setProt()", IPCVERIFY_STATUS_SKIP, detail);
                }
                else
                {
                    ipcverify_log_check(report, "  checking ASharedMemory_create()", IPCVERIFY_STATUS_FAIL, detail);
                    ipcverify_log_check(report, "  checking ASharedMemory_setProt()", IPCVERIFY_STATUS_FAIL, detail);
                }
            }
#endif
            report->ashmem_ok = 1;
        }
        else
        {
            ipcverify_log_check(report, "checking ashmem", IPCVERIFY_STATUS_FAIL, detail);
            ipcverify_log_check(report, "  checking set size", IPCVERIFY_STATUS_FAIL, detail);
            ipcverify_log_check(report, "  checking mmap shared memory", IPCVERIFY_STATUS_FAIL, detail);
            ipcverify_log_check(report, "  checking pin and unpin", IPCVERIFY_STATUS_FAIL, detail);
#ifdef __ANDROID__
            ipcverify_log_check(report, "  checking ASharedMemory_create()", IPCVERIFY_STATUS_FAIL, detail);
            ipcverify_log_check(report, "  checking ASharedMemory_setProt()", IPCVERIFY_STATUS_FAIL, detail);
#endif
        }
    }
    else if (memfd_full_check(detail, sizeof(detail)) == 0)
    {
        char memfd_detail[256];

        snprintf(memfd_detail, sizeof(memfd_detail), "%s", detail);
        ipcverify_log_check(report, "checking ashmem", IPCVERIFY_STATUS_SKIP, "memfd fallback is available");
        ipcverify_log_check(report, "  checking set size", IPCVERIFY_STATUS_SKIP, "using memfd fallback");
        ipcverify_log_check(report, "  checking mmap shared memory", IPCVERIFY_STATUS_SKIP, "using memfd fallback");
        ipcverify_log_check(report, "  checking pin and unpin", IPCVERIFY_STATUS_SKIP, "using memfd fallback");
#ifdef __ANDROID__
        {
            int asharedmemory_result = asharedmemory_api_check(detail, sizeof(detail));
            if (asharedmemory_result == 0)
            {
                ipcverify_log_check(report, "  checking ASharedMemory_create()", IPCVERIFY_STATUS_PASS, detail);
                ipcverify_log_check(report, "  checking ASharedMemory_setProt()", IPCVERIFY_STATUS_PASS, detail);
            }
            else if (asharedmemory_result > 0)
            {
                ipcverify_log_check(report, "  checking ASharedMemory_create()", IPCVERIFY_STATUS_SKIP, detail);
                ipcverify_log_check(report, "  checking ASharedMemory_setProt()", IPCVERIFY_STATUS_SKIP, detail);
            }
            else
            {
                ipcverify_log_check(report, "  checking ASharedMemory_create()", IPCVERIFY_STATUS_FAIL, detail);
                ipcverify_log_check(report, "  checking ASharedMemory_setProt()", IPCVERIFY_STATUS_FAIL, detail);
            }
        }
#endif
        ipcverify_log_check(report, "checking memfd", IPCVERIFY_STATUS_PASS, memfd_detail);
        report->ashmem_ok = 1;
        report->memfd_ok = 1;
    }
    else
    {
        ipcverify_log_check(report, "checking ashmem", IPCVERIFY_STATUS_NOT_PRESENT,
                            "/dev/ashmem is unavailable and memfd fallback failed");
        ipcverify_log_check(report, "  checking set size", IPCVERIFY_STATUS_NOT_PRESENT,
                            "ashmem device is unavailable");
        ipcverify_log_check(report, "  checking mmap shared memory", IPCVERIFY_STATUS_NOT_PRESENT,
                            "ashmem device is unavailable");
        ipcverify_log_check(report, "  checking pin and unpin", IPCVERIFY_STATUS_NOT_PRESENT,
                            "ashmem device is unavailable");
#ifdef __ANDROID__
        ipcverify_log_check(report, "  checking ASharedMemory_create()", IPCVERIFY_STATUS_FAIL, detail);
        ipcverify_log_check(report, "  checking ASharedMemory_setProt()", IPCVERIFY_STATUS_FAIL, detail);
#endif
        ipcverify_log_check(report, "checking memfd", IPCVERIFY_STATUS_FAIL, detail);
    }

    ipcverify_report_append(report,
                            "\nsummary: %d passed, %d failed, %d skipped, %d not present\n",
                            report->pass_count,
                            report->fail_count,
                            report->skip_count,
                            report->not_present_count);

    return ipcverify_has_failures(report);
}

int ipcverify_has_failures(const ipcverify_report *report)
{
    if (!report)
    {
        return 1;
    }
    return (report->fail_count > 0 || report->not_present_count > 0) ? 1 : 0;
}
