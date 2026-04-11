.DEFAULT_GOAL := all

PASS_THROUGH_TARGETS := install modules_install load reload uninstall clean-modules dkms-install info
SUBDIRS := ashmem binder
MODULE_VERSION := $(shell cat VERSION)
VERSION := $(MODULE_VERSION)
OUTPUT_DIR ?= $(CURDIR)/bin
PREBUILT_ROOT ?= $(OUTPUT_DIR)/prebuilt
RELEASE_BUNDLE_SCRIPT := ./scripts/build-release-bundle.sh
IPCVERIFY_DIR := $(CURDIR)/ipcverify

all: validate-version modules ipcverify-host ipcverify-android stage-bin package-prebuilt release-bundle

modules:
	$(MAKE) -C ashmem all
	$(MAKE) -C binder all

$(PASS_THROUGH_TARGETS):
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done
	@$(MAKE) -C ipcverify clean >/dev/null 2>&1 || true
	@rm -rf "$(OUTPUT_DIR)"

# --- CI targets (top-level only, not propagated to module subdirs) ---

ci-build:
	$(MAKE) modules
	$(MAKE) -C ipcverify host OUTPUT_DIR="$(OUTPUT_DIR)"

# Runtime environment verification for binderfs/binder/ashmem.
verify:
	@bash ./scripts/verify-environment.sh --format keyvalue

# CI runtime checks without install side effects.
ci-test:
	$(MAKE) -C ipcverify host OUTPUT_DIR="$(OUTPUT_DIR)"
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

stage-bin: modules ipcverify-host
	@mkdir -p "$(OUTPUT_DIR)"
	@cp -f binder/binder_linux.ko "$(OUTPUT_DIR)/binder_linux.ko"
	@cp -f ashmem/ashmem_linux.ko "$(OUTPUT_DIR)/ashmem_linux.ko"
	@cp -f ipcverify/build/ipcverify-host "$(OUTPUT_DIR)/ipcverify-host"
	@echo "Staged Linux artifacts in $(OUTPUT_DIR)"

# Package the currently built artifacts into a per-kernel prebuilt layout.
package-prebuilt: validate-version stage-bin
	@KVER="$$(uname -r)"; \
	mkdir -p "$(PREBUILT_ROOT)/$$KVER"; \
	cp -f "$(OUTPUT_DIR)/binder_linux.ko" "$(PREBUILT_ROOT)/$$KVER/binder_linux.ko"; \
	cp -f "$(OUTPUT_DIR)/ashmem_linux.ko" "$(PREBUILT_ROOT)/$$KVER/ashmem_linux.ko"; \
	cp -f "$(OUTPUT_DIR)/ipcverify-host" "$(PREBUILT_ROOT)/$$KVER/ipcverify-host"; \
	cp -f VERSION "$(PREBUILT_ROOT)/$$KVER/VERSION"; \
	printf '%s\n' "$$KVER" > "$(PREBUILT_ROOT)/$$KVER/KERNEL"; \
	if [ -f "$(OUTPUT_DIR)/ipcverify.apk" ]; then \
		cp -f "$(OUTPUT_DIR)/ipcverify.apk" "$(PREBUILT_ROOT)/$$KVER/ipcverify.apk"; \
	fi; \
	rm -f "$(PREBUILT_ROOT)/$$KVER/SHA256SUMS"; \
	( cd "$(PREBUILT_ROOT)/$$KVER" && \
	  for artifact in binder_linux.ko ashmem_linux.ko ipcverify-host VERSION KERNEL ipcverify.apk; do \
		[ -f "$$artifact" ] && sha256sum "$$artifact"; \
	  done > SHA256SUMS ); \
	echo "Packaged prebuilt artifacts in $(PREBUILT_ROOT)/$$KVER"

# Build the self-extracting .run installer from bin/prebuilt or a provided PREBUILT_ROOT.
release-bundle: validate-version package-prebuilt
	@test -f "$(RELEASE_BUNDLE_SCRIPT)" || { echo "Missing $(RELEASE_BUNDLE_SCRIPT)"; exit 1; }
	@chmod +x "$(RELEASE_BUNDLE_SCRIPT)"
	"$(RELEASE_BUNDLE_SCRIPT)" --version "$(VERSION)" --prebuilt-root "$(PREBUILT_ROOT)" --output-dir "$(OUTPUT_DIR)"

release-layout:
	@if [ -f "$(OUTPUT_DIR)/release-layout.txt" ]; then \
		cat "$(OUTPUT_DIR)/release-layout.txt"; \
	else \
		echo "No generated layout found. Run 'make release-bundle' first."; \
	fi

ipcverify-host:
	$(MAKE) -C ipcverify host OUTPUT_DIR="$(OUTPUT_DIR)"

ipcverify-android:
	$(MAKE) -C ipcverify android OUTPUT_DIR="$(OUTPUT_DIR)"

ipcverify: ipcverify-host ipcverify-android

support-bundle:
	@chmod +x ./scripts/create-support-bundle.sh
	@./scripts/create-support-bundle.sh

.PHONY: $(PASS_THROUGH_TARGETS) $(SUBDIRS) all modules clean ci-build ci-test ci-check verify validate-version stage-bin package-prebuilt release-bundle release-layout ipcverify-host ipcverify-android ipcverify support-bundle
