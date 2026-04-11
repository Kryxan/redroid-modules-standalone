# Android IPC Kernel Modules

Out-of-tree `binder_linux` and `ashmem_linux` modules for ReDroid / Waydroid style Android container hosts, with release packaging, DKMS fallback, and `ipcverify` validation.

## Supported Prebuilt Kernel Modules

| Distro | Kernel track used for prebuilt assets | Module availability |
| --- | --- | --- |
| Ubuntu 24.04 | `linux-headers-generic` | ✅ bundled in the release `.run` and GitHub release assets |
| Debian 12 amd64 | `linux-headers-amd64` | ✅ bundled in the release `.run` and GitHub release assets |
| Debian 13 (trixie) amd64 | `linux-headers-amd64` | ✅ bundled in the release `.run` and GitHub release assets |
| Proxmox VE 8 | `pve-headers` | ✅ bundled in the release `.run` and GitHub release assets |
| Proxmox VE 9 | `pve-headers` | ✅ bundled in the release `.run` and GitHub release assets |

RPM-family CI (Fedora, Silverblue, RHEL/CentOS Stream, and Amazon Linux 2/2023) currently validates DKMS builds on GitHub even when those families are not all emitted as prebuilt release assets.

## `.run` Installer

The self-extracting installer ships workflow-built prebuilt modules under `prebuilt/<kernel-release>/`, the bundled `binder/` and `ashmem/` DKMS trees, `load_modules.sh`, and verification helpers. It detects `uname -r`, installs matching prebuilt modules first, and falls back to DKMS only when no exact prebuilt module exists; use `--no-dkms` to disable that fallback.

```bash
sudo ./redroid-modules-standalone-<version>.run
```

Use `--extract-only --target <dir>` when you want to inspect the bundle without installing it.

## `ipcverify`

> `ipcverify` (Linux + Android) is an idempotent verification tool used by the installer to validate binder/ashmem functionality. It can also run independently of the kernel modules.

See `docs/BUILD.md`, `docs/KERNEL_COMPAT.md`, `docs/release_bundle.md`, `docs/ipcverify.md`, `docs/versioning.md`, and `docs/design.md` for the detailed behavior.
