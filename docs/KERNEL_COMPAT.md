# Kernel Compatibility Layer

This project maintains out-of-tree `binder_linux` and `ashmem_linux` kernel modules
for use with [redroid](https://github.com/remote-android/redroid-doc).  Kernel
internal APIs change frequently; the **compat headers** centralise every
version-gated adaptation in one place so that functional source files stay clean.

## Compat headers

| Header | Scope |
| --- | --- |
| `binder/compat.h` | binder, binderfs, binder_alloc |
| `ashmem/compat.h` | ashmem |

When a new kernel release breaks an API, fix it in the relevant `compat.h`
rather than adding `#if LINUX_VERSION_CODE` blocks to functional code.

## API compatibility table

### binder/compat.h

| Macro / function | Since | Description |
| --- | --- | --- |
| `compat_lookup_one()` | n/a | Normalized to `lookup_one_len()` for broad distro compatibility |
| `compat_inode_init_ts()` | 6.6 | `simple_inode_init_ts()` backfill |
| `COMPAT_RENAME_HAS_IDMAP` | 5.12 | Rename callback uses `mnt_idmap *` on idmapped kernels |
| `compat_mmap_read_lock/trylock/unlock()` | 5.8 | `mmap_sem` → mmap lock API |
| `compat_zap_page_range()` | 6.3 | `zap_page_range` → `zap_page_range_single` |
| `COMPAT_LRU_CB_ARGS` | 6.7 | LRU walk callback lost `spinlock_t *` |
| `compat_lru_lock/unlock()` | 6.7 | Pair with `COMPAT_LRU_CB_ARGS` |
| `compat_list_lru_add/del()` | 6.0 | `list_lru_add/del` gained nid + memcg args |
| `COMPAT_SHRINKER_DYNAMIC` | 6.0 | Dynamic shrinker alloc flag |
| `compat_register_shrinker()` | 6.0 | Handles `register_shrinker` signature differences |
| `compat_unregister_shrinker()` | n/a | Uniform `unregister_shrinker` entry point |
| `compat_lsm_ctx_t` | n/a | Build-stable LSM context shim |
| `compat_secid_to_secctx()` | n/a | No-op secctx shim for backport-heavy kernels |
| `compat_secctx_data/len()` | n/a | Accessors for shim context |
| `compat_release_secctx()` | n/a | No-op release for shim context |
| `compat_task_getsecid()` | n/a | No-op secid shim (sets `0`) |
| `COMPAT_HAS_BINDER_CRED` | 5.15.2 | `security_binder_*` takes `cred` not `task` |
| `COMPAT_BINDER_CRED()` | 5.15.2 | Pick `proc->cred` or `proc->tsk` |
| `COMPAT_TWA_RESUME` | 5.11 | `TWA_RESUME` enum vs `true` |
| `compat_freezer_do_not_count/count()` | 6.1 | Freezer removed (integrated into scheduler) |
| `compat_vm_flags_set/clear()` | 6.3 | `vm_flags_set/clear` vs direct assignment |
| `DEFINE_SHOW_ATTRIBUTE` | 4.16 | Backfill for very old kernels |

### ashmem/compat.h

| Macro / function | Since | Description |
| --- | --- | --- |
| `compat_get_unmapped_area()` | n/a | Uses VMA callback path for cross-distro stability |
| `COMPAT_SHRINKER_DYNAMIC` | 6.0 | Dynamic shrinker alloc flag |
| `compat_register_shrinker()` | 6.0 | Handles `register_shrinker` signature differences |
| `compat_unregister_shrinker()` | n/a | Uniform `unregister_shrinker` entry point |
| `compat_vm_flags_clear()` | 6.3 | `vm_flags_clear` vs direct assignment |
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

## Notes for Proxmox 9 baseline

1. `linux-headers-6.17.13-2-pve` is used as a local build reference.
2. Treat binder live replacement as best-effort only on kernels where unload
 is unreliable; use reboot as the stable activation path.
3. Prefer validating behavior with `make ci-test` and
 `scripts/detect-ipc-runtime.sh --strict` after module build.

## Minimum supported kernel

The oldest guards target **4.16** (DEFINE_SHOW_ATTRIBUTE) and **4.18**
(vma_set_anonymous).  In practice the modules are tested on **5.x – 6.17+**.
