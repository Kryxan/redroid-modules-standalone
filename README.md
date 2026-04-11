# Android IPC Kernel Modules

Out-of-tree `binder_linux` and `ashmem_linux` modules for ReDroid / Waydroid style Android container hosts, with release packaging, DKMS fallback, and `ipcverify` validation.

## Supported Prebuilt Kernel Modules

| Distro | Kernel track used for prebuilt assets | Module availability |
| --- | --- | --- |
| Ubuntu 24.04 | `linux-headers-generic` | ✅ bundled inside the release `.run` installer |
| Debian 12 amd64 | `linux-headers-amd64` | ✅ bundled inside the release `.run` installer |
| Debian 13 (trixie) amd64 | `linux-headers-amd64` | ✅ bundled inside the release `.run` installer |
| Proxmox VE 8 | `pve-headers` | ✅ bundled inside the release `.run` installer |
| Proxmox VE 9 | `pve-headers` | ✅ bundled inside the release `.run` installer |

RPM-family CI now validates DKMS builds across Fedora, Silverblue, RHEL/CentOS Stream, AlmaLinux, Rocky, CloudLinux, Amazon Linux 2/2023, openEuler, and Alibaba Cloud Linux-compatible Anolis targets even when the loose release assets stay limited to the installer and `ipcverify` tools.

## `.run` Installer

The self-extracting installer is the **canonical installation mechanism**. It ships workflow-built prebuilt modules under `prebuilt/<kernel-release>/`, the bundled `binder/` and `ashmem/` DKMS trees, `load_modules.sh`, and both `ipcverify` helpers. It verifies the embedded bundle manifest at install time, detects `uname -r`, installs matching prebuilt modules first, and falls back to DKMS only when no exact prebuilt module exists and the host can provide usable headers/build tools; use `--no-dkms` to disable that fallback.

> On rpm-ostree / immutable hosts such as Fedora Silverblue, current-boot module installation is supported, but persistent build-tool availability may still require package layering for DKMS-based rebuilds.

```bash
sudo ./redroid-modules-standalone-<version>.run
```

Use `--extract-only --target <dir>` when you want to inspect the bundle without installing it.

## `ipcverify`

> `ipcverify` (Linux + Android) is an idempotent verification tool used by the installer to validate binder/ashmem functionality. It can also run independently of the kernel modules.

See `docs/BUILD.md`, `docs/KERNEL_COMPAT.md`, `docs/release_bundle.md`, `docs/ipcverify.md`, `docs/versioning.md`, and `docs/design.md` for the detailed behavior.
