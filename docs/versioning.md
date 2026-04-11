# Versioning Standard

This repository uses **Semantic Versioning** with a single source of truth at the repo root:

```text
VERSION
```

The file must contain only:

```text
X.Y.Z
```

No prefixes, suffixes, or extra text are allowed.

## Core Inclusion Rule

> **Any commit that changes behavior, adds features, fixes bugs, changes installer logic, updates kernel support, alters packaging output, modifies `ipcverify`, or changes CI/workflows in a way that affects shipped output must appear in the changelog and affect versioning.**

This rule is based on **impact**, not on commit-message prefixes.

### Included changes

- feature commits
- bug-fix commits
- installer logic changes
- kernel compatibility and support updates
- CI/workflow changes that affect packaged or validated output
- `ipcverify` additions and behavior changes
- packaging changes
- documentation changes that affect operator-visible behavior or release/install rules

### Excluded changes

- pure formatting
- comment-only edits
- README-only wording changes with no behavior impact
- file creation/deletion that does not affect functionality

## Semantic Versioning Rules

| Part | Increment when | Examples |
| --- | --- | --- |
| `MAJOR` | A deliberate breaking redesign changes the public behavior contract in an incompatible way | incompatible installer redesign, dropped compatibility contract, intentional runtime break |
| `MINOR` | New features, new kernels, new distro support, new installer capabilities, or new packaging/verification functionality are added without breaking existing flows | Amazon Linux support, new release packaging, new `ipcverify` capability |
| `PATCH` | Fixes, corrections, small adjustments, or workflow/packaging changes that preserve the public behavior contract | compat fixes, workflow fixes, DKMS corrections, output-affecting CI fixes |

Repository policy remains **MAJOR = 2** unless a real breaking redesign is intentionally introduced.

## Changelog and Version-Bump Logic

1. Start from the previous semantic tag `vX.Y.Z`.
2. If no tags exist yet, use the standalone-project baseline at `2.0.0` (`eb21159`).
3. Scan all commits since that boundary.
4. Exclude only the non-behavioral categories listed above.
5. Classify every remaining commit as `MINOR` or `PATCH`.
6. If the interval contains any `MINOR`-class change, the next release is the next `MINOR`.
7. Otherwise, increment `PATCH` once per included behavior-changing commit.

### Patch-series consolidation

Versioning should **count each included patch commit**, but the changelog may **consolidate a consecutive patch run** under the resulting release heading for readability.

Example:

- `2.1.0` + six behavior-changing patch commits → changelog can summarize that run as **`2.1.6`**
- the six fixes are still counted individually for versioning purposes

## Historical Reconstruction for This Repo

The current checked-in version is:

```text
2.3.0
```

The standalone-project history currently reconstructs as:

| Version | Date | Reason |
| --- | --- | --- |
| `2.0.0` | `2026-03-31` | initial standalone import and baseline packaging |
| `2.0.5` | `2026-03-31` | early CI/build/compatibility fixes after the standalone import |
| `2.1.0` | `2026-03-31` | cross-compatibility updates across kernels and distros |
| `2.1.6` | `2026-03-31` | six follow-up patch fixes for CI/runtime/compat stability |
| `2.2.0` | `2026-04-07` | semver CI, runtime validation, and `.run` packaging improvements |
| `2.2.2` | `2026-04-07` | two follow-up release/payload patch fixes |
| `2.3.0` | `2026-04-10` | Amazon Linux and broader RPM-family workflow/compat expansion |

## Single Source of Truth and Automation

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

## Release Tag Rules

When tags are created, they must be exactly:

```text
vX.Y.Z
```

Examples:

- `v2.0.0`
- `v2.1.0`
- `v2.1.6`

The release workflow reads `VERSION` first and fails if the pushed tag does not match the file.

## Validation Expectations

Every versioned change should remain compatible with the existing validation path:

- `make ci-check`
- `make verify`
- `make ci-test`
- release matrix builds
- kernel-triggered preview builds

Compatibility-only kernel or workflow fixes should remain `PATCH` unless they intentionally add new supported behavior and justify a `MINOR` release.
