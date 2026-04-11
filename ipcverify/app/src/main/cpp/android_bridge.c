#include <jni.h>
#include <android/log.h>
#include <string.h>

#include "ipc_checks.h"

#define IPCVERIFY_TAG "IPCVerify"

static void log_report_lines(const char *text)
{
    const char *cursor = text;

    while (cursor && *cursor)
    {
        const char *end = strchr(cursor, '\n');
        if (!end)
        {
            __android_log_print(ANDROID_LOG_INFO, IPCVERIFY_TAG, "%s", cursor);
            break;
        }

        if (end > cursor)
        {
            char line[512];
            size_t length = (size_t)(end - cursor);
            if (length >= sizeof(line))
            {
                length = sizeof(line) - 1U;
            }
            memcpy(line, cursor, length);
            line[length] = '\0';
            __android_log_print(ANDROID_LOG_INFO, IPCVERIFY_TAG, "%s", line);
        }
        cursor = end + 1;
    }
}

JNIEXPORT jstring JNICALL
Java_com_kryxan_ipcverify_NativeBridge_runNativeChecks(JNIEnv *env, jclass clazz)
{
    ipcverify_report report;
    (void)clazz;

    ipcverify_run_local_checks(&report);
    log_report_lines(report.text);
    return (*env)->NewStringUTF(env, report.text);
}
