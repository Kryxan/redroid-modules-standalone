.DEFAULT_GOAL := all

MODULE_TARGETS := all install modules_install load reload uninstall clean clean-modules dkms-install info
SUBDIRS := ashmem binder
MODULE_VERSION := $(shell cat VERSION)
VERSION := $(MODULE_VERSION)
PREBUILT_ROOT ?= $(CURDIR)/dist/prebuilt
OUTPUT_DIR ?= $(CURDIR)/dist
RELEASE_BUNDLE_SCRIPT := ./scripts/build-release-bundle.sh

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

validate-version:
	@test -f VERSION || { echo "Missing VERSION file"; exit 1; }
	@printf '%s\n' "$(MODULE_VERSION)" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$$' || { echo "VERSION must be formatted as X.Y.Z"; exit 1; }
	@echo "Semantic version: $(MODULE_VERSION)"

# Package the currently built .ko files into a per-kernel prebuilt layout.
package-prebuilt: validate-version
	$(MAKE) -C test all
	@KVER="$$(uname -r)"; \
	mkdir -p "$(PREBUILT_ROOT)/$$KVER"; \
	cp -f binder/binder_linux.ko "$(PREBUILT_ROOT)/$$KVER/binder_linux.ko"; \
	cp -f ashmem/ashmem_linux.ko "$(PREBUILT_ROOT)/$$KVER/ashmem_linux.ko"; \
	cp -f test/test_ipc "$(PREBUILT_ROOT)/$$KVER/test_ipc"; \
	cp -f VERSION "$(PREBUILT_ROOT)/$$KVER/VERSION"; \
	printf '%s\n' "$$KVER" > "$(PREBUILT_ROOT)/$$KVER/KERNEL"; \
	echo "Packaged prebuilt modules in $(PREBUILT_ROOT)/$$KVER"

# Build the self-extracting .run installer from dist/prebuilt or a provided PREBUILT_ROOT.
release-bundle: validate-version
	@test -f "$(RELEASE_BUNDLE_SCRIPT)" || { echo "Missing $(RELEASE_BUNDLE_SCRIPT)"; exit 1; }
	@chmod +x "$(RELEASE_BUNDLE_SCRIPT)"
	"$(RELEASE_BUNDLE_SCRIPT)" --version "$(VERSION)" --prebuilt-root "$(PREBUILT_ROOT)" --output-dir "$(OUTPUT_DIR)"

release-layout:
	@if [ -f "$(OUTPUT_DIR)/release-layout.txt" ]; then \
		cat "$(OUTPUT_DIR)/release-layout.txt"; \
	else \
		echo "No generated layout found. Run 'make release-bundle' first."; \
	fi

.PHONY: $(MODULE_TARGETS) $(SUBDIRS) ci-build ci-test ci-check verify validate-version package-prebuilt release-bundle release-layout
