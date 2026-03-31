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

/* ------------------------------------------------------------------ */
/* MM: mm_get_unmapped_area added in 6.0, replacing direct callback   */
/* ------------------------------------------------------------------ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
#define compat_get_unmapped_area(mm, file, addr, len, pgoff, flags) \
    mm_get_unmapped_area(mm, file, addr, len, pgoff, flags)
#else
#define compat_get_unmapped_area(mm, file, addr, len, pgoff, flags) \
    (mm)->get_unmapped_area(file, addr, len, pgoff, flags)
#endif

/* ------------------------------------------------------------------ */
/* Shrinker API: dynamic allocation in 6.0+                           */
/* ------------------------------------------------------------------ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
#define COMPAT_SHRINKER_DYNAMIC 1
#else
#define COMPAT_SHRINKER_DYNAMIC 0
#endif

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
#define compat_vm_flags_clear(vma, flags) \
    do                                    \
    {                                     \
        (vma)->vm_flags &= ~(flags);      \
    } while (0)
#endif

#endif /* _ASHMEM_COMPAT_H */
