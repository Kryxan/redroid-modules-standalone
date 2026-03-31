# Android IPC Kernel Modules (Standalone)

Standalone Linux kernel module project for Android IPC on host kernels that do not ship
usable Binder/Ashmem modules for containerized Android workloads.

This repository builds and packages:

- `binder_linux` (Binder IPC + binderfs support)
- `ashmem_linux` (legacy Android shared memory compatibility)

Target use cases include ReDroid and Waydroid hosts, especially on modern distro and
Proxmox kernels.

## Project Status

This is a new, independent repository with a clean Git history.

Compared to the upstream base, this project includes significant additional work:

- kernel API compatibility hardening for newer kernels
- stronger DKMS pre/post-build checks
- CI/CD workflows for build, analysis, and packaging
- runtime instrumentation for ashmem and binder allocator debugging
- userspace validation tests for binderfs, binder, and ashmem

## Based On Work By

This project is based on prior work from the original `redroid-modules` maintainers and
contributors, plus upstream Linux Android IPC interfaces.

- Original codebase: `remote-android/redroid-modules`
- Upstream kernel interfaces: Linux Binder/BinderFS and Ashmem-related APIs

See [NOTICE](NOTICE) for attribution and provenance details.

## Quick Start

1. Install build dependencies and kernel headers.
2. Build modules.
3. Install and load modules.
4. Run userspace validation tests.

```bash
sudo apt-get update
sudo apt-get install -y build-essential kmod dkms linux-headers-$(uname -r)

make
sudo make install
sudo ./load_modules.sh

make -C test run-all
```

## Repository Layout

```text
ashmem/                 ashmem module sources + dkms config
binder/                 binder module sources + dkms config
test/                   userspace validation tests
	ashmem_test.c         focused ashmem test suite
	test.c                binderfs device creation test
	test_ipc.c            combined binderfs+binder+ashmem validation
deploy/                 k8s deployment templates
docs/                   compatibility and planning docs
.github/workflows/      CI/CD pipelines
```

## Build and Test

```bash
make                    # build kernel modules
make ci-check           # compat and guard checks
make -C test all        # build all userspace tests
sudo make -C test run-all
```

The combined test program `test/test_ipc.c` validates:

- binderfs mount and device behavior
- binder ioctls and buffer mapping
- ashmem ioctls, mapping, pin/unpin, and sharing behavior
- relevant sysfs/debugfs runtime visibility

## DKMS Packaging

Use GitHub Actions (`package.yml`) for artifact builds, or package manually with scripts in
`.github/scripts/`.

## License

This derivative project remains GPL-2.0 licensed. See:

- [LICENSE](LICENSE)
- [NOTICE](NOTICE)

## Disclaimer

This repository is not the canonical upstream project. It is an independently maintained
derivative focused on compatibility, validation, and release engineering.
