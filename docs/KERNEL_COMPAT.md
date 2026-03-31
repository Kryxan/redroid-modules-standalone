# Kernel Compatibility Layer

This project maintains out-of-tree `binder_linux` and `ashmem_linux` kernel modules
for use with [redroid](https://github.com/remote-android/redroid-doc).  Kernel
internal APIs change frequently; the **compat headers** centralise every
version-gated adaptation in one place so that functional source files stay clean.

## Compat headers

| Header | Scope |
|---|---|
| `binder/compat.h` | binder, binderfs, binder_alloc |
| `ashmem/compat.h` | ashmem |

When a new kernel release breaks an API, fix it in the relevant `compat.h`
rather than adding `#if LINUX_VERSION_CODE` blocks to functional code.

## API compatibility table

### binder/compat.h

| Macro / function | Since | Description |
|---|---|---|
| `compat_lookup_one()` | 6.12 | `lookup_one_len()` → `lookup_one()` |
| `compat_inode_init_ts()` | 6.6 | `simple_inode_init_ts()` backfill |
| `COMPAT_RENAME_HAS_IDMAP` | 6.3 | Rename callback gained `mnt_idmap *` |
| `compat_mmap_read_lock/trylock/unlock()` | 5.8 | `mmap_sem` → mmap lock API |
| `compat_zap_page_range()` | 6.3 | `zap_page_range` → `zap_page_range_single` |
| `COMPAT_LRU_CB_ARGS` | 6.7 | LRU walk callback lost `spinlock_t *` |
| `compat_lru_lock/unlock()` | 6.7 | Pair with `COMPAT_LRU_CB_ARGS` |
| `compat_list_lru_add/del()` | 6.0 | `list_lru_add/del` gained nid + memcg args |
| `COMPAT_SHRINKER_DYNAMIC` | 6.0 | Dynamic shrinker alloc flag |
| `compat_lsm_ctx_t` | 6.8/6.12 | Unified LSM security context type |
| `compat_secid_to_secctx()` | 6.8 | Unified secid → secctx |
| `compat_secctx_data/len()` | 6.8 | Accessors for LSM context |
| `compat_release_secctx()` | 6.8 | Release LSM context |
| `compat_task_getsecid()` | 6.8 | `security_task_getsecid` removed |
| `COMPAT_HAS_BINDER_CRED` | 5.15.2 | `security_binder_*` takes `cred` not `task` |
| `COMPAT_BINDER_CRED()` | 5.15.2 | Pick `proc->cred` or `proc->tsk` |
| `COMPAT_TWA_RESUME` | 5.11 | `TWA_RESUME` enum vs `true` |
| `compat_freezer_do_not_count/count()` | 6.1 | Freezer removed (integrated into scheduler) |
| `compat_vm_flags_set/clear()` | 6.3 | `vm_flags_set/clear` vs direct assignment |
| `DEFINE_SHOW_ATTRIBUTE` | 4.16 | Backfill for very old kernels |

### ashmem/compat.h

| Macro / function | Since | Description |
|---|---|---|
| `compat_get_unmapped_area()` | 6.0 | `mm_get_unmapped_area` vs callback |
| `COMPAT_SHRINKER_DYNAMIC` | 6.0 | Dynamic shrinker alloc flag |
| `vma_set_anonymous()` | 4.18 | Backfill |

### deps.c (both modules)

The `deps.c` files use `kallsyms_lookup_name` (via kprobe on 5.7+) to resolve
non-exported kernel symbols at runtime.  Version checks in `deps.c` are
structural — they define which symbol name or function signature to look up and
are **not** candidates for the compat headers.

## How to adapt to a new kernel

1. **Build** against the new kernel headers: `make KDIR=/path/to/headers`.
2. If compilation fails, identify the broken API.
3. Add a new compat entry in the appropriate `compat.h`.
4. Replace the raw API call in functional code with the compat wrapper.
5. Update this table.

## Minimum supported kernel

The oldest guards target **4.16** (DEFINE_SHOW_ATTRIBUTE) and **4.18**
(vma_set_anonymous).  In practice the modules are tested on **5.x – 6.17+**.
