# Versioning Standard

This repository uses **Semantic Versioning** with a single source of truth at the repo root:

```text
VERSION
```

The file contains only:

```text
X.Y.Z
```

No prefixes, suffixes, or extra text are allowed.

## Semantic Versioning Rules

| Part | Increment when | Examples |
| --- | --- | --- |
| `MAJOR` | A kernel-facing behavior changes in a breaking way, a compatibility contract is intentionally dropped, or an installation/runtime expectation changes incompatibly | dropping old behavior, changing ioctl behavior, incompatible module-loading rules |
| `MINOR` | New features, broader distro/kernel support, new installer capabilities, or newly supported kernels are added without breaking existing flows | adding support for a new Proxmox/Debian kernel family, new runtime checks, new packaging features |
| `PATCH` | Bug fixes, CI fixes, packaging fixes, or compatibility shims that do not change the public behavior contract | compiler fixups, workflow fixes, small DKMS/install corrections |

## Current Baseline

The standalone project baseline starts at:

```text
2.0.0
```

## Single Source of Truth

All release tooling reads the version from `VERSION`:

- top-level `Makefile`
- `binder/Makefile`
- `ashmem/Makefile`
- DKMS packaging helpers
- `.run` bundle generation
- GitHub Actions release and prerelease workflows

Helper scripts:

- `scripts/read-version.sh` — validates and prints the repo version
- `scripts/validate-version.sh` — verifies that a tag matches `VERSION`
- `scripts/bump-version.sh` — increments `major`, `minor`, or `patch`

## DKMS and Module Packaging Rules

1. The checked-in `dkms.conf` files are synchronized from `VERSION` during local DKMS install, CI packaging, and release bundle generation.
2. `make dkms-install` synchronizes `PACKAGE_VERSION` from `VERSION` before calling DKMS.
3. DKMS installs into:

```text
/lib/modules/<kernel>/updates/dkms/
```

instead of `extra/`, so updates are placed in the expected DKMS-managed path.

## Release Tag Rules

Release tags must be exactly:

```text
vX.Y.Z
```

Examples:

- `v2.0.0`
- `v2.1.0`
- `v2.1.3`

The release workflow reads `VERSION` first and fails if the pushed tag does not match the file.

## Release Workflow Behavior

The release workflow is triggered by:

1. pushing a semantic tag `vX.Y.Z`
2. running `workflow_dispatch`

On each release run, CI:

1. reads and validates `VERSION`
2. validates that the git tag matches `VERSION`
3. builds `binder_linux` and `ashmem_linux` for the kernel matrix
4. stages per-kernel prebuilt bundles
5. builds `redroid-modules-standalone-X.Y.Z.run`
6. uploads artifacts
7. publishes a GitHub Release with generated release notes and per-kernel assets

## Version Bump Workflow

The `version-bump.yml` workflow provides controlled semantic bumps.

Inputs:

- `major`
- `minor`
- `patch`

The workflow:

1. reads `VERSION`
2. increments the selected field
3. commits the updated `VERSION`
4. creates tag `vX.Y.Z`
5. pushes the commit and tag so the release workflow runs automatically

## Kernel-Triggered Rebuilds and Prereleases

New header discovery does **not** change semantic project versioning.

When new kernel headers appear:

1. the kernel rebuild workflow detects newly available headers
2. it builds only the affected matrix targets from the latest `prerelease` branch if present, otherwise from the latest release tag
3. successful preview artifacts are published to a prerelease channel
4. `VERSION` is left unchanged
5. if the build fails, an autopatch request is staged on `prerelease` with captured compiler errors for Copilot-assisted follow-up

## `.run` Installer Version Embedding

The installer generator reads `VERSION` and uses it for:

- the installer filename: `redroid-modules-standalone-X.Y.Z.run`
- the extracted bundle directory name
- the copied `VERSION` file inside the release bundle
- the generated `RELEASE_MANIFEST.txt`
- the DKMS metadata bundled inside the installer

## Validation Expectations

Every versioned change should remain compatible with the existing validation path:

- `make ci-check`
- `make verify`
- `make ci-test`
- release matrix builds
- kernel-triggered preview builds

Compatibility-only kernel fixes should usually stay on the same semantic version unless they intentionally add new supported kernels or change behavior enough to justify a `MINOR` or `PATCH` increment.
