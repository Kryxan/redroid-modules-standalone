# build

If you have a custom kernel, not yet targetted kernel, or wish to build the modules yourself.

Extract the packaged modules and source code

```bash
# Extract only, without installing
./redroid-modules-standalone-<version>.run --extract-only --target /redroid-release
```

Install build dependencies and kernel headers.

```bash
sudo apt-get update
sudo apt-get install -y build-essential kmod dkms linux-headers-$(uname -r)
```

Build, Install, and load modules.

```bash
make
sudo make install
sudo ./load_modules.sh
```

Run userspace validation tests.

```bash
make -C test run-all
```

## Distro-Specific Setup

### Fedora (latest)

```bash
sudo dnf install -y make gcc clang kmod dkms kernel-devel-$(uname -r)
```

On Fedora, `kernel-devel-$(uname -r)` is preferred because it matches the running kernel ABI. If that exact package is unavailable, use `kernel-devel` and build against the selected headers via `KERNEL_SRC`.

### Fedora Silverblue

Silverblue is immutable. Use one of these approaches:

Layer headers on host:

```bash
sudo rpm-ostree install kernel-devel
sudo systemctl reboot
```

Build inside toolbox:

```bash
toolbox create
toolbox enter
sudo dnf install -y make gcc clang kmod kernel-devel
```

### RHEL / CentOS Stream (8, 9)

```bash
sudo yum install -y kernel-devel-$(uname -r)
```

On RHEL-compatible systems, header packages may trail the running kernel. When exact `kernel-devel-$(uname -r)` is unavailable, install generic `kernel-devel` and point `KERNEL_SRC` to a valid headers tree.

### Proxmox VE 8 and 9

Workflows include explicit Proxmox repository setup and `pve-headers`
installation paths for:

1. Proxmox 8 (bookworm)
2. Proxmox 9 (trixie)

## Runtime Verification

The repository includes a unified runtime detector for binderfs, binder devices, binder ioctl support, and ashmem mmap behavior.

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

## Build and Test

```bash
make                    # build kernel modules
make ci-check           # compat and guard checks
make verify             # runtime environment detection (binderfs/binder/ashmem)
make ci-test            # CI runtime checks without install side effects
make -C test all        # build all userspace tests
sudo make -C test run-all
```

`make ci-test` intentionally does not install or remove modules. It validates the current runtime environment via `scripts/verify-environment.sh`.

Binder module replacement behavior varies by kernel. On some kernels (including recent Proxmox variants), `binder_linux` may not unload cleanly even with refcount 0. In those cases, updates are staged on disk and activated on reboot.

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
