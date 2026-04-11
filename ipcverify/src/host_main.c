#define _GNU_SOURCE

#include "ipc_checks.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define DEFAULT_ADB_HOST "localhost"
#define DEFAULT_ADB_PORT "5555"
#define DEFAULT_APK_RELATIVE "bin/ipcverify.apk"
#define ACTIVITY_NAME "com.kryxan.ipcverify/.HeadlessCheckActivity"

static int file_exists(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0;
}

static void trim_newline(char *text)
{
    size_t len;

    if (!text)
    {
        return;
    }

    len = strlen(text);
    while (len > 0 && (text[len - 1U] == '\n' || text[len - 1U] == '\r'))
    {
        text[--len] = '\0';
    }
}

static void copy_with_default(char *dest, size_t dest_size, const char *value, const char *fallback)
{
    const char *source = (value && *value) ? value : fallback;
    size_t length;

    if (!dest || dest_size == 0U)
    {
        return;
    }

    if (!source)
    {
        dest[0] = '\0';
        return;
    }

    length = strlen(source);
    if (length >= dest_size)
    {
        length = dest_size - 1U;
    }

    memcpy(dest, source, length);
    dest[length] = '\0';
}

static int join_path(char *dest, size_t dest_size, const char *left, const char *right)
{
    size_t left_len;
    size_t right_len;

    if (!dest || !left || !right || dest_size == 0U)
    {
        return 0;
    }

    left_len = strlen(left);
    right_len = strlen(right);
    if (left_len + 1U + right_len + 1U > dest_size)
    {
        dest[0] = '\0';
        return 0;
    }

    memcpy(dest, left, left_len);
    dest[left_len] = '/';
    memcpy(dest + left_len + 1U, right, right_len);
    dest[left_len + 1U + right_len] = '\0';
    return 1;
}

static int ask_yes_no(const char *prompt, int default_yes, int assume_yes)
{
    char answer[32];

    if (assume_yes)
    {
        printf("%s %s\n", prompt, default_yes ? "yes" : "no");
        return default_yes;
    }

    printf("%s [%c/%c]: ", prompt, default_yes ? 'Y' : 'y', default_yes ? 'n' : 'N');
    fflush(stdout);

    if (!fgets(answer, sizeof(answer), stdin))
    {
        return default_yes;
    }

    trim_newline(answer);
    if (answer[0] == '\0')
    {
        return default_yes;
    }

    return (answer[0] == 'y' || answer[0] == 'Y') ? 1 : 0;
}

static void ask_value(const char *prompt, const char *fallback,
                      char *buffer, size_t buffer_size, int assume_yes)
{
    char input[256];

    if (assume_yes)
    {
        copy_with_default(buffer, buffer_size, fallback, fallback);
        printf("%s %s\n", prompt, buffer);
        return;
    }

    printf("%s [%s]: ", prompt, fallback);
    fflush(stdout);
    if (!fgets(input, sizeof(input), stdin))
    {
        copy_with_default(buffer, buffer_size, fallback, fallback);
        return;
    }

    trim_newline(input);
    copy_with_default(buffer, buffer_size, input, fallback);
}

static int run_command(const char *command)
{
    int rc;

    printf("$ %s\n", command);
    rc = system(command);
    if (rc != 0)
    {
        printf("command failed with exit code %d\n", rc);
    }
    return rc;
}

static int capture_command(const char *command, char *buffer, size_t buffer_size)
{
    FILE *pipe;
    size_t used = 0U;

    if (!buffer || buffer_size == 0U)
    {
        return -1;
    }

    buffer[0] = '\0';
    pipe = popen(command, "r");
    if (!pipe)
    {
        return -1;
    }

    while (!feof(pipe) && used + 1U < buffer_size)
    {
        size_t remaining = buffer_size - used;
        if (!fgets(buffer + used, (int)remaining, pipe))
        {
            break;
        }
        used = strlen(buffer);
    }

    return pclose(pipe);
}

static int find_repo_root(char *buffer, size_t buffer_size)
{
    char current[PATH_MAX];

    if (!getcwd(current, sizeof(current)))
    {
        return 0;
    }

    while (1)
    {
        char makefile_path[PATH_MAX];
        char version_path[PATH_MAX];
        char ipcverify_path[PATH_MAX];
        char *slash;

        if (!join_path(makefile_path, sizeof(makefile_path), current, "Makefile") ||
            !join_path(version_path, sizeof(version_path), current, "VERSION") ||
            !join_path(ipcverify_path, sizeof(ipcverify_path), current, "ipcverify"))
        {
            break;
        }
        if (file_exists(makefile_path) && file_exists(version_path) && file_exists(ipcverify_path))
        {
            snprintf(buffer, buffer_size, "%s", current);
            return 1;
        }

        slash = strrchr(current, '/');
        if (!slash || slash == current)
        {
            break;
        }
        *slash = '\0';
    }

    return 0;
}

static int find_adb_binary(const char *repo_root, char *buffer, size_t buffer_size)
{
    const char *env_adb = getenv("ADB");
    char candidate[PATH_MAX];

    if (env_adb && *env_adb)
    {
        snprintf(buffer, buffer_size, "%s", env_adb);
        return 1;
    }

    if (repo_root && *repo_root)
    {
        snprintf(candidate, sizeof(candidate), "%s/adk-platform-tools/adb", repo_root);
        if (file_exists(candidate))
        {
            snprintf(buffer, buffer_size, "%s", candidate);
            return 1;
        }
        snprintf(candidate, sizeof(candidate), "%s/adk-platform-tools/adb.exe", repo_root);
        if (file_exists(candidate))
        {
            snprintf(buffer, buffer_size, "%s", candidate);
            return 1;
        }
    }

    if (capture_command("command -v adb 2>/dev/null", buffer, buffer_size) == 0)
    {
        trim_newline(buffer);
        if (*buffer)
        {
            return 1;
        }
    }

    if (capture_command("command -v adb.exe 2>/dev/null", buffer, buffer_size) == 0)
    {
        trim_newline(buffer);
        if (*buffer)
        {
            return 1;
        }
    }

    return 0;
}

static int ensure_android_apk(const char *repo_root, char *apk_path, size_t apk_path_size)
{
    char command[PATH_MAX * 2];

    if (apk_path[0] != '\0' && file_exists(apk_path))
    {
        return 0;
    }

    if (repo_root && *repo_root)
    {
        snprintf(apk_path, apk_path_size, "%s/%s", repo_root, DEFAULT_APK_RELATIVE);
        if (file_exists(apk_path))
        {
            return 0;
        }

        snprintf(command, sizeof(command), "cd '%s' && make ipcverify-android", repo_root);
        if (run_command(command) == 0 && file_exists(apk_path))
        {
            return 0;
        }
    }

    return -1;
}

static int maybe_write_text_file(const char *repo_root, const char *relative_path, const char *content)
{
    char path[PATH_MAX];
    FILE *file;

    if (!repo_root || !*repo_root || !content)
    {
        return -1;
    }

    snprintf(path, sizeof(path), "%s/%s", repo_root, relative_path);
    file = fopen(path, "w");
    if (!file)
    {
        return -1;
    }
    fputs(content, file);
    fclose(file);
    return 0;
}

static void maybe_create_support_bundle(const char *repo_root)
{
    char command[PATH_MAX * 2];
    char output[PATH_MAX];

    if (!repo_root || !*repo_root)
    {
        printf("support bundle suggestion: make support-bundle\n");
        return;
    }

    snprintf(command, sizeof(command), "cd '%s' && ./scripts/create-support-bundle.sh", repo_root);
    if (capture_command(command, output, sizeof(output)) == 0)
    {
        trim_newline(output);
        if (*output)
        {
            printf("support bundle: %s\n", output);
            return;
        }
    }

    printf("support bundle suggestion: cd '%s' && make support-bundle\n", repo_root);
}

static int run_adb_verification(const char *repo_root, const char *adb_binary,
                                const char *adb_host, const char *adb_port,
                                const char *apk_path)
{
    char command[PATH_MAX * 3];
    char logcat[16384];
    int rc;
    int has_failure = 0;

    snprintf(command, sizeof(command), "'%s' version", adb_binary);
    if (run_command(command) != 0)
    {
        return -1;
    }

    snprintf(command, sizeof(command), "'%s' connect %s:%s", adb_binary, adb_host, adb_port);
    if (run_command(command) != 0)
    {
        return -1;
    }

    snprintf(command, sizeof(command), "'%s' wait-for-device", adb_binary);
    if (run_command(command) != 0)
    {
        return -1;
    }

    snprintf(command, sizeof(command), "'%s' logcat -c", adb_binary);
    (void)run_command(command);

    snprintf(command, sizeof(command), "'%s' install -r '%s'", adb_binary, apk_path);
    if (run_command(command) != 0)
    {
        return -1;
    }

    snprintf(command, sizeof(command), "'%s' shell am start -W -a com.kryxan.ipcverify.RUN_HEADLESS -n %s --ez autorun true", adb_binary, ACTIVITY_NAME);
    if (run_command(command) != 0)
    {
        return -1;
    }

    sleep(2);
    snprintf(command, sizeof(command), "'%s' logcat -d -s IPCVerify:I *:S", adb_binary);
    rc = capture_command(command, logcat, sizeof(logcat));
    if (rc == 0 && logcat[0] != '\0')
    {
        printf("\n--- adb logcat (IPCVerify) ---\n%s\n", logcat);
        if (strstr(logcat, "failed") || strstr(logcat, "not present"))
        {
            has_failure = 1;
        }
        (void)maybe_write_text_file(repo_root, "bin/adb-ipcverify-logcat.txt", logcat);
    }

    return has_failure ? 1 : 0;
}

int main(int argc, char **argv)
{
    ipcverify_report report;
    char repo_root[PATH_MAX] = "";
    char adb_binary[PATH_MAX] = "";
    char adb_host[128] = DEFAULT_ADB_HOST;
    char adb_port[32] = DEFAULT_ADB_PORT;
    char apk_path[PATH_MAX] = "";
    int assume_yes = 0;
    int skip_android = 0;
    int verify_android_requested = 0;
    int local_rc;
    int verify_android;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--yes") == 0)
        {
            assume_yes = 1;
        }
        else if (strcmp(argv[i], "--verify-android") == 0)
        {
            verify_android_requested = 1;
        }
        else if (strcmp(argv[i], "--no-android") == 0 || strcmp(argv[i], "--local-only") == 0)
        {
            skip_android = 1;
        }
        else if (strcmp(argv[i], "--adb-host") == 0 && i + 1 < argc)
        {
            snprintf(adb_host, sizeof(adb_host), "%s", argv[++i]);
        }
        else if (strcmp(argv[i], "--adb-port") == 0 && i + 1 < argc)
        {
            snprintf(adb_port, sizeof(adb_port), "%s", argv[++i]);
        }
        else if (strcmp(argv[i], "--apk") == 0 && i + 1 < argc)
        {
            snprintf(apk_path, sizeof(apk_path), "%s", argv[++i]);
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            printf("Usage: %s [--yes] [--verify-android] [--no-android|--local-only] [--adb-host HOST] [--adb-port PORT] [--apk PATH]\n", argv[0]);
            return 0;
        }
        else
        {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 2;
        }
    }

    (void)find_repo_root(repo_root, sizeof(repo_root));

    local_rc = ipcverify_run_local_checks(&report);
    printf("%s", report.text);

    verify_android = verify_android_requested || (!skip_android && ask_yes_no(
                                                                       "Should Android container IPC be verified now?", 1, assume_yes));
    if (!verify_android)
    {
        printf("Android container verification skipped. Run ipcverify again later if you want to test ADB flows.\n");
        if (local_rc != 0)
        {
            maybe_create_support_bundle(repo_root);
        }
        return local_rc;
    }

    if (!report.binder_ok)
    {
        if (!ask_yes_no("Local binder checks failed. Is the Android container on another system?", 0, assume_yes))
        {
            printf("ADB verification cancelled because the container was not confirmed as remote.\n");
            maybe_create_support_bundle(repo_root);
            return 1;
        }
    }

    ask_value("ADB host", DEFAULT_ADB_HOST, adb_host, sizeof(adb_host), assume_yes);
    ask_value("ADB port", DEFAULT_ADB_PORT, adb_port, sizeof(adb_port), assume_yes);

    if (!find_adb_binary(repo_root, adb_binary, sizeof(adb_binary)))
    {
        fprintf(stderr, "adb was not found. Install platform-tools or provide ADB=/path/to/adb.\n");
        maybe_create_support_bundle(repo_root);
        return 1;
    }

    if (ensure_android_apk(repo_root, apk_path, sizeof(apk_path)) != 0)
    {
        fprintf(stderr, "The Android APK is missing. Run 'make ipcverify-android' to produce bin/ipcverify.apk after configuring Gradle + ANDROID_SDK_ROOT.\n");
        maybe_create_support_bundle(repo_root);
        return 1;
    }

    if (run_adb_verification(repo_root, adb_binary, adb_host, adb_port, apk_path) != 0)
    {
        fprintf(stderr, "IPC verification reported a failure through adb.\n");
        maybe_create_support_bundle(repo_root);
        return 1;
    }

    printf("ADB-driven IPC verification finished successfully.\n");
    return local_rc;
}
