Third-Party Licensing and Attribution

Recommended layout for derivative-but-independent repositories:

1. Root LICENSE

- Place the primary project license text at repository root.
- For this project, keep GPL-2.0 to comply with derivative obligations.

1. Root NOTICE

- Include high-level attribution and provenance.
- Clarify that the project is independent and significantly modified.

1. LICENSES/ directory

- Store third-party attribution details and any additional notices.
- Add per-component notices if future dependencies introduce extra obligations.

Current attribution summary:

- Base lineage: remote-android/redroid-modules
- Upstream interfaces: Linux Binder/BinderFS and Ashmem-related kernel APIs

If additional external code is imported later, add corresponding notices here and
update NOTICE.
