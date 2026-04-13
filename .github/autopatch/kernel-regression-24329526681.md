## Kernel compatibility autopatch request

- Repository version: `2.3.0`
- Generated at: `2026-04-13T06:44:30Z`
- Target branch: `prerelease`

This request was generated automatically after a kernel-triggered rebuild failed.
Please keep the semantic project version unchanged and stage any compatibility fix on
`prerelease` first.

### Guardrails

1. Preserve the planned compatibility scaffolding documented in `docs/PLANNED_CHANGES.md`.
2. Do not remove `ashmem_backing_mode`, related debug hooks, or runtime tests.
3. Limit the fix to kernel compatibility shims, CI adjustments, or safe build/runtime changes.
4. Re-run `make ci-check`, `make ci-test`, and the failing kernel matrix entry after patching.

### Compiler errors

```text
No compiler diagnostics were extracted from artifacts```
