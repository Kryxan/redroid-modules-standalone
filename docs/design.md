# Design

This document is the **authoritative description of the current as-built design**. Future-only items belong in `docs/PLANNED_CHANGES.md`.

## High-Level Architecture

| Concern | Current implementation |
| --- | --- |
| Presence / runtime detection service | `scripts/detect-ipc-runtime.sh` and `scripts/verify-environment.sh` |
| Kernel module suite | `binder/` and `ashmem/` |
| Kernel compatibility layer | `binder/compat.h`, `ashmem/compat.h`, `binder/deps.c`, `ashmem/deps.c` |
| Installer architecture | `scripts/build-release-bundle.sh` and `packaging/run-installer/install.sh` |
| Verification subsystem | `ipcverify/` (`ipcverify-host` + `ipcverify.apk`) |
| CI/CD workflows | `.github/workflows/*.yml` |
| Reusable composite actions | `.github/actions/deb-dkms-build/action.yml` and `.github/actions/rpm-dkms-build/action.yml` |

## Current Component Design

### 1. Presence / runtime detection

The repo does not currently ship a long-running daemon. Instead, the “presence service” is a script-based detector that answers:

- is `binderfs` supported?
- can it be mounted?
- are Binder devices present and usable?
- does Ashmem mapping work, or should the system rely on memfd fallback?

This logic lives in:

- `scripts/detect-ipc-runtime.sh`
- `scripts/verify-environment.sh`

### 2. Kernel modules

The core out-of-tree modules are:

- `binder_linux`
- `ashmem_linux`

Each module carries its own DKMS metadata and build rules in its own directory. There is **no separate top-level `dkms/` folder**; the module trees themselves are the DKMS source of truth.

### 3. Kernel compatibility layer

Cross-kernel compatibility is centralized in:

- `binder/compat.h`
- `ashmem/compat.h`

and supporting dependency glue in `deps.c`. This keeps the functional implementation files mostly free of direct kernel-version branching.

### 3a. Ashmem memory-modernization state

The current ashmem implementation already includes the following merged behavior:

- `ashmem_create_backing_file()` to dispatch between legacy shmem and memfd-compatible backing creation
- `ashmem_backing_mode` as a module parameter with a backward-compatible default of `0=shmem`
- ashmem debugfs observability under `/sys/kernel/debug/ashmem/regions` and `/sys/kernel/debug/ashmem/stats`
- a debug mask covering open/close, mmap, pin/unpin, and shrink paths
- VM-flag and shrinker compatibility wrappers in `ashmem/compat.h`

What is **not** fully implemented yet:

- explicit memfd-mode behavior tests for purge semantics and mode transitions
- seal-operation compatibility helpers and any seal-ioctl extension work
- reclaim stress validation and shrinker-activity counters aimed specifically at the memfd-backed path

### 4. Installer architecture

The installer has two halves:

1. `scripts/build-release-bundle.sh` assembles the release tree and `.run` artifact.
2. `packaging/run-installer/install.sh` performs the runtime installation.

Current policy:

- install matching workflow-built prebuilt modules first
- use DKMS only as fallback when no exact prebuilt match exists
- run verification after install unless the operator explicitly skips it

### 5. `ipcverify`

`ipcverify` is an idempotent verification layer that can be invoked by the installer or run independently. The host-side binary validates binderfs, Binder, and Ashmem behavior; the Android APK supports optional `adb`-driven verification from a running Android container/device.

### 6. CI/CD and composite actions

The current workflow set includes:

- release / prebuilt packaging
- kernel rebuild / prerelease refresh
- Fedora validation
- Fedora Silverblue validation
- RHEL / CentOS Stream validation
- Amazon Linux 2 validation
- Amazon Linux 2023 validation
- version bump / semantic release helpers

The reusable RPM-family path is already implemented in:

```text
.github/actions/rpm-dkms-build/action.yml
```

## Current Release and Validation Matrix

| Track | Current purpose |
| --- | --- |
| Ubuntu 24.04, Debian 12/13, Proxmox 8/9 | workflow-built prebuilt release assets plus Debian-family DKMS validation wiring |
| Fedora, Silverblue, RHEL / CentOS Stream, Amazon Linux 2/2023 | DKMS validation evidence and artifact/log capture |

## What Is Already Implemented

- compatibility wrappers for Binder and Ashmem across multiple kernel generations
- `ashmem_create_backing_file()` and `ashmem_backing_mode` with a backward-compatible shmem default
- ashmem debugfs observability for `regions` / `stats` plus debug-mask tracing
- script-based runtime detection for binderfs / Binder / Ashmem presence
- self-extracting `.run` release packaging
- prebuilt-first installation with DKMS fallback only when needed
- `ipcverify-host` and `ipcverify.apk` integration
- reusable Debian-family and RPM-family DKMS composite actions for validation workflows

## Current Directory Layout

The repo is intentionally organized around the actual subsystems:

```text
redroid-modules-standalone/
├── binder/
├── ashmem/
├── ipcverify/
├── packaging/run-installer/
├── scripts/
├── docs/
├── .github/workflows/
└── .github/actions/
```

Key implications:

- there is no separate `installer/` root; installer runtime code lives in `packaging/run-installer/`
- there is no separate `presence/` service directory; runtime detection is script-based today
- there is no standalone `ipcverify` version file; it ships as part of the repo release version

## Known Limitations

1. Release-prebuilt coverage is still centered on Debian / Ubuntu / Proxmox; RPM families are validated in CI but are not yet all part of the prebuilt release matrix.
2. Some kernels do not allow clean Binder live replacement, so reboot remains the reliable activation path.
3. Exact header availability can differ sharply across rpm-ostree, RHEL-like, and Amazon Linux environments.
4. Secure Boot hosts may still need module-signing enrollment outside the scope of this repo.

## Design Principles

1. **Single codebase, broad kernel spread** via compatibility wrappers.
2. **Prebuilt-first installs** with DKMS as the fallback safety net.
3. **Idempotent verification** through `ipcverify` and the runtime detector.
4. **Deterministic packaging** with `VERSION`, manifests, and checksums.
5. **CI evidence before claims**: distro support should be reflected by successful workflow runs and reproducible artifacts.
