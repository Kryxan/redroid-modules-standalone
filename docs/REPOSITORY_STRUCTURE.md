# Standalone Repository Structure

This repository is intentionally organized as a clean derivative project:

```text
.github/
  workflows/
    build.yml            multi-distro DKMS module build validation
    dkms-package.yml     packaging (deb + tar.gz) artifacts
    analyze.yml          sparse + clang-tidy + compat checks
  scripts/
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
