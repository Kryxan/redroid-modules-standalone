# Build Guide

## Quick Start

### Build the modules from the repo

```bash
make modules
sudo make install
sudo ./load_modules.sh
```

### Build the full release-style bundle

```bash
make all
```

That stages the following under `bin/`:

- `binder_linux.ko`
- `ashmem_linux.ko`
- `ipcverify-host`
- `ipcverify.apk`
- `redroid-modules-standalone-<version>.run`

### Extract a release without installing it

```bash
./redroid-modules-standalone-<version>.run --extract-only --target /tmp/redroid-release
```

## DKMS Fallback Builds

The repository and the extracted `.run` bundle both support DKMS fallback when no exact prebuilt module exists.

### From the repository

```bash
sudo apt-get update
sudo apt-get install -y dkms make gcc kmod linux-headers-$(uname -r)
sudo make dkms-install
sudo ./load_modules.sh
```

`make dkms-install` keeps `PACKAGE_VERSION` synchronized with the root `VERSION` file before it calls DKMS.

### From an extracted `.run` bundle

```bash
./redroid-modules-standalone-<version>.run --extract-only --target /tmp/redroid-release
sudo sh /tmp/redroid-release/redroid-modules-standalone/install.sh --yes
```

Behavior:

1. Matching `prebuilt/<uname -r>/` modules are preferred.
2. Bundled DKMS sources are used only when no exact prebuilt match exists.
3. `--no-dkms` refuses fallback.
4. `--skip-test` skips the post-install `ipcverify-host` run.

## Build `ipcverify`

```bash
make ipcverify-host
make ipcverify-android
./bin/ipcverify-host --local-only --yes
./bin/ipcverify-host --verify-android --yes
```

`make ipcverify-android` requires Java 17, Gradle 8.x, and `ANDROID_SDK_ROOT` configured with Android 34 / build-tools 34.

## Distro-Specific Header Setup

| Distro family | Recommended header package path | Notes |
| --- | --- | --- |
| Ubuntu / Debian | `linux-headers-$(uname -r)` or distro generic headers | Release CI currently builds prebuilt assets for Ubuntu 24.04, Debian 12, and Debian 13 |
| Proxmox VE 8 / 9 | `pve-headers` (or `proxmox-headers`) | Use Proxmox repos; Debian generic headers are not enough |
| Fedora | `kernel-devel-$(uname -r)` first, then `kernel-devel` | Exact ABI match is preferred |
| Fedora Silverblue | `rpm-ostree install kernel-devel` or build in toolbox | Immutable hosts need live layering or toolbox |
| RHEL / CentOS Stream | `kernel-devel-$(uname -r)` first, then generic `kernel-devel` | Enterprise repos can lag the running kernel |
| Amazon Linux 2 | `kernel-devel-$(uname -r)` first, then generic packages | Older 4.14-era quirks are handled in compat code |
| Amazon Linux 2023 | prefer the `6.1.*` kernel-devel/header set when exact running-kernel headers are missing | Current CI explicitly handles this branch |

## Validate the Runtime

```bash
make ci-check
make verify
make ci-test
bash scripts/detect-ipc-runtime.sh --format json
bash scripts/detect-ipc-runtime.sh --strict --format keyvalue
```

`make ci-test` intentionally avoids install/remove side effects. It uses `scripts/verify-environment.sh`, while `ipcverify-host` is the standalone idempotent verifier.

## Support Bundle

```bash
make support-bundle
```

This produces an `ipcverify-support-<date-time>.tar.gz` archive with runtime detector output, module information, and recent verification logs.
