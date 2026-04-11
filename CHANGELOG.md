# Changelog

<!-- markdownlint-disable MD024 -->

This changelog includes **all standalone-project commits that change behavior, support coverage, installer logic, packaging output, CI output, or runtime verification**.

For readability, consecutive patch-only commits may be consolidated under the resulting patch release heading.

## [2.3.0] - 2026-04-10

### Added

- Amazon Linux 2 and Amazon Linux 2023 DKMS validation workflows
- a reusable `.github/actions/deb-dkms-build/action.yml` path for Debian, Ubuntu, and Proxmox DKMS validation
- a reusable `.github/actions/rpm-dkms-build/action.yml` path for Fedora, Silverblue, RHEL / CentOS Stream, and Amazon Linux jobs
- broader Debian-family and RPM-family validation coverage around `ipcverify` and packaged outputs

### Changed

- Amazon Linux DKMS builds now run in containers on GitHub-hosted runners
- supported Amazon Linux kernel targets are selected explicitly in CI
- RPM-family workflows now share a standardized DKMS bootstrap and artifact/log collection path

### Fixed

- Amazon Linux workflow bootstrap issues
- Amazon Linux Binder and DKMS compatibility backfills
- enterprise-distro DKMS bootstrapping and shell-quoting problems
- host-specific Android SDK/JDK path assumptions in `ipcverify/build-android.sh`
- non-actionable RPM header/repo mismatches and CentOS Stream 8 soft-fail handling

## [2.2.2] - 2026-04-07

### Fixed

- skipped unprepared common header trees during release and CI packaging
- rebuilt the release payload from downloaded artifacts so `.run` bundles are assembled from the correct staged outputs

## [2.2.0] - 2026-04-07

### Added

- semver-driven CI release validation and runtime verification
- self-extracting `redroid-modules-standalone-X.Y.Z.run` packaging
- structured binderfs / Binder / Ashmem environment detection

### Changed

- release automation now stages per-kernel `KERNEL`, `VERSION`, and `SHA256SUMS`
- prerelease kernel rebuilds can refresh preview artifacts without forcing a new semantic release

## [2.1.6] - 2026-03-31

### Fixed

- compat build regressions and UBI workflow toolchain issues
- cross-distro CI build and runtime-check stabilization
- Proxmox and Fedora CI regressions
- binderfs and enterprise-kernel compatibility problems in CI
- Fedora modpost undefined-symbol skip handling and non-actionable mismatch detection

## [2.1.0] - 2026-03-31

### Added

- cross-compatibility updates for Binder and Ashmem across a wider kernel and distro spread
- centralized compatibility handling in `binder/compat.h` and `ashmem/compat.h`
- shrinker registration compatibility handling for newer and older kernel signatures in the standalone tree

### Changed

- binder and ashmem now route more kernel-version drift through explicit `compat.h` wrappers, including VM-flag and shrinker-registration glue
- out-of-tree module logic now relies on explicit compatibility wrappers rather than ad hoc per-file version branching

## [2.0.5] - 2026-03-31

### Fixed

- userspace analysis scope and header detection in CI
- checkout order, script permissions, and early standalone build-workflow issues
- compatibility threshold handling for shrinker, `task_work_add`, and LSM context backfills

## [2.0.0] - 2026-03-31

### Added

- initial standalone import based on the upstream `redroid-modules` line
- standalone packaging and the Proxmox-oriented baseline for `binder_linux` and `ashmem_linux`
- `ashmem_create_backing_file()` to abstract shmem vs memfd-compatible backing creation
- `ashmem_backing_mode` with a backward-compatible `0=shmem` default and optional `1=memfd-compatible` behavior
- ashmem debugfs observability via `/sys/kernel/debug/ashmem/regions` and `/sys/kernel/debug/ashmem/stats`

### Changed

- ashmem runtime instrumentation now exposes backing-mode and pin/unpin debug information while preserving the existing ioctl surface

---

Older upstream `redroid-modules` history is treated as the **1.x** line and is intentionally excluded from this standalone changelog.
