#ifndef IPCVERIFY_IPC_CHECKS_H
#define IPCVERIFY_IPC_CHECKS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    enum ipcverify_status
    {
        IPCVERIFY_STATUS_PASS = 0,
        IPCVERIFY_STATUS_FAIL,
        IPCVERIFY_STATUS_SKIP,
        IPCVERIFY_STATUS_NOT_PRESENT
    };

    typedef struct ipcverify_report
    {
        char text[32768];
        size_t used;
        int pass_count;
        int fail_count;
        int skip_count;
        int not_present_count;
        int binder_ok;
        int ashmem_ok;
        int memfd_ok;
    } ipcverify_report;

    void ipcverify_report_init(ipcverify_report *report);
    void ipcverify_report_append(ipcverify_report *report, const char *fmt, ...);
    const char *ipcverify_status_text(enum ipcverify_status status);
    void ipcverify_log_check(ipcverify_report *report, const char *label,
                             enum ipcverify_status status, const char *detail);
    int ipcverify_run_local_checks(ipcverify_report *report);
    int ipcverify_has_failures(const ipcverify_report *report);

#ifdef __cplusplus
}
#endif

#endif /* IPCVERIFY_IPC_CHECKS_H */
