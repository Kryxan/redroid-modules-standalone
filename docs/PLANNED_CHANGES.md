# Planned Changes

This file is **future-only**. Items that are already implemented belong in `docs/design.md`.

## Installer Behavior and Policy

1. Keep the `.run` installer strictly **prebuilt-first** and continue auditing for any path that could bypass workflow-built modules.
2. Decide whether a maintainer-facing “force DKMS rebuild” mode is needed, or whether the current `prebuilt first / fallback only when needed` rule should stay absolute.
3. Make the installer’s decision path even more explicit in logs and release notes so users can tell when prebuilt assets were used versus DKMS fallback.

## `ipcverify` Integration Follow-Up

1. Expand the structured reporting around `ipcverify-host` so support bundles capture a more uniform machine-readable summary.
2. Clarify how optional Android-side verification results should feed into release notes and CI annotations.
3. Decide whether `ipcverify` should continue to share the repo version forever or eventually publish its own compatibility/reporting schema version.

## Kernel Header Resolution and Compatibility

1. Keep reducing distro-specific header resolution differences between Debian-family and RPM-family workflows.
2. Continue documenting and testing edge cases for Proxmox kernels, rpm-ostree hosts, and Amazon Linux 2 vs 2023.

## Ashmem Memory Modernization Backlog

Already implemented and documented in `docs/design.md`:

- `ashmem_create_backing_file()`
- `ashmem_backing_mode`
- debugfs observability for backing mode and region/stats output
- VM-flag wrapper usage
- shrinker registration wrappers

### Phase M1 — Memfd semantics and safety

1. Add explicit memfd-mode behavior tests for pin/unpin and purge semantics.
2. Add compatibility helpers for seal operations where needed.
3. Add stricter assertions around backing-mode transitions and edge cases.

### Phase M2 — Reclaim behavior

1. Add reclaim stress tests aimed at the memfd-backed path.
2. Add shrinker-activity debugfs stats/counters beyond the current high-level region totals.
3. Add reclaim regression validation to the regular verification path.

### Phase M3 — Controlled extension

1. Evaluate seal-ioctl support behind guardrails rather than enabling it by default.
2. Keep the backward-compatible ashmem default as the normal shipped behavior.
3. Add CI gating specifically for any future non-default ashmem semantics.

## CI Workflow Standardization

1. Preserve `.github/actions/rpm-dkms-build/action.yml` as the authoritative RPM-family path and extend the same standardization to Debian-family workflows where it helps.
2. Keep expanding the validation matrix only when stable public headers are reproducibly available.
3. Standardize artifact naming, detector output capture, and summary formatting across all distro workflows.

## Module Packaging and Release Metadata

1. Keep release bundles centered on workflow-built prebuilt modules, bundled DKMS sources, and deterministic manifests.
2. Add stronger install-time validation for generated `SHA256SUMS` and `RELEASE_MANIFEST.txt`.
3. Publish a clearer machine-readable supported-kernel manifest alongside release assets.

## Versioning and Changelog Governance

1. Enforce the `MAJOR == 2` policy automatically until a true breaking redesign exists.
2. Add stronger automation around deciding when a change should be a `MINOR` versus a `PATCH`.
3. Generate `CHANGELOG.md` by scanning all commits since the previous version boundary and keeping every behavior-changing commit, regardless of prefix.
4. Exclude only formatting-only, comment-only, README-only, or otherwise behavior-neutral commits.
5. Allow the changelog to consolidate consecutive patch runs for readability while still counting each included patch commit for versioning.

## Longer-Term Feature Ideas

1. Extend `ipcverify` stress coverage for reclaim, memfd-backed ashmem behavior, and Android runtime edge cases.
2. Improve Secure Boot guidance and optional signing helpers for external modules.
3. Revisit GPU normalization / shim work only after the kernel-module and validation baseline stays stable.

## Branch and Process Note

Planning remains a backlog, not a promise.
