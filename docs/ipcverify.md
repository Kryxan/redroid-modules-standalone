# `ipcverify`

> `ipcverify` (Linux + Android) is an idempotent verification tool used by the installer to validate binder/ashmem functionality. It can also run independently of the kernel modules.

## Components

| Component | Purpose |
| --- | --- |
| `bin/ipcverify-host` | standalone Linux host verifier for binderfs, Binder, and Ashmem checks |
| `bin/ipcverify.apk` | optional Android-side verifier launched through `adb` |

## Installer Integration

The `.run` installer installs `ipcverify-host` and, unless `--skip-test` is used, runs:

```bash
ipcverify-host --local-only --yes
```

That gives the normal install path an immediate post-install runtime check.

## Standalone Usage

### Host-only verification

```bash
./bin/ipcverify-host --local-only --yes
```

### Host + Android verification through `adb`

```bash
./bin/ipcverify-host --verify-android --yes
```

Useful flags exposed by the current CLI include:

- `--yes`
- `--verify-android`
- `--local-only` / `--no-android`
- `--adb-host <host>`
- `--adb-port <port>`
- `--apk <path>`

## What It Checks

### Linux host checks

The host verifier reports whether the environment can actually support Android IPC workloads, including:

- `binderfs` support and mountability
- Binder device-node presence and ioctl behavior
- Ashmem mapping behavior
- fallback expectations when the host is using memfd-backed semantics instead of legacy ashmem behavior

### Optional Android checks

When `--verify-android` is enabled and `adb` is available, `ipcverify-host` can:

- connect to the target Android container/device
- install or reuse `ipcverify.apk`
- launch the Android verifier activity
- capture relevant `IPCVerify` logcat output

## Output and Exit Codes

The current host report uses a simple status model:

- `PASS`
- `FAIL`
- `SKIP`
- `NOT_PRESENT`

It ends with a summary line similar to:

```text
summary: <passed> passed, <failed> failed, <skipped> skipped, <not_present> not present
```

Exit codes:

| Code | Meaning |
| --- | --- |
| `0` | verification succeeded |
| `1` | runtime verification failed or a required Android step could not be completed |
| `2` | invalid CLI usage |

## Support Bundle

For deeper diagnostics, use:

```bash
make support-bundle
```

This generates `ipcverify-support-<date-time>.tar.gz` with detector output, module information, and recent verification logs.
