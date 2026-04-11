# Kernel Compatibility Layer

This project keeps its Binder and Ashmem sources buildable across older and newer kernels by concentrating API drift in small compatibility shims instead of scattering `#ifdef` logic through the implementation files.

## Key Compatibility Files

| File | Purpose |
| --- | --- |
| `binder/compat.h` | Binder-facing shims for allocator, VFS, binderfs, and kernel API drift |
| `ashmem/compat.h` | Ashmem-facing shims, especially `vm_flags_*()` handling and mm changes |
| `binder/deps.c` / `ashmem/deps.c` | Dependency and symbol-presence glue for external-module builds |

When a new kernel release breaks an API, the rule is to extend the relevant `compat.h` first rather than forking functional logic.

## API Compatibility Table

### `binder/compat.h`

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

### `ashmem/compat.h`

| Macro / function | Since | Description |
| --- | --- | --- |
| `compat_get_unmapped_area()` | n/a | Uses VMA callback path for cross-distro stability |
| `COMPAT_SHRINKER_DYNAMIC` | 6.0 | Dynamic shrinker alloc flag |
| `compat_register_shrinker()` | 6.0 | Handles `register_shrinker` signature differences |
| `compat_unregister_shrinker()` | n/a | Uniform `unregister_shrinker` entry point |
| `compat_vm_flags_clear()` | 6.3 | `vm_flags_clear` vs direct assignment |
| `vma_set_anonymous()` | 4.18 | Backfill |

### `deps.c` in both modules

The `deps.c` files use `kallsyms_lookup_name` (via kprobe on 5.7+) to resolve non-exported kernel symbols at runtime. Those checks are structural and remain the right place for symbol-name discovery logic.

## Runtime Detection and Presence Checks

The current “presence service” is script-based rather than daemonized:

- `scripts/detect-ipc-runtime.sh`
- `scripts/verify-environment.sh`

These checks drive CI summaries, support bundles, and local verification. They currently probe:

- whether `binderfs` is supported by the running kernel
- whether `binderfs` can be mounted (or is already mounted)
- whether Binder device nodes are present (`/dev/binder`, `/dev/binderfs/*`)
- whether Binder ioctl interactions succeed
- whether Ashmem mmap behavior is available, or whether the host is expected to use memfd fallback

Examples:

```bash
bash scripts/detect-ipc-runtime.sh --format json
bash scripts/detect-ipc-runtime.sh --format keyvalue
bash scripts/detect-ipc-runtime.sh --strict --format keyvalue
```

`--strict` exits non-zero if required runtime capabilities are missing.

## Header Resolution Logic

### Debian / Ubuntu / Proxmox families

The release workflows and manual builds prefer the running-kernel header tree first:

1. `/lib/modules/<uname -r>/build`
2. distro-provided `linux-headers-*`
3. Proxmox-specific `pve-headers` / `proxmox-headers`

Proxmox kernels must use the Proxmox header packages; generic Debian headers are not enough.

### RPM families

The shared `.github/actions/rpm-dkms-build/action.yml` path standardizes header resolution for:

- Fedora
- Fedora Silverblue
- RHEL / UBI / CentOS Stream
- Amazon Linux 2
- Amazon Linux 2023

That action prefers the exact `kernel-devel-$(uname -r)` match, falls back to the closest available header tree when required, and records logs/artifacts when distro repositories lag the running kernel.

## Distro-Specific Quirks

- **Fedora / Silverblue:** some GitHub-hosted kernels can expose non-actionable external-module mismatches such as symbol export differences.
- **RHEL / CentOS Stream:** enterprise repo enablement and mirror availability can affect exact header resolution.
- **Amazon Linux 2 vs 2023:** the header and toolchain paths differ enough that they are handled as separate CI targets; AL2023 currently prefers the `6.1.*` branch when exact headers are not present.
- **Secure Boot:** external modules may still require MOK enrollment before they can load.

## Notes for the Proxmox Baseline

1. `linux-headers-6.17.13-2-pve` is used as a local build reference.
2. Treat Binder live replacement as best-effort only on kernels where unload is unreliable; reboot is the stable activation path.
3. Prefer validating behavior with `make ci-test` and `scripts/detect-ipc-runtime.sh --strict` after a module build.

## Minimum Supported Kernel

The oldest guards target **4.16** (`DEFINE_SHOW_ATTRIBUTE`) and **4.18** (`vma_set_anonymous`). In practice the modules are validated on **5.x – 6.17+**.
