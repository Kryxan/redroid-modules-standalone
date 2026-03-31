# Planned Changes

This file tracks broad follow-up ideas for the local `local/proxmox-6.17-roadmap` branch.
It is a refinement backlog, not a committed roadmap.

## Current Baseline

- `binder_linux` and `ashmem_linux` now compile for Proxmox 9 / Linux `6.17.13-2-pve`.
- Recent work focused on kernel API compatibility and safer module install flows.
- `binder_linux` appears effectively reboot-bound on this kernel when already loaded: unload may fail even with a zero visible refcount.
- The current state should be treated as a stable checkpoint to preserve while follow-up work is planned.

## Planning Principles

- Prefer compatibility wrappers over scattering `#if LINUX_VERSION_CODE` checks across functional code.
- Keep runtime behavior stable before attempting larger refactors.
- Separate "compile on new kernels" work from "architectural modernization" work.
- Treat binder live-reload support as best-effort only on kernels where unload is unreliable.
- Add documentation and verification steps alongside non-trivial kernel-facing changes.

## Near-Term Priorities

### 1. Kernel API Resilience Layer  ✅ DONE

Implemented via `binder/compat.h` and `ashmem/compat.h`.  All
`LINUX_VERSION_CODE` checks have been moved out of functional source files
(`binder.c`, `binderfs.c`, `binder_alloc.c`, `binder_alloc.h`,
`binder_internal.h`, `ashmem.c`) into the compat headers.  Only `deps.c`
and `idr.c` retain version checks (structural to their purpose of providing
ABI-compatible kallsyms wrappers and backfills).

See [docs/KERNEL_COMPAT.md](KERNEL_COMPAT.md) for the full API table.

### 2. Build and DKMS Hardening  ✅ DONE

- `make verify` / `make info` targets added to both sub-Makefiles and
  top-level — report kernel release, header presence, install state, and
  loaded state.
- Install / reload / uninstall targets already idempotent (prior session).
- `load_modules.sh` updated to prefer `modprobe` with `insmod` fallback,
  added already-loaded check, binderfs mount section, module status report.
- DKMS configs updated: version `"2.0.0"`, `DEST_MODULE_LOCATION="/extra"`,
  `REMAKE_INITRD="no"`.
- `redroid.conf` and `99-redroid.rules` improved (binder device rules added).
- `test/Makefile` created; `test/ashmem_test.c` rewritten as 12-test suite
  (all passing on 6.17.13-2-pve).

Remaining:

- `make test-build` against multiple header trees.
- Shared Makefile include fragments if duplication grows.

### 3. Documentation and Operator Guidance  ✅ PARTIALLY DONE

- `docs/KERNEL_COMPAT.md` created — kernel compatibility table and
  "how to adapt to a new kernel" guide.
- `deploy/k8s/base/daemonset.yaml` rewritten from placeholder to functional
  template (privileged initContainer w/ apt/yum build, hostPID, volume mounts).
- All 4 kustomize overlays fixed (`bases:` → `resources:`).

Remaining:

- "how to test binderfs" guide.
- "how to validate ashmem" guide.
- Document binder unload limitations on newer Proxmox kernels.

## Medium-Term Work

### 4. Memory Management Modernization  ✅ PARTIALLY DONE

Done:

- `compat_vm_flags_clear()` added to `ashmem/compat.h` (parallels
  `binder/compat.h`); bare `vm_flags_clear()` call in `ashmem.c` replaced.
- Ashmem backing store abstracted via `ashmem_create_backing_file()` factory
  function.  Two backing modes implemented: `ASHMEM_BACKING_SHMEM` (default,
  original behavior) and `ASHMEM_BACKING_MEMFD` (VM_NORESERVE, deferred swap
  accounting, foundation for file sealing).
- `ashmem_backing_mode` module parameter added (0=shmem, 1=memfd-compatible).
- `struct ashmem_area` extended with `backing` field and `area_entry` for
  global region tracking.

Primary goal: reduce reliance on older MM assumptions that will continue to drift.

Candidate work:

- Audit `get_user_pages()` usage and replace with `pin_user_pages()` where appropriate.
- Review VMA operations for `6.12+` behavior changes.
- Introduce compatibility macros for recurring `vm_flags` churn.
- Avoid direct access to `vm_area_struct` internals where helper APIs exist.
- Audit folio-related changes where page-centric assumptions are likely to age poorly.

Notes:

- This is higher risk than pure API wrapping and should be broken into small, testable changes.
- Refactors should follow observed kernel drift, not assumed future churn.

### 5. Runtime Instrumentation and Debugging  ✅ PARTIALLY DONE

Primary goal: improve visibility when things compile but behave incorrectly at runtime.

Done:

- `alloc_debug_mask` exposed as module parameter (was inaccessible static var).
- NULL-pointer safety added to all kallsyms wrapper functions in both `deps.c`
  files — `WARN_ON_ONCE` + safe return values prevent silent crashes.
- `// HACKED` comments replaced with descriptive explanations throughout.
- `ashmem_debug_mask` module parameter added (bitmask: 1=open/close, 2=mmap,
  4=pin/unpin, 8=shrink).  Debug logging via `ashmem_debug()` macro.
- debugfs directory `/sys/kernel/debug/ashmem/` created with two files:
  - `regions` — lists all active ashmem areas (name, size, backing, pin status).
  - `stats` — aggregate statistics (regions, total size, mapped, backing
    counts, lru_count, current backing mode).
- `ashmem_show_fdinfo()` extended to report backing type per region.

Candidate work:

- Add or extend binder transaction tracepoints.
- Add binderfs mount options for selected debugging or policy behavior.
- Consider memory limit knobs where they map cleanly to module semantics.

Definition of done:

- Runtime failures should become easier to triage without recompiling the modules.

## Long-Term Exploration

### 6. Additional Android IPC Pieces  ✅ FOUNDATION LAID

**Design decisions (resolved):**

- ashmem continues to function as-is for backward compatibility.
- ashmem's architecture now supports switching backing stores: the
  `ashmem_create_backing_file()` abstraction dispatches to either
  legacy shmem or memfd-compatible mode via module parameter.
- Future memfd integration will add file-sealing support to the memfd
  backing path, allowing ashmem to act as a compatibility wrapper that
  translates legacy pin/unpin semantics to modern memfd/seal operations.
- Even though Android is deprecating ashmem in favor of memfd, many
  container workloads (redroid) still require it.  The wrapper design
  keeps the ashmem ioctl interface stable while modernizing internals.

**Remaining directions:**

- Implement file-sealing ioctls (`ASHMEM_SET_SEAL` / `ASHMEM_GET_SEAL`)
  for memfd-backed regions, translating to `shmem_set_seals()` /
  `shmem_get_seals()` resolved via kallsyms.
- Map ashmem pin/unpin semantics to memfd-appropriate operations
  (e.g., `MADV_DONTNEED` on unpin for memfd backing).
- A more modular binder suite with clearer split points such as alloc, transaction, and debug support.
- Optional helpers for environments where binderfs exists but binder IPC support is incomplete.

Notes:

- Builds of binder and ashmem should be linked (shared CI, shared compat
  conventions) but not dependent on each other's compilation.
- The compat layer remains module-local until patterns stabilize, then
  shared headers may be extracted.

### 7. Full Android IPC Compatibility Layer

Stretch goal:

- Unified `/dev/android-ipc` style entry point.
- Automatic binderfs mount and binder device creation.
- Automatic load orchestration for binder, ashmem, and future memfd compatibility pieces.
- A systemd unit or equivalent bootstrap flow for container-heavy environments.

Notes:

- This is a product-level feature set and should remain explicitly separate from the current kernel-compatibility maintenance track.

### 8. CI/CD and Release Automation  ✅ DONE (foundation)

Implemented in `.github/`:

**Workflows:**

- `build.yml` — Matrix build across Ubuntu 22.04/24.04, Debian 12/trixie.
  Compiles both modules, runs `modinfo` verification, builds userspace tests.
  Supports `workflow_dispatch` with custom kernel-headers package.
- `kernel-check.yml` — Weekly scheduled check of Ubuntu, Debian, and Proxmox
  repos for new kernel-header packages.  Opens/updates GitHub issues with
  `kernel-update` label when new versions detected.  Triggers test builds.
- `dkms-package.yml` — Builds DKMS `.deb` packages on version tags (`v*`).
  Uploads to GitHub Releases.
- `analyze.yml` — Sparse static analysis, stray `LINUX_VERSION_CODE` guard
  detection, compat header orphan macro check.

**Scripts:**

- `.github/scripts/check-compat.sh` — Validates all compat macros are consumed.
- `.github/scripts/check-kernels.sh` — Queries distro repos for new kernels.
- `.github/scripts/dkms-package.sh` — Produces `redroid-{binder,ashmem}-dkms_VERSION_all.deb`.

**DKMS hardening:**

- Pre-build hooks validate header presence and completeness.
- Post-build hooks verify `vermagic` matches target kernel.
- `.gitattributes` enforces LF line endings for all scripts and source.

**Supporting files:**

- `.github/kernel-versions.json` — Tracks known/tested kernel versions.
- Top-level Makefile: `ci-build` and `ci-check` targets.

Remaining:

- Kernel-header caching (GitHub Actions cache) for faster CI runs.
- BTF validation when `vmlinux` is available.
- Release notes generation from compat header diffs.

## Suggested Sequencing

If time is limited, the safest order is:

1. Kernel API compatibility layer.
2. Documentation for current install and validation flows.
3. Build verification helpers.
4. MM modernization in small audited slices.
5. Runtime instrumentation.
6. New Android IPC compatibility features.

## Open Questions (Resolved)

- **Compat layer sharing:** Remain module-local until patterns stabilize.
  Both `binder/compat.h` and `ashmem/compat.h` follow the same conventions;
  a shared header can be extracted later once the macro surface area settles.
- **Binder unload failure:** Likely a binderfs interaction on newer kernels.
  Treated as best-effort; documented in operator notes.
- **Memfd compatibility:** In scope as an additional layer within this
  repository.  ashmem continues to function as-is for backward compatibility.
  The backing abstraction (`ashmem_create_backing_file`) provides the
  extension point for memfd-style behavior without breaking existing users.
  ashmem is deprecated even for Android, but still widely used in container
  workloads — the wrapper pattern ensures a smooth transition path.
- **CI targets:** Distro kernels primarily (Ubuntu, Debian, Proxmox).
  Mainline can be added as the compat layer matures.

## Branch Note

Planning work was split onto a local-only branch for safety:

- `local/proxmox-6.17-roadmap`
