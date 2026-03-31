ccflags-y += -I$(src)
obj-m := ashmem_linux.o
ashmem_linux-y := ashmem.o deps.o

MODULE_NAME := ashmem_linux
MODULE_FILE := $(MODULE_NAME).ko
DKMS_PACKAGE_NAME := redroid-ashmem
DKMS_PACKAGE_VERSION := 2.0.0
RUNNING_KERNEL := $(shell uname -r)
KERNEL_RELEASE ?= $(if $(KERNEL_SRC),$(notdir $(patsubst %/build,%,$(KERNEL_SRC))),$(RUNNING_KERNEL))
KDIR ?= $(if $(KERNEL_SRC),$(KERNEL_SRC),/lib/modules/$(KERNEL_RELEASE)/build)
INSTALL_DIR ?= /lib/modules/$(KERNEL_RELEASE)/extra
DEPMOD ?= depmod
MODPROBE ?= modprobe
RMMOD ?= rmmod
INSTALL ?= install
RM ?= rm -f

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

install: all install-files install-note

install-files:
	$(INSTALL) -D -m 644 $(MODULE_FILE) $(INSTALL_DIR)/$(MODULE_FILE)
	$(DEPMOD) -a $(KERNEL_RELEASE)
	@echo "Installed $(MODULE_FILE) to $(INSTALL_DIR)"

install-note:
	@if [ "$(KERNEL_RELEASE)" != "$(RUNNING_KERNEL)" ]; then \
		echo "Installed for kernel $(KERNEL_RELEASE). Running kernel is $(RUNNING_KERNEL); skipping live-module checks."; \
	elif grep -q '^$(MODULE_NAME) ' /proc/modules; then \
		echo "NOTICE: $(MODULE_NAME) is already loaded."; \
		echo "Run 'make reload' to activate the updated module without rebooting."; \
	else \
		echo "Module is not currently loaded. Use 'make load' or 'modprobe $(MODULE_NAME)' when ready."; \
	fi

modules_install: install

load:
	@if [ "$(KERNEL_RELEASE)" != "$(RUNNING_KERNEL)" ]; then \
		echo "Refusing to load $(MODULE_NAME): built for $(KERNEL_RELEASE) but running kernel is $(RUNNING_KERNEL)."; \
		exit 1; \
	fi
	$(MODPROBE) $(MODULE_NAME)

reload: install
	@if [ "$(KERNEL_RELEASE)" != "$(RUNNING_KERNEL)" ]; then \
		echo "Refusing to reload $(MODULE_NAME): built for $(KERNEL_RELEASE) but running kernel is $(RUNNING_KERNEL)."; \
		exit 1; \
	fi
	@if grep -q '^$(MODULE_NAME) ' /proc/modules; then \
		if $(MODPROBE) -r $(MODULE_NAME) 2>/dev/null || $(RMMOD) $(MODULE_NAME) 2>/dev/null; then \
			echo "Unloaded $(MODULE_NAME)"; \
		else \
			echo "WARNING: could not unload $(MODULE_NAME); module is busy."; \
			exit 1; \
		fi; \
	fi
	$(MODPROBE) $(MODULE_NAME)

uninstall:
	@if [ "$(KERNEL_RELEASE)" = "$(RUNNING_KERNEL)" ] && grep -q '^$(MODULE_NAME) ' /proc/modules; then \
		if $(MODPROBE) -r $(MODULE_NAME) 2>/dev/null || $(RMMOD) $(MODULE_NAME) 2>/dev/null; then \
			echo "Unloaded $(MODULE_NAME)"; \
		else \
			echo "WARNING: could not unload $(MODULE_NAME); continuing with on-disk uninstall only."; \
		fi; \
	fi
	$(RM) $(INSTALL_DIR)/$(MODULE_FILE)
	$(DEPMOD) -a $(KERNEL_RELEASE)
	@echo "Removed $(MODULE_FILE) from $(INSTALL_DIR)"

clean-modules:
	$(RM) $(INSTALL_DIR)/$(MODULE_FILE)
	$(DEPMOD) -a $(KERNEL_RELEASE)
	@echo "Removed stale $(MODULE_FILE) from $(INSTALL_DIR)"

dkms-install:
	dkms add $(CURDIR) >/dev/null 2>&1 || true
	dkms build -m $(DKMS_PACKAGE_NAME) -v $(DKMS_PACKAGE_VERSION) -k $(KERNEL_RELEASE)
	dkms install -m $(DKMS_PACKAGE_NAME) -v $(DKMS_PACKAGE_VERSION) -k $(KERNEL_RELEASE) --force

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	$(RM) -r .tmp_versions Module.symvers modules.order *.o.* *.ko *.mod *.mod.c *.symvers

verify:
	@echo "=== $(MODULE_NAME) verify ==="
	@echo "Kernel release:  $(KERNEL_RELEASE)"
	@echo "Running kernel:  $(RUNNING_KERNEL)"
	@echo "Kernel headers:  $(KDIR)"
	@if [ -d "$(KDIR)" ]; then echo "Headers:         found"; else echo "Headers:         MISSING"; fi
	@if [ -f "$(INSTALL_DIR)/$(MODULE_FILE)" ]; then \
		echo "Installed:       $(INSTALL_DIR)/$(MODULE_FILE)"; \
		modinfo $(INSTALL_DIR)/$(MODULE_FILE) 2>/dev/null | grep -E '^(vermagic|srcversion)' || true; \
	else \
		echo "Installed:       not found"; \
	fi
	@if grep -q '^$(MODULE_NAME) ' /proc/modules 2>/dev/null; then \
		echo "Loaded:          yes"; \
	else \
		echo "Loaded:          no"; \
	fi

info: verify

.PHONY: all install install-files install-note modules_install load reload uninstall clean clean-modules dkms-install verify info

