/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Kernel version compatibility layer for binder and binderfs.
 *
 * Centralizes all LINUX_VERSION_CODE guards behind stable local names.
 * When a new kernel breaks an API, fix it here instead of scattering
 * #if directives across functional code.
 */

#ifndef _BINDER_COMPAT_H
#define _BINDER_COMPAT_H

#include <linux/version.h>
#include <linux/fs.h>

/* ------------------------------------------------------------------ */
/* VFS: lookup_one_len -> lookup_one (6.12+)                          */
/* ------------------------------------------------------------------ */
#include <linux/namei.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
#include <linux/mnt_idmapping.h>
static inline struct dentry *compat_lookup_one(struct dentry *parent,
                                               const char *name, size_t len)
{
    struct qstr qname = QSTR_INIT(name, len);

    return lookup_one(&nop_mnt_idmap, &qname, parent);
}
#else
static inline struct dentry *compat_lookup_one(struct dentry *parent,
                                               const char *name, size_t len)
{
    return lookup_one_len(name, parent, len);
}
#endif

/* ------------------------------------------------------------------ */
/* VFS: inode timestamp initialisation (6.6+)                         */
/* ------------------------------------------------------------------ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
#define compat_inode_init_ts(inode) simple_inode_init_ts(inode)
#else
#define compat_inode_init_ts(inode)                 \
    do                                              \
    {                                               \
        (inode)->i_mtime = (inode)->i_atime =       \
            (inode)->i_ctime = current_time(inode); \
    } while (0)
#endif

/* ------------------------------------------------------------------ */
/* VFS: rename callback gained mnt_idmap * in 6.3                     */
/* ------------------------------------------------------------------ */
/*
 * The function signature change for .rename is too structural for a
 * simple wrapper.  Use COMPAT_RENAME_HAS_IDMAP to pick the right
 * implementation at the call site.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
#define COMPAT_RENAME_HAS_IDMAP 1
#else
#define COMPAT_RENAME_HAS_IDMAP 0
#endif

/* ------------------------------------------------------------------ */
/* MM: mmap locking (5.8+)                                            */
/* ------------------------------------------------------------------ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
#define compat_mmap_read_lock(mm) mmap_read_lock(mm)
#define compat_mmap_read_trylock(mm) mmap_read_trylock(mm)
#define compat_mmap_read_unlock(mm) mmap_read_unlock(mm)
#else
#define compat_mmap_read_lock(mm) down_read(&(mm)->mmap_sem)
#define compat_mmap_read_trylock(mm) down_read_trylock(&(mm)->mmap_sem)
#define compat_mmap_read_unlock(mm) up_read(&(mm)->mmap_sem)
#endif

/* ------------------------------------------------------------------ */
/* MM: zap_page_range -> zap_page_range_single (6.3+)                 */
/* ------------------------------------------------------------------ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
#define compat_zap_page_range(vma, addr, size) \
    zap_page_range_single(vma, addr, size, NULL)
#else
/* Uses the kallsyms-resolved wrapper from deps.c */
extern void zap_page_range(struct vm_area_struct *, unsigned long, unsigned long);
#define compat_zap_page_range(vma, addr, size) \
    zap_page_range(vma, addr, size)
#endif

/* ------------------------------------------------------------------ */
/* LRU: list_lru_walk_cb spinlock parameter removed in 6.7            */
/* ------------------------------------------------------------------ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 7, 0)
#define COMPAT_LRU_CB_ARGS \
    struct list_head *item, struct list_lru_one *lru, void *cb_arg
#define COMPAT_LRU_CB_EXTRA_PARAMS /* nothing */
#define compat_lru_unlock()        /* no-op: lock managed internally */
#define compat_lru_lock()          /* no-op */
#else
#define COMPAT_LRU_CB_ARGS                            \
    struct list_head *item, struct list_lru_one *lru, \
        spinlock_t *lock, void *cb_arg
#define COMPAT_LRU_CB_EXTRA_PARAMS spinlock_t *lock,
#define compat_lru_unlock() spin_unlock(lock)
#define compat_lru_lock() spin_lock(lock)
#endif

/* ------------------------------------------------------------------ */
/* LRU: list_lru_add/del gained nid+memcg in 6.0                     */
/* ------------------------------------------------------------------ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
#define compat_list_lru_add(lru, item, page) \
    list_lru_add(lru, item, page_to_nid(page), NULL)
#define compat_list_lru_del(lru, item, page) \
    list_lru_del(lru, item, page_to_nid(page), NULL)
#else
#define compat_list_lru_add(lru, item, page) \
    list_lru_add(lru, item)
#define compat_list_lru_del(lru, item, page) \
    list_lru_del(lru, item)
#endif

/* ------------------------------------------------------------------ */
/* Shrinker API: dynamic allocation in 6.7+                           */
/* (shrinker_alloc/shrinker_free/shrinker_register replaced           */
/*  register_shrinker/unregister_shrinker starting in 6.7)           */
/* ------------------------------------------------------------------ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 7, 0)
#define COMPAT_SHRINKER_DYNAMIC 1
#else
#define COMPAT_SHRINKER_DYNAMIC 0
#endif

/* ------------------------------------------------------------------ */
/* LSM: security context API drift                                    */
/*   6.8+:  struct lsm_context  (.context, .len)  [renamed from       */
/*          lsmcontext in 6.8]                                        */
/*   <6.8:  separate char* + u32                                      */
/* ------------------------------------------------------------------ */
#include <linux/security.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)

/* 6.8+ uses a struct-based API (struct lsm_context, renamed from lsmcontext) */
typedef struct
{
    struct lsm_context inner;
} compat_lsm_ctx_t;

#define COMPAT_LSM_CTX_INIT {{0}}

static inline int compat_secid_to_secctx(u32 secid, compat_lsm_ctx_t *ctx)
{
    return security_secid_to_secctx(secid, &ctx->inner);
}

static inline char *compat_secctx_data(compat_lsm_ctx_t *ctx)
{
    return ctx->inner.context;
}

static inline u32 compat_secctx_len(compat_lsm_ctx_t *ctx)
{
    return ctx->inner.len;
}

static inline void compat_release_secctx(compat_lsm_ctx_t *ctx)
{
    security_release_secctx(&ctx->inner);
}

static inline void compat_task_getsecid(struct task_struct *task, u32 *secid)
{
    /* security_task_getsecid removed in 6.8; secid is always 0 */
    *secid = 0;
}

#define COMPAT_LSM_HAS_CTX 1

#else /* < 6.8 */

typedef struct
{
    char *context;
    u32 len;
} compat_lsm_ctx_t;

#define COMPAT_LSM_CTX_INIT {.context = NULL, .len = 0}

static inline int compat_secid_to_secctx(u32 secid, compat_lsm_ctx_t *ctx)
{
    return security_secid_to_secctx(secid, &ctx->context, &ctx->len);
}

static inline char *compat_secctx_data(compat_lsm_ctx_t *ctx)
{
    return ctx->context;
}

static inline u32 compat_secctx_len(compat_lsm_ctx_t *ctx)
{
    return ctx->len;
}

static inline void compat_release_secctx(compat_lsm_ctx_t *ctx)
{
    security_release_secctx(ctx->context, ctx->len);
}

static inline void compat_task_getsecid(struct task_struct *task, u32 *secid)
{
    security_task_getsecid(task, secid);
}

#define COMPAT_LSM_HAS_CTX 0

#endif /* LINUX_VERSION_CODE >= 6.8 */

/* ------------------------------------------------------------------ */
/* Security binder: cred-based API in 5.15.2+                        */
/* ------------------------------------------------------------------ */
/*
 * In 5.15.2+ security_binder_* functions take const struct cred *
 * instead of struct task_struct *.  struct binder_proc gained a
 * 'cred' member at the same time.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 2)
#define COMPAT_HAS_BINDER_CRED 1
#define COMPAT_BINDER_CRED(proc) ((proc)->cred)
#else
#define COMPAT_HAS_BINDER_CRED 0
#define COMPAT_BINDER_CRED(proc) ((proc)->tsk)
#endif

/* ------------------------------------------------------------------ */
/* task_work_add: TWA_RESUME appeared in 5.11                         */
/* ------------------------------------------------------------------ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
#define COMPAT_TWA_RESUME TWA_RESUME
#else
#define COMPAT_TWA_RESUME true
#endif

/* ------------------------------------------------------------------ */
/* file_close_fd: native in 6.8+, deps.c provides wrapper for <6.8   */
/* ------------------------------------------------------------------ */
/*
 * deps.c defines file_close_fd() for kernels < 6.8 (wrapping the
 * older close_fd_get_file via kallsyms).  Call sites can always use
 * file_close_fd() unconditionally.
 */

/* ------------------------------------------------------------------ */
/* Freezer: removed in 6.1 (integrated into scheduler)               */
/* ------------------------------------------------------------------ */
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
#define compat_freezer_do_not_count() freezer_do_not_count()
#define compat_freezer_count() freezer_count()
#else
#define compat_freezer_do_not_count() /* no-op: scheduler handles freezing */
#define compat_freezer_count()        /* no-op */
#endif

/* ------------------------------------------------------------------ */
/* VM flags: vm_flags_set/vm_flags_clear added in 6.3                 */
/* ------------------------------------------------------------------ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
#define compat_vm_flags_set(vma, flags) vm_flags_set(vma, flags)
#define compat_vm_flags_clear(vma, flags) vm_flags_clear(vma, flags)
#else
#define compat_vm_flags_set(vma, flags) \
    do                                  \
    {                                   \
        (vma)->vm_flags |= (flags);     \
    } while (0)
#define compat_vm_flags_clear(vma, flags) \
    do                                    \
    {                                     \
        (vma)->vm_flags &= ~(flags);      \
    } while (0)
#endif

/* ------------------------------------------------------------------ */
/* DEFINE_SHOW_ATTRIBUTE backfill (available since 4.16)              */
/* ------------------------------------------------------------------ */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)
#define DEFINE_SHOW_ATTRIBUTE(__name)                                \
    static int __name##_open(struct inode *inode, struct file *file) \
    {                                                                \
        return single_open(file, __name##_show, inode->i_private);   \
    }                                                                \
    static const struct file_operations __name##_fops = {            \
        .owner = THIS_MODULE,                                        \
        .open = __name##_open,                                       \
        .read = seq_read,                                            \
        .llseek = seq_lseek,                                         \
        .release = single_release,                                   \
    }
#endif

#endif /* _BINDER_COMPAT_H */
