# Planned Changes

This file tracks practical follow-up work after the current cross-distro
compatibility push.

## Current Position

1. Modules currently build on Proxmox 9 reference kernel `6.17.13-2-pve`.
2. Compatibility wrappers are in place and functional code mostly avoids direct
   version branching.
3. CI coverage now includes Ubuntu, Debian, Proxmox 8/9, Fedora, Silverblue,
   and RHEL/CentOS Stream tracks.
4. Runtime verification is integrated through `scripts/detect-ipc-runtime.sh`,
   `scripts/verify-environment.sh`, `make verify`, and `make ci-test`.
5. `binder_linux` remains best-effort for live replacement on newer kernels.
   Reboot should be treated as the reliable activation path where unload fails.

## Scope Guardrails

1. `linux-headers-6.17.13-2-pve` is local reference material only and is not
   part of this repository.
2. Preserve clean repository boundaries: reference headers stay untracked.
3. Keep changes testable in small slices and paired with CI + runtime checks.

## Status Review

### 1. Kernel API Resilience Layer: Done

1. Compat wrappers are active in `binder/compat.h` and `ashmem/compat.h`.
2. Functional code paths are using wrapper macros for VM flags, shrinkers,
   rename ABI split, and mmap-lock churn.
3. Remaining version checks are structural in helper/backfill files.

### 2. Build, Packaging, and Validation: Done (expanded)

1. Existing CI workflows cover Ubuntu, Debian, Proxmox 8/9.
2. Additional workflows now cover Fedora, Silverblue, and RHEL/CentOS Stream.
3. Runtime verification runs in CI through `make ci-test`.
4. Artifact upload covers built `.ko` outputs including optional `binderfs.ko`
   if that object exists as a separate module in a given configuration.

### 3. Documentation and Operator Guidance: Done (current cycle)

1. README now includes distro-specific header guidance.
2. Runtime detector usage and strict CI mode are documented.
3. Binder reboot-bound behavior is documented as an operator expectation.

### 4. Memory Management Modernization: Partially Done

Already implemented:

1. `ashmem_create_backing_file()` abstraction with shmem and memfd-compatible
   backing modes.
2. `ashmem_backing_mode` module parameter and debugfs-backed observability.
3. VM-flag wrapper usage to reduce direct MM internals coupling.
4. Cross-kernel shrinker registration wrappers to absorb API signature drift.

Practicality check:

1. No direct `get_user_pages()` or `pin_user_pages()` usage is present in the
   current binder/ashmem trees, so that migration item is not actionable now.
2. The most practical next work is improving memfd-backed behavior semantics,
   not page-pinning conversion.

## Next Memory Modernization Phase

### Phase M1: Memfd semantics and safety

1. Add explicit memfd-mode behavior tests for pin/unpin and purge semantics.
2. Introduce compat wrappers for seal operations where available.
3. Add strict assertions around mode transitions and fallback behavior.

Exit criteria:

1. `ashmem_backing_mode=1` passes userspace ashmem tests.
2. No regressions in `ashmem_backing_mode=0` legacy path.
3. CI evidence captured across at least Proxmox 9 and one Fedora/RHEL family run.

### Phase M2: Pressure and reclaim behavior

1. Add targeted reclaim stress checks in `test/` for both backing modes.
2. Track shrinker activity in debugfs stats for easier regression spotting.
3. Validate that reclaim paths do not regress container workload startup.

Exit criteria:

1. Reclaim stress tests complete without kernel warnings.
2. Debug signals are sufficient to root-cause reclaim failures quickly.

### Phase M3: Controlled feature extension

1. Evaluate whether seal ioctls should be introduced behind module parameter
   guardrails.
2. Keep backward-compatible ashmem interface as default.
3. Gate new semantics with CI and explicit doc updates.

## Validation Resources

1. Local Proxmox 9 SSH host (`root@vostro`) remains a primary validation target
   for build, reboot, and runtime checks.
2. GitHub workflows provide cross-distro drift detection and baseline confidence.
3. Local and CI validations should stay aligned by using `make ci-test` and the
   same runtime detection script.

## Additional Improvement Opportunities

1. Add workflow-level caching for kernel headers and package indexes.
2. Add BTF-aware validation path when `vmlinux` is available.
3. Add a small CI summary artifact containing detector JSON output per distro.
4. Add explicit secure-boot module-signing validation notes and optional helper.
5. Add a compact runbook for "headers found but ABI mismatch" troubleshooting.

## Working Sequence (Recommended)

1. Preserve compatibility baseline and current CI green state.
2. Execute Phase M1 and document behavior deltas.
3. Execute Phase M2 with stress coverage.
4. Reassess Phase M3 based on observed workload benefit.

## Branch and Process Note

Planning remains a backlog, not a promise. Keep implementation slices small,
cross-validated, and documentation-first for any kernel-facing behavior changes.
