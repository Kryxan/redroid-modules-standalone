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

1. `linux-headers-6.17.13-2-pve` is used as local reference headers.
2. Preserve clean repository boundaries: reference headers stay untracked.
3. Keep changes testable in small slices and paired with CI + runtime checks.
4. Keep binder/ashmem semantics compatible with in‑Android ARM/ARM64 translation layers (e.g., libndk_translatio libarm64translator).
5. No host‑side assumptions that would block ARM‑on‑x86 Android use.

## Status Review

### 1. Kernel API Resilience Layer: Done

1. Compat wrappers are active in `binder/compat.h` and `ashmem/compat.h`.
2. Functional code paths are using wrapper macros for VM flags, shrinkers,
   rename ABI split, and mmap-lock churn.
3. Remaining version checks are structural in helper/backfill files.

### 2. Build, Packaging, and Validation: Partially Done

1. Existing CI workflows cover Ubuntu, Debian, Proxmox 8/9.
2. Additional workflows now cover Fedora, Silverblue, and RHEL/CentOS Stream.
3. Runtime verification runs in CI through `make ci-test`.
4. Artifact upload covers built `.ko` outputs including optional `binderfs.ko`
   if that object exists as a separate module in a given configuration.

### 3. Self-contained installer: Partially Done

1. Goal: Ship a `.run` (or similar) artifact that:
   - Installs prebuilt modules for known kernels.
   - Falls back to DKMS build for unknown kernels.
   - Runs `verify-environment.sh` and `test_ipc` post‑install.
2. CI: Add workflow to:
   - Build per‑kernel `.ko` artifacts.
   - Assemble `.run` installer.
   - Publish as a GitHub Release asset.

### 4. Documentation and Operator Guidance: Ongoing

1. Keep README.md brief. Detailed documentation goes in the `docs` path.
2. BUILD.md now includes distro-specific header guidance.
3. Runtime detector usage and strict CI mode are documented.
4. Binder reboot-bound behavior is documented as an operator expectation.

### 5. Memory Management Modernization: Partially Done

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

### 6. Android Runtime Integration Checks

1. Container runtimesfor minimal “smoke test” recipes.
2. Verify operation ReDroid in Docker.
3. Verify operation LXC/LXD profile.

Exit criteria:

1. Documented “known good” host recipe where: Modules load & Binderfs mounts.
2. A reference Android image boots and reaches `adb shell`.

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

1. Local Proxmox 9 SSH host remains a primary validation target
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
6. Add a `support-bundle` target:
   - Collects: kernel version, module info, detector output, test logs.
   - Produces a single tarball for bug reports.

## Working Sequence (Recommended)

1. Preserve compatibility baseline and current CI green state.
2. Execute Phase M1 and document behavior deltas.
3. Execute Phase M2 with stress coverage.
4. Reassess Phase M3 based on observed workload benefit.
5. Start working on the GPU shims.
6. TBA.

---

## GPU Shim plan

In-Android shim for GPU normalization to provide better GPU support. This is the general plan, not to be a primary goal until after full testing and optimization on the Linux kernel modules has been completed.

### 1. Scope and placement

Where it lives:

- In the project path:

```text
redroid-modules-standalone/
├── README.md
├── binder/
├── ashmem/
├── gpushim/
│   ├── gralloc/
│   ├── hwcomposer/
│   ├── libEGL/
│   ├── Vulkan/
```

- Inside the Android rootfs, as drop-in HAL/GL/Vulkan shims:
  - `system/lib64/hw/gralloc.normalized.so`
  - `system/lib64/hw/hwcomposer.normalized.so`
  - `system/lib64/egl/libEGL_normalized.so`
  - `system/lib64/egl/libGLESv2_normalized.so`
  - `system/lib64/libvulkan_normalized.so` (optional/phase 2)

- Use `hwservicemanager`/HAL search order + symlinks or `ro.hardware.*` props to point:
  - `gralloc.*` → `gralloc.normalized.so`
  - `hwcomposer.*` → `hwcomposer.normalized.so`
- For EGL/GLES/Vulkan, wrap the vendor libs:
  - `libEGL.so` → thin wrapper that `dlopen()`s host‑mapped Mesa/NVIDIA/AMD libs
  - Same for `libGLESv2.so`, `libvulkan.so`

### 2. Host capability probe (small C helper, run by .run)

Binary on host (installed by `.run`):

- `redroid-gpu-probe` (C, tiny, no deps beyond libdrm/libvulkan)

What it does:

- Enumerates:
  - DRM devices (`/dev/dri/card*`, `/dev/dri/renderD*`)
  - Supported DRM formats + modifiers (via `DRM_IOCTL_MODE_GETRESOURCES` + `DRM_IOCTL_MODE_GETPLANERESOURCES`)
  - Vulkan physical devices + extensions (if available)
- Emits a JSON profile to a known path, e.g.:
  - `/var/lib/redroid/gpu-profile.json`

Example fields:

```json
{
  "primary_node": "/dev/dri/card1",
  "render_node": "/dev/dri/renderD129",
  "drm_formats": ["XR24", "AR24", "NV12"],
  "vulkan": {
    "available": true,
    "apiVersion": "1.3.275",
    "extensions": ["VK_KHR_swapchain", "VK_KHR_external_memory_fd"]
  }
}
```

The `.run` installer:

- Builds and installs `redroid-gpu-probe`
- Runs it once per host
- Optionally bakes the profile path into a ReDroid env var:
  - `ANDROIDBOOT_REDROID_GPU_PROFILE=/var/lib/redroid/gpu-profile.json`

### 3. Android-side normalization shims (C, inside container)

#### 3.1 Gralloc shim (`gralloc.normalized.so`)

Responsibilities:

- Read host GPU profile (via bind-mounted JSON or env var)
- Map Android formats → host DRM formats
- Normalize usage flags (e.g. `HW_TEXTURE`, `HW_COMPOSER`, `PROTECTED`)
- Allocate buffers via:
  - GBM (preferred)
  - or DRM dumb buffers as fallback
- Export/import DMA‑BUF FDs
- Normalize error codes and behavior

Implementation:

- C shared library, implements standard gralloc HAL v3/v4 symbols
- Internally uses:
  - `libdrm`
  - `libgbm` (if present)
- No Android kernel changes required

#### 3.2 HWC shim (`hwcomposer.normalized.so`)

Responsibilities:

- Present a stable HWC2 interface to SurfaceFlinger
- Normalize:
  - Display modes
  - vsync timing
  - composition types (client vs device)
- Use DRM/KMS atomic modesetting where possible
- Fallback to “GPU-only composition” when overlays aren’t viable

Implementation:

- C shared library implementing HWC2
- Uses:
  - DRM/KMS
  - Fences from gralloc shim
- Reads same GPU profile to decide capabilities

#### 3.3 EGL/GLES shim (`libEGL_normalized.so`, `libGLESv2_normalized.so`)

Responsibilities:

- Wrap underlying host EGL/GLES (Mesa/NVIDIA/AMD)
- Normalize:
  - EGLConfigs
  - Error codes
  - Extension exposure (hide host-specific oddities)
- Optionally enforce a “safe subset” of extensions

Implementation:

- C shared libs that:
  - `dlopen("libEGL_host.so")` (symlinked to host-mapped lib)
  - Forward most calls
  - Intercept config/extension queries

#### 3.4 Vulkan shim (Phase 2)

- Same pattern as EGL/GLES:
  - Wrap `vkEnumerateInstanceExtensionProperties`, etc.
  - Expose a stable subset
  - Hide unsupported/unstable extensions

### 4. Integration via `.run` installer

The `.run` installer gains a “GPU integration” phase:

1. Host side:
   - Install `redroid-gpu-probe`
   - Run it → write `/var/lib/redroid/gpu-profile.json`
2. Container image prep (documented + optional helper):
   - Copy shims into Android rootfs:
     - `system/lib64/hw/gralloc.normalized.so`
     - `system/lib64/hw/hwcomposer.normalized.so`
     - `system/lib64/egl/libEGL_normalized.so`
     - `system/lib64/egl/libGLESv2_normalized.so`
   - Adjust props or symlinks:
     - `gralloc.*` → `gralloc.normalized.so`
     - `hwcomposer.*` → `hwcomposer.normalized.so`
     - `libEGL.so` → `libEGL_normalized.so`
3. Runtime:
   - ReDroid container gets:
     - `-v /var/lib/redroid/gpu-profile.json:/vendor/etc/redroid-gpu-profile.json:ro`
   - Shims read that file at init

Waydroid can use the same shims by:

- Bind-mounting the profile
- Dropping the same `.so` files into its system image
- Adjusting its HAL search order

### 5. Phased implementation plan

Phase G1 — Probe + Gralloc shim

- Implement `redroid-gpu-probe` (host)
- Implement `gralloc.normalized.so` (Android)
- Wire into one ReDroid image
- Validate:
  - Intel iGPU on Proxmox 9
  - Basic GLES apps
  - No regressions vs stock

Phase G2 — HWC shim

- Implement `hwcomposer.normalized.so`
- Normalize vsync + modes
- Validate:
  - UI smoothness
  - Multi-instance behavior
  - Thin-client streaming stability

Phase G3 — EGL/GLES normalization

- Implement EGL/GLES wrappers
- Normalize configs + extensions
- Validate:
  - Cross-GPU behavior (Intel vs AMD vs NVIDIA)
  - App compatibility

Phase G4 — Vulkan (optional)

- Implement Vulkan wrapper
- Normalize extension exposure
- Validate with Vulkan test apps

## Branch and Process Note

Planning remains a backlog, not a promise.
