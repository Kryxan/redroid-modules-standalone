# Standalone Repository Structure

This repository is intentionally organized as a clean derivative project:

```text
.github/
  workflows/
    build.yml            Ubuntu/Debian/Proxmox matrix build validation
    build-fedora.yml     Fedora latest build + runtime validation
    build-silverblue.yml Fedora Silverblue toolbox-oriented build validation
    build-rhel-centos.yml RHEL/CentOS Stream 8/9 build validation
    dkms-package.yml     packaging (deb + tar.gz) artifacts
    analyze.yml          sparse + clang-tidy + compat checks
    kernel-check.yml     scheduled kernel update + matrix test build
  scripts/
scripts/
  detect-ipc-runtime.sh unified binderfs/binder/ashmem runtime detector
  verify-environment.sh strict runtime verification wrapper
ashmem/
binder/
deploy/
docs/
  KERNEL_COMPAT.md
  PLANNED_CHANGES.md
  REPOSITORY_STRUCTURE.md
test/
LICENSE
NOTICE
LICENSES/
  THIRD_PARTY.md
README.md
.gitignore
Makefile
```

## Local reference assets

The repository intentionally excludes local kernel header copies used for
development reference (for example `linux-headers-6.17.13-2-pve`). Keep those
outside version control.

## Why this layout

- Keeps kernel sources (`ashmem/`, `binder/`) separate and DKMS-ready.
- Keeps userspace validation tests in a dedicated `test/` folder.
- Keeps legal/provenance material explicit (`LICENSE`, `NOTICE`, `LICENSES/`).
- Keeps automation in `.github/workflows/` and reusable scripts under `.github/scripts/`.

## Creating a clean standalone repo from an existing codebase

```bash
# from source repo root
mkdir ../new-standalone-repo
rsync -a --exclude .git --exclude linux-headers-* ./ ../new-standalone-repo/
cd ../new-standalone-repo

git init
git add .
git commit -m "Initial standalone import"
git branch -M main
git remote add origin <new-repo-url>
git push -u origin main
```

This preserves source content while dropping old commit history.
