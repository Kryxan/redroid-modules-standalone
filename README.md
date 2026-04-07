# Android IPC Kernel Modules

Linux kernel module project for Android IPC on host kernels that do not ship usable Binder/Ashmem modules for containerized Android workloads.

This repository builds and packages:

- `binder_linux` (Binder IPC + binderfs support)
- `ashmem_linux` (legacy Android shared memory compatibility)

Target use cases include ReDroid and Waydroid host containers (LXC or Docker), especially on modern distro and Proxmox kernels. VMs are unnecessary, not required.

## Release Installer (`.run`)

Releases now publish a self-extracting installer that behaves like a lightweight `.run` package.

It embeds:

- `prebuilt/<kernel>/binder_linux.ko`
- `prebuilt/<kernel>/ashmem_linux.ko`
- the bundled DKMS source trees from `binder/` and `ashmem/`
- `load_modules.sh`
- `scripts/verify-environment.sh`
- `test/test_ipc`

Installer flow:

1. Detect the running kernel with `uname -r`
2. Install matching prebuilt `.ko` files when available
3. Fall back to DKMS rebuild when no exact prebuilt match exists
4. Load `ashmem_linux` and `binder_linux`
5. Mount `binderfs` at `/dev/binderfs`
6. Run `test/test_ipc`
7. Write a full log to `/var/log/redroid-modules-install.log`

Usage:

```bash
chmod +x redroid-modules-standalone-<version>.run
sudo ./redroid-modules-standalone-<version>.run
```

Non-interactive install

```bash
sudo ./redroid-modules-standalone-<version>.run --silent --yes
```

The GitHub Actions release workflow builds per-kernel prebuilt modules for Debian and Proxmox targets on `ubuntu-latest`, uploads the `.run` installer as a release asset, and also publishes the raw per-kernel `.ko` files.

See [docs/RELEASE_BUNDLE_LAYOUT.md](docs/RELEASE_BUNDLE_LAYOUT.md) for the exact bundle layout.

## Project Status

- kernel API compatibility hardening for newer kernels
- stronger DKMS pre/post-build checks
- CI/CD workflows for build, analysis, and packaging
- runtime instrumentation for ashmem and binder allocator debugging
- userspace validation tests for binderfs, binder, and ashmem
