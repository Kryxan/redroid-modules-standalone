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

## Distro-Specific Setup

### Fedora (latest)

```bash
sudo dnf install -y make gcc clang kmod dkms kernel-devel-$(uname -r)
```

On Fedora, `kernel-devel-$(uname -r)` is preferred because it matches the running
kernel ABI. If that exact package is unavailable, use `kernel-devel` and build
against the selected headers via `KERNEL_SRC`.

### Fedora Silverblue

Silverblue is immutable. Use one of these approaches:

1. Layer headers on host:

```bash
sudo rpm-ostree install kernel-devel
sudo systemctl reboot
```

1. Build inside toolbox:

```bash
toolbox create
toolbox enter
sudo dnf install -y make gcc clang kmod kernel-devel
```

### RHEL / CentOS Stream (8, 9)

```bash
sudo yum install -y kernel-devel-$(uname -r)
```

On RHEL-compatible systems, header packages may trail the running kernel.
When exact `kernel-devel-$(uname -r)` is unavailable, install generic
`kernel-devel` and point `KERNEL_SRC` to a valid headers tree.

### Proxmox VE 8 and 9

Workflows include explicit Proxmox repository setup and `pve-headers`
installation paths for:

1. Proxmox 8 (bookworm)
2. Proxmox 9 (trixie)

## Runtime Verification

The repository includes a unified runtime detector for binderfs, binder devices,
binder ioctl support, and ashmem mmap behavior.

```bash
# JSON output
bash scripts/detect-ipc-runtime.sh

# key=value output
bash scripts/detect-ipc-runtime.sh --format keyvalue

# fail on missing runtime capabilities
bash scripts/detect-ipc-runtime.sh --strict --format keyvalue
```

The helper below is used by `make verify` and CI:

```bash
bash scripts/verify-environment.sh --format keyvalue
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
make verify             # runtime environment detection (binderfs/binder/ashmem)
make ci-test            # CI runtime checks without install side effects
make -C test all        # build all userspace tests
sudo make -C test run-all
```

`make ci-test` intentionally does not install or remove modules. It validates the
current runtime environment via `scripts/verify-environment.sh`.

Binder module replacement behavior varies by kernel. On some kernels (including
recent Proxmox variants), `binder_linux` may not unload cleanly even with refcount
0. In those cases, updates are staged on disk and activated on reboot.

## Kernel Header and Signing Quirks

1. Fedora / Silverblue: kernel-devel packages are tightly coupled to kernel ABI.
2. RHEL / CentOS Stream: exact running-kernel headers can be unavailable in some repos.
3. Proxmox: use `pve-headers` from Proxmox repositories; Debian generic headers are insufficient.
4. Secure Boot systems may require module signing enrollment (MOK) before modules can load.

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
