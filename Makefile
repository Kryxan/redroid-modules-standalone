.DEFAULT_GOAL := all

MODULE_TARGETS := all install modules_install load reload uninstall clean clean-modules dkms-install info
SUBDIRS := ashmem binder

$(MODULE_TARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

# --- CI targets (top-level only, not propagated to subdirs) ---

# Build both modules and the userspace test suite.
ci-build:
	$(MAKE) -C ashmem all
	$(MAKE) -C binder all
	$(MAKE) -C test all

# Runtime environment verification for binderfs/binder/ashmem.
verify:
	@bash ./scripts/verify-environment.sh --format keyvalue

# CI runtime tests without module installation side effects.
ci-test:
	$(MAKE) -C test all
	@bash ./scripts/verify-environment.sh --strict --format keyvalue

# Quick sanity: ensure compat macros are consumed and no stray version guards.
ci-check:
	@if [ -x .github/scripts/check-compat.sh ]; then \
		.github/scripts/check-compat.sh; \
	else \
		echo "check-compat.sh not found; skipping"; \
	fi
	@echo "Checking for stray LINUX_VERSION_CODE in functional files..."
	@FOUND=0; \
	for f in binder/binder.c binder/binderfs.c binder/binder_alloc.c \
	         binder/binder_alloc.h binder/binder_internal.h \
	         ashmem/ashmem.c; do \
		if grep -n 'LINUX_VERSION_CODE' "$$f" 2>/dev/null; then \
			echo "ERROR: version guard in $$f"; FOUND=1; \
		fi; \
	done; \
	if [ "$$FOUND" -eq 0 ]; then echo "OK: no stray version guards."; fi

.PHONY: $(MODULE_TARGETS) $(SUBDIRS) ci-build ci-test ci-check verify
