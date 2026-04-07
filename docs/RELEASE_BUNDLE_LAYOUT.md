# Release Bundle Layout

The self-extracting `.run` release expands into the following structure:

```text
redroid-modules-standalone/
├── install.sh
├── load_modules.sh
├── Makefile
├── README.md
├── RELEASE_MANIFEST.txt
├── binder/
│   ├── Makefile
│   ├── dkms.conf
│   ├── *.c
│   ├── *.h
│   ├── linux/
│   └── uapi/
├── ashmem/
│   ├── Makefile
│   ├── dkms.conf
│   ├── *.c
│   ├── *.h
│   ├── ion/
│   └── uapi/
├── scripts/
│   ├── detect-ipc-runtime.sh
│   └── verify-environment.sh
├── test/
│   ├── test_ipc
│   ├── test_ipc.c
│   ├── ashmem_test.c
│   └── test.c
└── prebuilt/
    ├── <kernel-release>/
    │   ├── binder_linux.ko
    │   ├── ashmem_linux.ko
    │   ├── test_ipc
    │   ├── KERNEL
    │   └── SHA256SUMS
    └── ...
```

Runtime behavior:

1. `install.sh` detects `uname -r`.
2. If `prebuilt/<kernel-release>/` exists, the matching `.ko` files are installed.
3. If no exact match exists, the bundled `binder/` and `ashmem/` DKMS trees are used.
4. `load_modules.sh` loads `binder_linux` and `ashmem_linux` and mounts `binderfs`.
5. `test/test_ipc` and `scripts/verify-environment.sh` validate the install.
