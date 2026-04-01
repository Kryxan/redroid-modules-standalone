/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Kernel version compatibility layer for ashmem.
 *
 * Centralizes kernel-version guards behind stable local names.
 * Fix new kernel API breakages here instead of scattering #if
 * directives across functional code.
 */

#ifndef _ASHMEM_COMPAT_H
#define _ASHMEM_COMPAT_H

#include <linux/version.h>
#include <linux/mm.h>
#include <linux/shrinker.h>

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
/* Shrinker registration helper: signature differs across kernels      */
/* ------------------------------------------------------------------ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 7, 0)
#define compat_register_shrinker(shrinker, name) 0
#else
static inline int compat_register_shrinker_impl(struct shrinker *shrinker,
                                                const char *name)
{
    typedef int (*register_shrinker_one_t)(struct shrinker *);
    typedef int (*register_shrinker_two_t)(struct shrinker *, const char *, ...);

    if (__builtin_types_compatible_p(typeof(&register_shrinker), register_shrinker_two_t))
        return ((register_shrinker_two_t)register_shrinker)(shrinker, name);

    return ((register_shrinker_one_t)register_shrinker)(shrinker);
}

#define compat_register_shrinker(shrinker, name) \
    compat_register_shrinker_impl((shrinker), (name))
#endif

#define compat_unregister_shrinker(shrinker) unregister_shrinker((shrinker))

/* ------------------------------------------------------------------ */
/* VMA: vma_set_anonymous added in 4.18                               */
/* ------------------------------------------------------------------ */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0)
static inline void vma_set_anonymous(struct vm_area_struct *vma)
{
    vma->vm_ops = NULL;
}
#endif

/* ------------------------------------------------------------------ */
/* VM flags: vm_flags_set/vm_flags_clear added in 6.3                 */
/* ------------------------------------------------------------------ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
#define compat_vm_flags_clear(vma, flags) vm_flags_clear(vma, flags)
#else
#define compat_vm_flags_clear(vma, flags) do { } while (0)
#endif

#endif /* _ASHMEM_COMPAT_H */
